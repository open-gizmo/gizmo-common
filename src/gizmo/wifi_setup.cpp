#include "wifi_setup.h"

// Wi-Fi setup, only available on Arduino targets (native test builds skip it)
#ifdef ARDUINO

#include <ArduinoJson.h>
#include <DNSServer.h>
#include <ESPmDNS.h>
#include <Preferences.h>
#include <WiFi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <vector>

#include "http.h"
#include "log.h"

/*
The main loop never waits inside a Wi-Fi retry loop.
Instead it starts a connection attempt, polls its progress across later loop iterations,
and keeps the setup hotspot available while the device has no working network.
*/

namespace {
	constexpr uint16_t DNS_PORT = 53; // DNS port for the captive portal
	constexpr unsigned long WIFI_CONNECT_TIMEOUT = 12500;
	constexpr unsigned long WIFI_RETRY_INTERVAL = 10000;
	constexpr unsigned long ACCESS_POINT_SHUTDOWN_DELAY = 120000; // Keep the hotspot for 2 minutes after reconnecting (so a phone in the setup flow sees the result)
	constexpr int WIFI_SCAN_FAILED_STATUS = -2;

	// NVS namespace and keys of the saved network (names are limited to 15 characters)
	constexpr char STORAGE_NAMESPACE[] = "gizmo-wifi";
	constexpr char SSID_KEY[] = "ssid";
	constexpr char PASSWORD_KEY[] = "password";

	/******************************** Shared state ********************************/

	// Project settings
	gizmo::WifiSetupOptions setupOptions = { "Gizmo", "gizmo", "/", nullptr };

	// Status shared with the web server task
	volatile gizmo::WifiStatus wifiStatus = gizmo::WifiStatus::Disconnected;
	volatile bool accessPointEnabled = false;
	volatile bool isAccessPointShutdownRequested = false;

	// Connection state (used by the main loop only)
	struct WifiRuntimeState {
		bool mdnsActive = false;
		bool connectInProgress = false;
		bool isAttemptWithNewCredentials = false; // The current attempt uses credentials that are not saved yet
		bool isWaitingForOldLinkToDrop = false; // The attempt started while connected, and the old link is still up
		bool isConnected = false;
		unsigned long connectedSince = 0;
		unsigned long connectStartedAt = 0;
		unsigned long reconnectDelayStartedAt = 0; // When the wait before the next attempt started
		unsigned long reconnectDelay = 0; // How long to wait before the next attempt (0 = try now)
	};

	// Create the connection state instance
	WifiRuntimeState wifiState;

	// A network name and password
	struct WifiCredentials {
		char ssid[gizmo::WIFI_SSID_SIZE] = "";
		char password[gizmo::WIFI_PASSWORD_SIZE] = "";
	};

	// Saved credentials (RAM copy of NVS) and new ones from the web server, kept apart until they work
	WifiCredentials savedCredentials;
	WifiCredentials pendingCredentials;
	bool hasPendingCredentials = false;
	bool isAttemptRequested = false;
	portMUX_TYPE credentialsMux = portMUX_INITIALIZER_UNLOCKED;

	// DNS server that answers every name with the hotspot address (created on first use, so projects that don't use the Wi-Fi setup don't pay for it)
	DNSServer *dnsServer = nullptr;

	/******************************** Credentials ********************************/

	// Copy a string into a fixed-size field, cutting it off safely if it is too long
	void copyText(char *destination, const char *text, size_t length) {
		if (length == 0) return;
		snprintf(destination, length, "%s", text == nullptr ? "" : text);
	}

	// Load the saved network from NVS into the RAM copy
	void loadSavedCredentials() {
		Preferences preferences;
		WifiCredentials credentials;

		// Read the saved network, if any
		if (preferences.begin(STORAGE_NAMESPACE, true)) {
			preferences.getString(SSID_KEY, credentials.ssid, sizeof(credentials.ssid));
			preferences.getString(PASSWORD_KEY, credentials.password, sizeof(credentials.password));
			preferences.end();
		}

		// Update the RAM copy
		portENTER_CRITICAL(&credentialsMux);
		savedCredentials = credentials;
		portEXIT_CRITICAL(&credentialsMux);
	}

	// Save the credentials that just worked to NVS and to the RAM copy
	void saveWorkingCredentials(const WifiCredentials &credentials) {
		Preferences preferences;

		// Write the network to NVS
		const bool isOpen = preferences.begin(STORAGE_NAMESPACE, false);
		const bool isSaved = isOpen && preferences.putString(SSID_KEY, credentials.ssid) > 0 && preferences.putString(PASSWORD_KEY, credentials.password) == strlen(credentials.password);
		if (isOpen) preferences.end();
		if (!isSaved) gizmo::printLogln("Failed to save the new WiFi credentials");

		// Update the RAM copy (even if the flash write failed, so the device keeps using the working network)
		portENTER_CRITICAL(&credentialsMux);
		savedCredentials = credentials;
		portEXIT_CRITICAL(&credentialsMux);
	}

	// Copy the credentials to try next: the pending ones if any, otherwise the saved ones
	bool readCredentialsToTry(WifiCredentials &credentials) {
		portENTER_CRITICAL(&credentialsMux);
		const bool isNew = hasPendingCredentials;
		credentials = isNew ? pendingCredentials : savedCredentials;
		portEXIT_CRITICAL(&credentialsMux);
		return isNew;
	}

	// Copy the pending credentials, if any
	bool readPendingCredentials(WifiCredentials &credentials) {
		portENTER_CRITICAL(&credentialsMux);
		const bool hasCredentials = hasPendingCredentials;
		credentials = pendingCredentials;
		portEXIT_CRITICAL(&credentialsMux);
		return hasCredentials;
	}

	// Forget the pending credentials
	void clearPendingCredentials() {
		portENTER_CRITICAL(&credentialsMux);
		pendingCredentials = WifiCredentials();
		hasPendingCredentials = false;
		portEXIT_CRITICAL(&credentialsMux);
	}

	// Check and clear a connection request from the web server
	bool takeConnectionRequest() {
		portENTER_CRITICAL(&credentialsMux);
		const bool isRequested = isAttemptRequested;
		isAttemptRequested = false;
		portEXIT_CRITICAL(&credentialsMux);
		return isRequested;
	}

	// Check whether a network is saved
	bool hasSavedCredentials() {
		portENTER_CRITICAL(&credentialsMux);
		const bool hasSsid = savedCredentials.ssid[0] != '\0';
		portEXIT_CRITICAL(&credentialsMux);
		return hasSsid;
	}

	// Queue a connection attempt with new credentials (called from the web server task)
	void queueConnectionAttempt(const char *ssid, const char *password) {
		// Hand the new credentials over to the main loop
		portENTER_CRITICAL(&credentialsMux);
		copyText(pendingCredentials.ssid, ssid, sizeof(pendingCredentials.ssid));
		copyText(pendingCredentials.password, password, sizeof(pendingCredentials.password));
		hasPendingCredentials = true;
		isAttemptRequested = true;
		portEXIT_CRITICAL(&credentialsMux);

		// Report the attempt as in progress right away
		wifiStatus = gizmo::WifiStatus::Connecting;
	}

	/******************************** mDNS and hotspot ********************************/

	// Stop mDNS service if active
	void stopMdns() {
		if (!wifiState.mdnsActive) return;
		MDNS.end();
		wifiState.mdnsActive = false;
		gizmo::printLogln("mDNS stopped");
	}

	// Start mDNS service
	void startMdns() {
		// Don't start mDNS if not connected to Wi-Fi
		if (WiFi.status() != WL_CONNECTED) {
			stopMdns();
			return;
		}

		// Don't start mDNS if it's already active
		if (wifiState.mdnsActive) {
			return;
		}

		// Start mDNS with the configured hostname
		if (!MDNS.begin(setupOptions.hostname)) {
			gizmo::printLogfln("Failed to start mDNS on http://%s.local", setupOptions.hostname);
			return;
		}

		// Advertise the HTTP service on mDNS
		MDNS.addService("http", "tcp", 80);
		wifiState.mdnsActive = true;
		gizmo::printLogfln("mDNS ready on http://%s.local", setupOptions.hostname);
	}

	// Turn the setup hotspot on, with a DNS server for the captive portal
	bool startAccessPoint() {
		stopMdns();

		// If the hotspot is already on, nothing to do
		if (accessPointEnabled) {
			return true;
		}

		// Start the open hotspot
		accessPointEnabled = WiFi.softAP(setupOptions.accessPointName);
		if (!accessPointEnabled) {
			gizmo::printLogfln("Failed to start access point with SSID: %s", setupOptions.accessPointName);
			return false;
		}

		// Answer every DNS name with the hotspot address, so phones open the setup page
		if (dnsServer == nullptr) dnsServer = new DNSServer();
		dnsServer->start(DNS_PORT, "*", WiFi.softAPIP());
		gizmo::printLogfln("Access point ready on http://%s", WiFi.softAPIP().toString().c_str());
		return true;
	}

	// Turn the setup hotspot off when requested
	void manageAccessPointShutdown() {
		if (!isAccessPointShutdownRequested || !accessPointEnabled) return;

		// Stop the DNS server and the hotspot
		if (dnsServer != nullptr) dnsServer->stop();
		accessPointEnabled = !WiFi.softAPdisconnect();
		if (!accessPointEnabled) {
			isAccessPointShutdownRequested = false;
		}
	}

	/******************************** Connection attempts ********************************/

	// Clear the current connection attempt state
	void resetConnectionAttempt() {
		wifiState.connectInProgress = false;
		wifiState.connectStartedAt = 0;
	}

	// Start a connection attempt with the pending credentials, or the saved ones
	bool beginConnectionAttempt() {
		WifiCredentials credentials;
		const bool hasNewCredentials = readCredentialsToTry(credentials);

		// If the SSID is empty, we cannot start a connection attempt
		if (credentials.ssid[0] == '\0') {
			return false;
		}

		// Start the connection attempt and mark it as in progress (disconnecting is asynchronous, so the old link can linger)
		wifiState.isWaitingForOldLinkToDrop = WiFi.status() == WL_CONNECTED;
		WiFi.disconnect();
		WiFi.begin(credentials.ssid, credentials.password);
		wifiState.connectInProgress = true;
		wifiState.isAttemptWithNewCredentials = hasNewCredentials;
		wifiState.connectStartedAt = millis();
		gizmo::printLogfln("Connecting to WiFi SSID: %s", credentials.ssid);
		return true;
	}

	// Mark the Wi-Fi as connected, save new credentials, and schedule the hotspot shutdown
	void completeConnectionSuccess() {
		const bool wasAttemptInProgress = wifiState.connectInProgress;
		resetConnectionAttempt();
		wifiState.reconnectDelay = 0;
		wifiStatus = gizmo::WifiStatus::Connected;

		// If we weren't already connected, print the success message and save the new credentials if they were used
		if (wasAttemptInProgress) {
			gizmo::printLogln("WiFi connected!");
			WifiCredentials credentials;
			if (wifiState.isAttemptWithNewCredentials && readPendingCredentials(credentials)) {
				saveWorkingCredentials(credentials);
				clearPendingCredentials();
			}
			wifiState.isAttemptWithNewCredentials = false;
		}

		// Remember when the connection came back
		if (!wifiState.isConnected) {
			wifiState.isConnected = true;
			wifiState.connectedSince = millis();
		}

		// Turn the setup hotspot off once Wi-Fi has been back for a while
		if (accessPointEnabled && !isAccessPointShutdownRequested && millis() - wifiState.connectedSince >= ACCESS_POINT_SHUTDOWN_DELAY) {
			gizmo::printLogln("WiFi is back, turning the access point off");
			isAccessPointShutdownRequested = true;
		}

		// Start mDNS service
		startMdns();
	}

	// Mark the current attempt as failed and schedule a future retry
	void completeConnectionFailure() {
		// Drop new credentials that didn't work (the next attempt goes back to the saved network)
		if (wifiState.isAttemptWithNewCredentials) {
			gizmo::printLogln("The new WiFi credentials didn't work, falling back to the saved ones");
			clearPendingCredentials();
			wifiState.isAttemptWithNewCredentials = false;
		}

		// Mark the attempt as failed
		WiFi.disconnect();
		resetConnectionAttempt();
		wifiStatus = gizmo::WifiStatus::Disconnected;
		wifiState.reconnectDelayStartedAt = millis();
		wifiState.reconnectDelay = WIFI_RETRY_INTERVAL;
		gizmo::printLogfln("Failed to connect to WiFi, retrying in %lu ms", WIFI_RETRY_INTERVAL);
	}

	// Check whether a fresh connection attempt should start on this loop iteration
	bool shouldStartConnectionAttempt(unsigned long currentMillis) {
		return wifiStatus == gizmo::WifiStatus::Connecting || currentMillis - wifiState.reconnectDelayStartedAt >= wifiState.reconnectDelay; // Elapsed-time check stays correct across millis() rollover
	}

	/******************************** Network scan ********************************/

	// One network found by a scan
	struct ScannedNetwork {
		String ssid;
		int32_t rssi;
		uint8_t channel;
		bool secured;
		uint8_t quality;
	};

	// Cache for scanned networks and state tracking (cache and scan flag are guarded by networksMutex)
	std::vector<ScannedNetwork> cachedNetworks;
	SemaphoreHandle_t networksMutex = nullptr;
	bool isScanCallbackRegistered = false;
	bool isScanInProgress = false;

	// Calculate signal quality percentage from RSSI
	uint8_t calculateSignalQuality(int32_t rssi) {
		if (rssi <= -100) return 0;
		if (rssi >= -50) return 100;
		return static_cast<uint8_t>(2 * (rssi + 100));
	}

	// Read the current scan results from the Wi-Fi stack
	std::vector<ScannedNetwork> collectScannedNetworks(int networkCount) {
		std::vector<ScannedNetwork> scannedNetworks;
		scannedNetworks.reserve(networkCount);

		// Collect information for each scanned network
		for (int index = 0; index < networkCount; index++) {
			const String ssid = WiFi.SSID(index);
			if (ssid.length() == 0) {
				continue;
			}

			// Add the network
			ScannedNetwork network;
			network.ssid = ssid;
			network.rssi = WiFi.RSSI(index);
			network.channel = WiFi.channel(index);
			network.secured = WiFi.encryptionType(index) != WIFI_AUTH_OPEN;
			network.quality = calculateSignalQuality(network.rssi);
			scannedNetworks.push_back(network);
		}

		// Clear the scan results from the Wi-Fi stack to free memory, and return the networks
		WiFi.scanDelete();
		return scannedNetworks;
	}

	// Register the scan done callback and create the cache mutex once
	bool ensureScanCallbackRegistered() {
		if (isScanCallbackRegistered) {
			return true;
		}

		// Create the mutex that guards the cache (written by the Wi-Fi event task, read by the web server task)
		networksMutex = xSemaphoreCreateMutex();
		if (networksMutex == nullptr) {
			return false;
		}

		// Read the results when a scan completes
		WiFi.onEvent([](arduino_event_id_t event, arduino_event_info_t info) {
			// Read the scan results outside the lock (an empty list if the scan failed)
			const int networkCount = WiFi.scanComplete();
			std::vector<ScannedNetwork> scannedNetworks;
			if (networkCount < 0) {
				WiFi.scanDelete();
			} else {
				scannedNetworks = collectScannedNetworks(networkCount);
			}

			// Swap the results into the cache and mark the scan as complete
			xSemaphoreTake(networksMutex, portMAX_DELAY);
			cachedNetworks.swap(scannedNetworks);
			isScanInProgress = false;
			xSemaphoreGive(networksMutex);
		}, ARDUINO_EVENT_WIFI_SCAN_DONE);

		// Mark the callback as registered to avoid registering it multiple times
		isScanCallbackRegistered = true;
		return true;
	}

	// Start an asynchronous Wi-Fi scan to avoid blocking the web server task
	bool startAsyncScan() {
		if (!ensureScanCallbackRegistered()) {
			return false;
		}

		// Mark the scan as in progress before starting it (so a fast scan done event can't be overwritten)
		xSemaphoreTake(networksMutex, portMAX_DELAY);
		const bool isScanAlreadyRunning = isScanInProgress;
		isScanInProgress = true;
		xSemaphoreGive(networksMutex);

		// If a scan is already running, keep serving the cached results
		if (isScanAlreadyRunning) {
			return true;
		}

		// Start a new asynchronous scan, and clear the flag if it fails to start
		WiFi.scanDelete();
		if (WiFi.scanNetworks(true, true) == WIFI_SCAN_FAILED_STATUS) {
			xSemaphoreTake(networksMutex, portMAX_DELAY);
			isScanInProgress = false;
			xSemaphoreGive(networksMutex);
			return false;
		}

		// Scan started
		return true;
	}

	// Build the JSON payload from the cached networks while holding the cache lock
	JsonDocument buildCachedNetworksData() {
		JsonDocument doc;
		JsonArray array = doc.to<JsonObject>()["networks"].to<JsonArray>();

		// Add each network to the JSON array
		xSemaphoreTake(networksMutex, portMAX_DELAY);
		for (const ScannedNetwork &network : cachedNetworks) {
			JsonObject item = array.add<JsonObject>();
			item["ssid"] = network.ssid;
			item["signal"] = network.rssi;
			item["quality"] = network.quality;
			item["channel"] = network.channel;
			item["secured"] = network.secured;
		}
		xSemaphoreGive(networksMutex);

		// Return the JSON document
		return doc;
	}

	/******************************** Web API ********************************/

	// Validate one credential field, sending an error response if it's invalid
	bool validateCredentialField(AsyncWebServerRequest *request, const char *fieldName, JsonVariantConst value, size_t maxLength, bool allowEmpty) {
		// Field must be present
		if (value.isNull()) {
			gizmo::printLogfln("Missing required %s value", fieldName);
			gizmo::sendErrorResponse(request, 400, "missing_required_fields", fieldName);
			return false;
		}

		// Field must be a string
		if (!value.is<const char *>()) {
			gizmo::printLogfln("Invalid %s type", fieldName);
			gizmo::sendErrorResponse(request, 400, "invalid_connection_field", fieldName);
			return false;
		}

		// Field must not be empty unless allowed
		const char *text = value.as<const char *>();
		if (!allowEmpty && text[0] == '\0') {
			gizmo::printLogfln("Missing required %s value", fieldName);
			gizmo::sendErrorResponse(request, 400, "missing_required_fields", fieldName);
			return false;
		}

		// Field must fit
		if (strlen(text) > maxLength) {
			gizmo::printLogfln("%s is too long: %u", fieldName, static_cast<unsigned>(strlen(text)));
			gizmo::sendErrorResponse(request, 400, "invalid_connection_field", fieldName);
			return false;
		}

		// Valid field
		return true;
	}

	// Register GET /api/networks: start a scan and return the networks found so far
	void setupNetworksRoute(AsyncWebServer &server) {
		server.on("/api/networks", HTTP_GET, [](AsyncWebServerRequest *request) {
			if (!gizmo::ensureCorsAllowed(request)) return;

			// Start a new scan if not already in progress
			if (!startAsyncScan()) {
				gizmo::sendErrorResponse(request, 500, "wifi_scan_failed", "Unable to scan Wi-Fi networks.");
				return;
			}

			// Return the current scan results
			JsonDocument doc = buildCachedNetworksData();
			gizmo::sendSuccessResponse(request, 200, &doc);
		});
	}

	// Register POST /api/connect: try new credentials (saved only once they work)
	void setupConnectRoute(AsyncWebServer &server) {
		server.on("/api/connect", HTTP_POST, [](AsyncWebServerRequest *request) {}, nullptr, [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
			// Read the JSON body
			JsonDocument doc;
			if (!gizmo::readJsonBody(request, data, len, index, total, doc)) return;

			// Validate the credential fields (the password can be empty for open networks)
			if (!validateCredentialField(request, "ssid", doc["ssid"], gizmo::WIFI_SSID_SIZE - 1, false)
				|| !validateCredentialField(request, "password", doc["password"], gizmo::WIFI_PASSWORD_SIZE - 1, true)) {
				return;
			}

			// Try the new credentials
			queueConnectionAttempt(doc["ssid"].as<const char *>(), doc["password"].as<const char *>());

			// Send the response with the updated connection status
			JsonDocument responseDoc;
			responseDoc["connectionStatus"] = static_cast<int>(wifiStatus);
			gizmo::sendSuccessResponse(request, 200, &responseDoc);
		});

		// OPTIONS request for preflighted requests
		server.on("/api/connect", HTTP_OPTIONS, [](AsyncWebServerRequest *request) {
			gizmo::sendCorsPreflightResponse(request, "POST, OPTIONS");
		});
	}

	// Register GET /api/check-connection: report the status, and close the hotspot once connected
	void setupCheckConnectionRoute(AsyncWebServer &server) {
		server.on("/api/check-connection", HTTP_GET, [](AsyncWebServerRequest *request) {
			if (!gizmo::ensureCorsAllowed(request)) return;

			// Add the connection status
			const gizmo::WifiStatus status = wifiStatus;
			JsonDocument doc;
			doc["connectionStatus"] = static_cast<int>(status);

			// If connected, add the network, address and hostname
			if (status == gizmo::WifiStatus::Connected && WiFi.status() == WL_CONNECTED) {
				doc["ssid"] = WiFi.SSID();
				doc["ip"] = WiFi.localIP().toString();
				doc["hostname"] = String(setupOptions.hostname) + ".local";
			}

			// Once connected, the setup page has its answer, so the hotspot can close
			if (status == gizmo::WifiStatus::Connected) {
				isAccessPointShutdownRequested = true;
			}

			// Send the JSON response
			gizmo::sendSuccessResponse(request, 200, &doc);
		});
	}
}

namespace gizmo {
	// Start the Wi-Fi setup and register its web API (/api/networks, /api/connect, /api/check-connection)
	void setupWifi(AsyncWebServer &server, const WifiSetupOptions &options) {
		setupOptions = options;

		// Station and hotspot together (the hotspot only starts when needed)
		WiFi.mode(WIFI_AP_STA);
		loadSavedCredentials();

		// Register the web API
		setupNetworksRoute(server);
		setupConnectRoute(server);
		setupCheckConnectionRoute(server);
	}

	// Connect, retry, and run the setup hotspot (call on every loop iteration)
	void manageWifi() {
		const unsigned long currentMillis = millis();

		// Answer DNS requests and close the hotspot when requested
		if (accessPointEnabled && dnsServer != nullptr) {
			dnsServer->processNextRequest();
		}
		manageAccessPointShutdown();

		// Start a connection attempt requested from the web server, even if already connected
		if (takeConnectionRequest()) {
			resetConnectionAttempt();
			wifiState.reconnectDelay = 0;
			wifiState.isConnected = false;
			stopMdns();
			beginConnectionAttempt();
			return;
		}

		// While an attempt waits for the previous link to drop, don't take the old connection for the new one
		if (wifiState.connectInProgress && wifiState.isWaitingForOldLinkToDrop && WiFi.status() != WL_CONNECTED) {
			wifiState.isWaitingForOldLinkToDrop = false;
		}

		// Check if connected to Wi-Fi
		if (WiFi.status() == WL_CONNECTED && !(wifiState.connectInProgress && wifiState.isWaitingForOldLinkToDrop)) {
			completeConnectionSuccess();
			return;
		}

		// Not connected
		wifiState.isConnected = false;
		stopMdns();

		// Poll the current connection attempt until it succeeds or times out
		if (wifiState.connectInProgress) {
			if (currentMillis - wifiState.connectStartedAt >= WIFI_CONNECT_TIMEOUT) {
				completeConnectionFailure();
				startAccessPoint();
			}
			return;
		}

		// Retry the saved network when it's time, keeping the hotspot available meanwhile
		if (hasSavedCredentials() && shouldStartConnectionAttempt(currentMillis) && beginConnectionAttempt()) {
			return;
		}
		startAccessPoint();
	}

	// Get the current connection status
	WifiStatus getWifiStatus() {
		return wifiStatus;
	}

	// Check whether the device is connected to Wi-Fi
	bool isWifiConnected() {
		return WiFi.status() == WL_CONNECTED;
	}

	// Check whether the setup hotspot is on
	bool isAccessPointActive() {
		return accessPointEnabled;
	}

	// Copy the saved network name, returning false if none is saved
	bool getSavedWifiSsid(char *ssid, size_t length) {
		portENTER_CRITICAL(&credentialsMux);
		copyText(ssid, savedCredentials.ssid, length);
		portEXIT_CRITICAL(&credentialsMux);
		return length > 0 && ssid[0] != '\0';
	}

	// Forget the saved network (safe to call from the web server task)
	bool clearWifiCredentials() {
		Preferences preferences;

		// Remove the saved network from NVS
		const bool isOpen = preferences.begin(STORAGE_NAMESPACE, false);
		const bool isCleared = isOpen && preferences.clear();
		if (isOpen) preferences.end();

		// Clear the RAM copy
		portENTER_CRITICAL(&credentialsMux);
		savedCredentials = WifiCredentials();
		portEXIT_CRITICAL(&credentialsMux);
		return isCleared;
	}

	// Redirect hotspot clients to the setup page (call first in the server's not-found handler; true if it answered)
	bool handleCaptivePortalRequest(AsyncWebServerRequest *request) {
		// Only page requests from clients connected through the hotspot (LAN clients keep the normal pages)
		if (!accessPointEnabled) return false;
		if (request->method() != HTTP_GET && request->method() != HTTP_HEAD) return false;
		if (request->client()->localIP() != WiFi.softAPIP()) return false;

		// Never redirect the setup page itself, its files, or the API
		const String path = request->url();
		if (path == setupOptions.setupPagePath || path.startsWith("/api/")) return false;
		if (setupOptions.assetPathPrefix != nullptr && path.startsWith(setupOptions.assetPathPrefix)) return false;

		// Send everything else to the setup page
		request->redirect(setupOptions.setupPagePath);
		return true;
	}
}

#endif