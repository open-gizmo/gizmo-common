#include "device.h"

#ifdef ARDUINO

#include <WiFi.h>

#include "http.h"
#include "log.h"
#include "wifi_setup.h"

namespace {
	// Time the web server gets to send the last response before a restart
	constexpr unsigned long RESTART_DELAY_MS = 1000;

	// Project details, and the hostname reported with them
	gizmo::DeviceInfo deviceInfo = { "", "" };

	// Deferred restart state (written by the web server task, read by the main loop)
	bool isRestartScheduled = false;
	unsigned long restartScheduledAt = 0;
	portMUX_TYPE restartMux = portMUX_INITIALIZER_UNLOCKED;

	// Turn an RSSI into a signal quality from 0 to 100 (-100 dBm or less is 0, -50 dBm or more is 100)
	int getSignalQuality(int rssi) {
		if (rssi <= -100) {
			return 0;
		}
		if (rssi >= -50) {
			return 100;
		}
		return 2 * (rssi + 100);
	}

	// Build the device details
	JsonDocument buildDeviceData() {
		JsonDocument doc;

		// Add the project details and the health of the device
		doc["name"] = deviceInfo.name;
		doc["version"] = deviceInfo.version;
		doc["uptimeSeconds"] = millis() / 1000;
		doc["freeHeap"] = ESP.getFreeHeap();

		// Add the saved network and the hotspot state
		char ssid[gizmo::WIFI_SSID_SIZE];
		gizmo::getSavedWifiSsid(ssid, sizeof(ssid));
		doc["ssid"] = ssid;
		doc["accessPointActive"] = gizmo::isAccessPointActive();

		// Add the address and the signal, once connected
		if (gizmo::isWifiConnected()) {
			doc["ip"] = WiFi.localIP().toString();
			doc["hostname"] = String(WiFi.getHostname()) + ".local";
			doc["signalQuality"] = getSignalQuality(WiFi.RSSI());
		}

		// Return the built JSON document
		return doc;
	}
}

namespace gizmo {
	// Register the device API: GET /api/device, POST /api/restart and POST /api/forget-wifi (call after setupWifi)
	void setupDeviceApi(AsyncWebServer &server, const DeviceInfo &info) {
		deviceInfo = info;

		// Get the device details
		server.on("/api/device", HTTP_GET, [](AsyncWebServerRequest *request) {
			if (!ensureCorsAllowed(request)) {
				return;
			}
			JsonDocument doc = buildDeviceData();
			sendSuccessResponse(request, 200, &doc);
		});

		// Restart the device
		server.on("/api/restart", HTTP_POST, [](AsyncWebServerRequest *request) {
			if (!ensureCorsAllowed(request)) {
				return;
			}
			printLogln("Restart requested from the web app");
			sendSuccessResponse(request);
			scheduleRestart();
		});

		// Forget the saved network and restart into the setup hotspot
		server.on("/api/forget-wifi", HTTP_POST, [](AsyncWebServerRequest *request) {
			if (!ensureCorsAllowed(request)) {
				return;
			}
			if (!clearWifiCredentials()) {
				sendErrorResponse(request, 500, "wifi_forget_failed", "Failed to forget the network");
				return;
			}
			printLogln("Network forgotten from the web app");
			sendSuccessResponse(request);
			scheduleRestart();
		});

		// Answer the preflight requests of the POST routes
		server.on("/api/restart", HTTP_OPTIONS, [](AsyncWebServerRequest *request) {
			sendCorsPreflightResponse(request, "POST, OPTIONS");
		});
		server.on("/api/forget-wifi", HTTP_OPTIONS, [](AsyncWebServerRequest *request) {
			sendCorsPreflightResponse(request, "POST, OPTIONS");
		});
	}

	// Restart the device once the current response has been sent (safe to call from the web server task)
	void scheduleRestart() {
		portENTER_CRITICAL(&restartMux);
		isRestartScheduled = true;
		restartScheduledAt = millis();
		portEXIT_CRITICAL(&restartMux);
	}

	// Perform a scheduled restart (call on every loop iteration)
	void manageDevice() {
		portENTER_CRITICAL(&restartMux);
		const bool isRestartDue = isRestartScheduled && millis() - restartScheduledAt >= RESTART_DELAY_MS;
		portEXIT_CRITICAL(&restartMux);

		// Restart the device
		if (isRestartDue) {
			ESP.restart();
		}
	}
}

#endif