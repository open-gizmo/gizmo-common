#ifndef GIZMO_WIFI_CONNECTION_H
#define GIZMO_WIFI_CONNECTION_H

#include <stdint.h>

// The state machine of the Wi-Fi connection, kept free of hardware headers so the native tests can drive it (times are millis(), 32-bit like on the ESP32)

namespace gizmo {
	constexpr uint32_t WIFI_CONNECT_TIMEOUT_MS = 12500; // How long an attempt may take before it counts as failed
	constexpr uint32_t WIFI_RETRY_INTERVAL_MS = 10000; // Wait after a failed attempt before trying the saved network again
	constexpr uint32_t ACCESS_POINT_SHUTDOWN_DELAY_MS = 120000; // Keep the hotspot for 2 minutes after reconnecting (so a phone in the setup flow sees the result)

	// Wi-Fi connection status (the values are part of the web API)
	enum class WifiStatus : uint8_t {
		Disconnected = 0,
		Connected = 1,
		Connecting = 2,
	};

	// Where the connection is
	enum class WifiConnectionPhase : uint8_t {
		Idle, // Not connected and not trying: waiting before the next attempt, or no network saved
		WaitingForOldLink, // An attempt started while connected, and the old link is still up (so it isn't taken for the new one)
		Connecting, // An attempt is under way
		Connected
	};

	// The connection, and the times it depends on
	struct WifiConnection {
		WifiConnectionPhase phase = WifiConnectionPhase::Idle;
		bool isAttemptWithNewCredentials = false; // The attempt uses credentials from the web app that are not saved yet
		uint32_t attemptStartedAt = 0;
		uint32_t retryWaitStartedAt = 0; // When the wait before the next attempt started
		uint32_t retryDelay = 0; // How long to wait before the next attempt (0 = try now)
		uint32_t connectedSince = 0;
	};

	// What the Wi-Fi reports on this loop iteration
	struct WifiConnectionInputs {
		uint32_t now;
		bool isLinkUp; // The station is connected
		bool isAttemptRequested; // The web app sent new credentials to try
		bool hasNewCredentials; // Credentials from the web app are waiting to be tried or saved
		bool hasSavedCredentials;
		bool isAccessPointOn;
		bool isAccessPointShutdownRequested;
	};

	// What the Wi-Fi code must do after a step, in this order
	struct WifiConnectionActions {
		bool stopMdns = false;
		bool dropNewCredentials = false; // The new credentials didn't work: the next attempt goes back to the saved network
		bool saveNewCredentials = false; // The new credentials worked: save them
		bool startAttempt = false; // Connect with the new credentials if any, otherwise the saved ones
		bool startAccessPoint = false;
		bool requestAccessPointShutdown = false; // The Wi-Fi has been back long enough to close the setup hotspot
		bool startMdns = false;
		bool hasConnected = false; // An attempt just worked
		bool hasFailed = false; // An attempt just timed out: disconnect it
	};

	// Move the connection on by one loop iteration, and say what to do
	WifiConnectionActions stepWifiConnection(WifiConnection &connection, const WifiConnectionInputs &inputs);

	// Check whether an attempt is under way
	bool isWifiAttemptInProgress(const WifiConnection &connection);

	// The status for the web app: Connecting only while new credentials from the web app are tried (the retries of the saved network stay Disconnected, so the setup page and the gadgets' screens don't flicker)
	WifiStatus getWifiConnectionStatus(const WifiConnection &connection);
}

#endif
