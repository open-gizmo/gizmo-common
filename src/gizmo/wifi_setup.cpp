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

	// Connection state (used by the main loop only): the connection itself is a state machine in wifi_connection.*, with its timeouts
	struct WifiRuntimeState {
		bool mdnsActive = false;
		gizmo::WifiConnection connection;
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
		if (length == 0) {
			return;
		}
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
		if (isOpen) {
			preferences.end();
		}
		if (!isSaved) {
			gizmo::printLogln("Failed to save the new Wi-Fi credentials");
		}

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

		// Report the attempt as in progress right away (under the lock, so the main loop can't report an older status meanwhile)
		wifiStatus = gizmo::WifiStatus::Connecting;
		portEXIT_CRITICAL(&credentialsMux);
	}

	// Check whether new credentials from the web app are waiting to be tried or saved
	bool hasNewCredentials() {
		portENTER_CRITICAL(&credentialsMux);
		const bool hasCredentials = hasPendingCredentials;
		portEXIT_CRITICAL(&credentialsMux);
		return hasCredentials;
	}

	// Publish the status for the web app: Connecting while a request waits for the main loop, otherwise the state of the connection
	void publishWifiStatus() {
		portENTER_CRITICAL(&credentialsMux);
		wifiStatus = isAttemptRequested ? gizmo::WifiStatus::Connecting : gizmo::getWifiConnectionStatus(wifiState.connection);
		portEXIT_CRITICAL(&credentialsMux);
	}

	/******************************** mDNS and hotspot ********************************/

	// Stop mDNS service if active
	void stopMdns() {
		if (!wifiState.mdnsActive) {
			return;
		}
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
		if (dnsServer == nullptr) {
			dnsServer = new DNSServer();
		}
		dnsServer->start(DNS_PORT, "*", WiFi.softAPIP());
		gizmo::printLogfln("Access point ready on http://%s", WiFi.softAPIP().toString().c_str());
		return true;
	}

	// Turn the setup hotspot off when requested
	void manageAccessPointShutdown() {
		if (!isAccessPointShutdownRequested || !accessPointEnabled) {
			return;
		}

		// Stop the DNS server and the hotspot
		if (dnsServer != nullptr) {
			dnsServer->stop();
		}
		accessPointEnabled = !WiFi.softAPdisconnect(true); // Switch the hotspot off: clearing its settings alone leaves it on, open and named ESP_XXXXXX
		if (!accessPointEnabled) {
			isAccessPointShutdownRequested = false;
		}
	}

	/******************************** Connection attempts ********************************/

	// Start a connection attempt with the pending credentials, or the saved ones (disconnecting is asynchronous, so the old link can linger: the state machine waits for it to drop)
	void beginConnectionAttempt() {
		WifiCredentials credentials;
		readCredentialsToTry(credentials);

		// If the SSID is empty, there is nothing to try (the attempt times out and the hotspot stays on)
		if (credentials.ssid[0] == '\0') {
			return;
		}
		WiFi.disconnect();
		WiFi.begin(credentials.ssid, credentials.password);
		gizmo::printLogfln("Connecting to Wi-Fi SSID: %s", credentials.ssid);
	}

	// Save the new credentials that just worked
	void saveNewCredentials() {
		WifiCredentials credentials;
		if (readPendingCredentials(credentials)) {
			saveWorkingCredentials(credentials);
			clearPendingCredentials();
		}
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
		if (rssi <= -100) {
			return 0;
		}
		if (rssi >= -50) {
			return 100;
		}
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
		},
			ARDUINO_EVENT_WIFI_SCAN_DONE);

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
			if (!gizmo::ensureCorsAllowed(request)) {
				return;
			}

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
			if (!gizmo::readJsonBody(request, data, len, index, total, doc)){ return;
}

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
			gizmo::sendSuccessResponse(request, 200, &responseDoc); });

		// OPTIONS request for preflighted requests
		server.on("/api/connect", HTTP_OPTIONS, [](AsyncWebServerRequest *request) {
			gizmo::sendCorsPreflightResponse(request, "POST, OPTIONS");
		});
	}

	// Register GET /api/check-connection: report the status, and close the hotspot once connected
	void setupCheckConnectionRoute(AsyncWebServer &server) {
		server.on("/api/check-connection", HTTP_GET, [](AsyncWebServerRequest *request) {
			if (!gizmo::ensureCorsAllowed(request)) {
				return;
			}

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

namespace {
	/******************************** Request guard ********************************/

	// API paths a client of the setup hotspot may use: the Wi-Fi setup, and the device details (anyone nearby can join the open hotspot, so nothing else answers there)
	const char *const HOTSPOT_API_PATHS[] = { "/api/networks", "/api/connect", "/api/check-connection", "/api/device" };

	// Whether a request came in through the setup hotspot
	bool isHotspotRequest(AsyncWebServerRequest *request) {
		return accessPointEnabled && request->client() != nullptr && request->client()->localIP() == WiFi.softAPIP();
	}

	// Whether the Host header names this device: its IP address, or its mDNS name (with or without .local), with any port
	bool isDeviceHost(String host) {
		host.toLowerCase();
		const int portStart = host.indexOf(':');
		if (portStart >= 0) {
			host = host.substring(0, portStart);
		}
		if (host.length() == 0) {
			return false;
		}
		if (host == WiFi.localIP().toString()) {
			return true;
		}
		String name = setupOptions.hostname != nullptr ? setupOptions.hostname : "";
		name.toLowerCase();
		return name.length() != 0 && (host == name || host == name + ".local");
	}

	// The check every API request passes besides its origin (see gizmo::setRequestGuard)
	const char *guardRequest(AsyncWebServerRequest *request) {
		// Through the hotspot: the setup only (the phone may have loaded the page under any name, so the Host isn't checked there)
		if (isHotspotRequest(request)) {
			for (const char *path : HOTSPOT_API_PATHS) {
				if (request->url() == path) {
					return nullptr;
				}
			}
			return "setup_hotspot_only";
		}

		// Through the local network: only under the device's own name or address, so a website that points its own name at the device (DNS rebinding) is refused
		return isDeviceHost(request->host()) ? nullptr : "host_not_allowed";
	}
}

namespace gizmo {
	// Start the Wi-Fi setup and register its web API (/api/networks, /api/connect, /api/check-connection)
	void setupWifi(AsyncWebServer &server, const WifiSetupOptions &options) {
		setupOptions = options;

		// Give the Wi-Fi the mDNS name too, so the router lists the device by it (the core applies it when the interface starts, so before WiFi.mode)
		if (setupOptions.hostname != nullptr && setupOptions.hostname[0] != '\0') {
			WiFi.setHostname(setupOptions.hostname);
		}

		// Station only: the hotspot starts when needed, with its name (WIFI_AP_STA would turn it on right away, open and named ESP_XXXXXX)
		WiFi.mode(WIFI_STA);
		loadSavedCredentials();

		// Refuse API requests under other names, and anything but the setup through the hotspot
		gizmo::setRequestGuard(guardRequest);

		// Register the web API
		setupNetworksRoute(server);
		setupConnectRoute(server);
		setupCheckConnectionRoute(server);
	}

	// Connect, retry, and run the setup hotspot (call on every loop iteration)
	void manageWifi() {
		// Answer DNS requests and close the hotspot when requested
		if (accessPointEnabled && dnsServer != nullptr) {
			dnsServer->processNextRequest();
		}
		manageAccessPointShutdown();

		// Move the connection on (wifi_connection.cpp decides, this runs what it asks for, in order)
		gizmo::WifiConnectionInputs inputs;
		inputs.now = millis();
		inputs.isLinkUp = WiFi.status() == WL_CONNECTED;
		inputs.isAttemptRequested = takeConnectionRequest();
		inputs.hasNewCredentials = hasNewCredentials();
		inputs.hasSavedCredentials = hasSavedCredentials();
		inputs.isAccessPointOn = accessPointEnabled;
		inputs.isAccessPointShutdownRequested = isAccessPointShutdownRequested;
		const gizmo::WifiConnectionActions actions = gizmo::stepWifiConnection(wifiState.connection, inputs);

		// Stop mDNS while the network is down or changing
		if (actions.stopMdns) {
			stopMdns();
		}

		// An attempt timed out: drop new credentials that didn't work (the next attempt goes back to the saved network)
		if (actions.hasFailed) {
			if (actions.dropNewCredentials) {
				gizmo::printLogln("The new Wi-Fi credentials didn't work, falling back to the saved ones");
				clearPendingCredentials();
			}
			WiFi.disconnect();
			gizmo::printLogfln("Failed to connect to Wi-Fi, retrying in %lu ms", static_cast<unsigned long>(gizmo::WIFI_RETRY_INTERVAL_MS));
		}

		// An attempt worked: save the new credentials it used
		if (actions.hasConnected) {
			gizmo::printLogln("Wi-Fi connected!");
			if (actions.saveNewCredentials) {
				saveNewCredentials();
			}
		}

		// Start an attempt, or keep the hotspot available while there is no network
		if (actions.startAttempt) {
			beginConnectionAttempt();
		}
		if (actions.startAccessPoint) {
			startAccessPoint();
		}

		// Turn the setup hotspot off once Wi-Fi has been back for a while, and answer on the mDNS name
		if (actions.requestAccessPointShutdown) {
			gizmo::printLogln("Wi-Fi is back, turning the access point off");
			isAccessPointShutdownRequested = true;
		}
		if (actions.startMdns) {
			startMdns();
		}

		// Tell the web app where the connection is
		publishWifiStatus();
	}

	// Get the mDNS name of the device, without .local (empty before setupWifi)
	const char *getHostname() {
		return setupOptions.hostname != nullptr ? setupOptions.hostname : "";
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
		if (isOpen) {
			preferences.end();
		}

		// Clear the RAM copy
		portENTER_CRITICAL(&credentialsMux);
		savedCredentials = WifiCredentials();
		portEXIT_CRITICAL(&credentialsMux);
		return isCleared;
	}

	// Redirect hotspot clients to the setup page (call first in the server's not-found handler; true if it answered)
	bool handleCaptivePortalRequest(AsyncWebServerRequest *request, const EmbeddedAsset *assets, size_t assetCount) {
		// Only page requests from clients connected through the hotspot (LAN clients keep the normal pages)
		if (!accessPointEnabled) {
			return false;
		}
		if (request->method() != HTTP_GET && request->method() != HTTP_HEAD) {
			return false;
		}
		if (request->client()->localIP() != WiFi.softAPIP()) {
			return false;
		}

		// Never redirect the setup page itself, its files, or the API
		const String path = request->url();
		if (path == setupOptions.setupPagePath || path.startsWith("/api/")) {
			return false;
		}
		if (setupOptions.assetPathPrefix != nullptr && path.startsWith(setupOptions.assetPathPrefix)) {
			return false;
		}
		if (isEmbeddedFile(assets, assetCount, path.c_str())) {
			return false;
		}

		// Send everything else to the setup page
		request->redirect(setupOptions.setupPagePath);
		return true;
	}
}

#endif