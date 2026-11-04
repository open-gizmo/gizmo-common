#ifndef GIZMO_DEVICE_H
#define GIZMO_DEVICE_H

// Device API, only available on Arduino targets (native test builds skip it)
#ifdef ARDUINO

#include <ESPAsyncWebServer.h>

namespace gizmo {
	// Project details shown by the device API (the strings must stay valid, for example string literals)
	struct DeviceInfo {
		const char *name; // Project name, for example "Gizmo Ticker"
		const char *version; // Firmware version, for example "v2026.11.04.1" (or "dev" for a local build)
	};

	// Register the device API: GET /api/device, POST /api/restart and POST /api/forget-wifi (call after setupWifi)
	void setupDeviceApi(AsyncWebServer &server, const DeviceInfo &info);

	// Restart the device once the current response has been sent (safe to call from the web server task)
	void scheduleRestart();

	// Perform a scheduled restart (call on every loop iteration)
	void manageDevice();
}

#endif

#endif