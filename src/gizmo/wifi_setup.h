#ifndef GIZMO_WIFI_SETUP_H
#define GIZMO_WIFI_SETUP_H

// Wi-Fi setup, only available on Arduino targets (native test builds skip it)
#ifdef ARDUINO

#include <ESPAsyncWebServer.h>

namespace gizmo {
	// Credential sizes from the Wi-Fi standard (32-character SSID, 64-character password, plus the terminator)
	constexpr size_t WIFI_SSID_SIZE = 33;
	constexpr size_t WIFI_PASSWORD_SIZE = 65;

	// Wi-Fi connection status (the values are part of the web API)
	enum class WifiStatus : uint8_t {
		Disconnected = 0,
		Connected = 1,
		Connecting = 2,
	};

	// Project settings for the Wi-Fi setup
	struct WifiSetupOptions {
		const char *accessPointName; // Name of the open setup hotspot, for example "Gizmo-Ticker"
		const char *hostname; // mDNS name, for example "ticker" for http://ticker.local
		const char *setupPagePath; // Page that hotspot clients are redirected to, for example "/ap-setup"
		const char *assetPathPrefix; // Path prefix of the setup page's files, never redirected (nullptr if none)
	};

	// Start the Wi-Fi setup and register its web API (/api/networks, /api/connect, /api/check-connection)
	void setupWifi(AsyncWebServer &server, const WifiSetupOptions &options);

	// Connect, retry, and run the setup hotspot (call on every loop iteration)
	void manageWifi();

	// Get the current connection status
	WifiStatus getWifiStatus();

	// Check whether the device is connected to Wi-Fi
	bool isWifiConnected();

	// Check whether the setup hotspot is on
	bool isAccessPointActive();

	// Copy the saved network name, returning false if none is saved
	bool getSavedWifiSsid(char *ssid, size_t length);

	// Forget the saved network (safe to call from the web server task)
	bool clearWifiCredentials();

	// Redirect hotspot clients to the setup page (call first in the server's not-found handler; true if it answered)
	bool handleCaptivePortalRequest(AsyncWebServerRequest *request);
}

#endif

#endif