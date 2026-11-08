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

	// Perform a scheduled restart, and watch the BOOT button for a factory reset (call on every loop iteration)
	void manageDevice();

	// The XIAO ESP32-S3's BOOT button (GPIO0): it only selects the flashing mode at power-up, so the firmware can read it once running
	constexpr uint8_t BOOT_BUTTON_PIN = 0;

	// How long the BOOT button must be held for a factory reset
	constexpr unsigned long FACTORY_RESET_HOLD_MS = 10000;

	// What a factory reset does besides forgetting the Wi-Fi (the callbacks run in the main loop)
	struct FactoryResetOptions {
		void (*clearSettings)(); // Forget the project's own settings (required)
		bool (*canReset)() = nullptr; // Optional: return false to refuse the reset right now
		void (*onHold)(unsigned long remainingMs) = nullptr; // Optional: called about every 250 ms while the button is held, with the time left
		void (*onCancel)() = nullptr; // Optional: called when the button is released before the reset
		void (*onRefused)() = nullptr; // Optional: called when the hold completes but canReset refuses it
	};

	// Turn on the factory reset: holding the BOOT button for FACTORY_RESET_HOLD_MS forgets the Wi-Fi and the project's settings, then restarts into the setup hotspot (only someone at the device can do it)
	void setupFactoryReset(const FactoryResetOptions &options);
}

#endif

#endif