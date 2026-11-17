#include "wifi_connection.h"

namespace {
	// Start an attempt: the old link must drop first if it is still up
	void beginAttempt(gizmo::WifiConnection &connection, const gizmo::WifiConnectionInputs &inputs, gizmo::WifiConnectionActions &actions) {
		connection.phase = inputs.isLinkUp ? gizmo::WifiConnectionPhase::WaitingForOldLink : gizmo::WifiConnectionPhase::Connecting;
		connection.isAttemptWithNewCredentials = inputs.hasNewCredentials;
		connection.attemptStartedAt = inputs.now;
		actions.startAttempt = true;
	}
}

namespace gizmo {
	// Move the connection on by one loop iteration, and say what to do
	WifiConnectionActions stepWifiConnection(WifiConnection &connection, const WifiConnectionInputs &inputs) {
		WifiConnectionActions actions;

		// Start an attempt requested from the web app right away, even if already connected
		if (inputs.isAttemptRequested) {
			actions.stopMdns = true;
			connection.retryDelay = 0;
			beginAttempt(connection, inputs, actions);
			return actions;
		}

		// While an attempt waits for the previous link to drop, don't take the old connection for the new one
		if (connection.phase == WifiConnectionPhase::WaitingForOldLink && !inputs.isLinkUp) {
			connection.phase = WifiConnectionPhase::Connecting;
		}

		// Connected (by an attempt, or by the Wi-Fi coming back on its own)
		if (inputs.isLinkUp && connection.phase != WifiConnectionPhase::WaitingForOldLink) {
			// An attempt just worked: save the new credentials it used
			if (connection.phase == WifiConnectionPhase::Connecting) {
				actions.hasConnected = true;
				actions.saveNewCredentials = connection.isAttemptWithNewCredentials;
			}

			// Remember when the connection came back
			if (connection.phase != WifiConnectionPhase::Connected) {
				connection.connectedSince = inputs.now;
			}
			connection.phase = WifiConnectionPhase::Connected;
			connection.isAttemptWithNewCredentials = false;
			connection.retryDelay = 0; // Try again right away if the link drops

			// Turn the setup hotspot off once Wi-Fi has been back for a while
			if (inputs.isAccessPointOn && !inputs.isAccessPointShutdownRequested && inputs.now - connection.connectedSince >= ACCESS_POINT_SHUTDOWN_DELAY_MS) {
				actions.requestAccessPointShutdown = true;
			}
			actions.startMdns = true;
			return actions;
		}

		// Not connected
		actions.stopMdns = true;

		// Poll the current attempt until it succeeds or times out (the elapsed time stays right across a millis() rollover)
		if (isWifiAttemptInProgress(connection)) {
			if (inputs.now - connection.attemptStartedAt >= WIFI_CONNECT_TIMEOUT_MS) {
				actions.hasFailed = true;
				actions.dropNewCredentials = connection.isAttemptWithNewCredentials;
				actions.startAccessPoint = true;
				connection.phase = WifiConnectionPhase::Idle;
				connection.isAttemptWithNewCredentials = false;
				connection.retryWaitStartedAt = inputs.now;
				connection.retryDelay = WIFI_RETRY_INTERVAL_MS;
			}
			return actions;
		}

		// The link dropped
		connection.phase = WifiConnectionPhase::Idle;

		// Retry the saved network when it's time, keeping the hotspot available meanwhile
		if (inputs.hasSavedCredentials && inputs.now - connection.retryWaitStartedAt >= connection.retryDelay) {
			beginAttempt(connection, inputs, actions);
			return actions;
		}
		actions.startAccessPoint = true;
		return actions;
	}

	// Check whether an attempt is under way
	bool isWifiAttemptInProgress(const WifiConnection &connection) {
		return connection.phase == WifiConnectionPhase::Connecting || connection.phase == WifiConnectionPhase::WaitingForOldLink;
	}

	// The status for the web app: Connecting only while new credentials from the web app are tried
	WifiStatus getWifiConnectionStatus(const WifiConnection &connection) {
		if (connection.phase == WifiConnectionPhase::Connected) {
			return WifiStatus::Connected;
		}
		if (isWifiAttemptInProgress(connection) && connection.isAttemptWithNewCredentials) {
			return WifiStatus::Connecting;
		}
		return WifiStatus::Disconnected;
	}
}
