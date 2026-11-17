#include <stdint.h>
#include <unity.h>

#include <gizmo/wifi_connection.h>

using gizmo::WifiConnection;
using gizmo::WifiConnectionActions;
using gizmo::WifiConnectionInputs;
using gizmo::WifiConnectionPhase;
using gizmo::WifiStatus;

namespace {
	// The connection under test, and the Wi-Fi around it
	WifiConnection connection;
	WifiConnectionInputs inputs;

	// Run one loop iteration at a time, like manageWifi (a request lasts one iteration, like takeConnectionRequest)
	WifiConnectionActions stepAt(uint32_t now) {
		inputs.now = now;
		const WifiConnectionActions actions = gizmo::stepWifiConnection(connection, inputs);
		inputs.isAttemptRequested = false;
		return actions;
	}

	// The web app sends new credentials
	WifiConnectionActions requestAt(uint32_t now) {
		inputs.isAttemptRequested = true;
		inputs.hasNewCredentials = true;
		return stepAt(now);
	}

	// What manageWifi does with the new credentials once they worked or failed
	void applyCredentialActions(const WifiConnectionActions &actions) {
		if (actions.saveNewCredentials) {
			inputs.hasSavedCredentials = true;
			inputs.hasNewCredentials = false;
		}
		if (actions.dropNewCredentials) {
			inputs.hasNewCredentials = false;
		}
	}

	// Check the phase and the status the web app sees
	void assertState(WifiConnectionPhase phase, WifiStatus status) {
		TEST_ASSERT_EQUAL(static_cast<int>(phase), static_cast<int>(connection.phase));
		TEST_ASSERT_EQUAL(static_cast<int>(status), static_cast<int>(gizmo::getWifiConnectionStatus(connection)));
	}

	// Boot with a saved network and connect to it at a given time
	void connectAtBoot(uint32_t connectedAt) {
		inputs.hasSavedCredentials = true;
		stepAt(0);
		inputs.isLinkUp = true;
		stepAt(connectedAt);
	}
}

// Start each test from a fresh boot: no network, nothing saved, hotspot off
void setUp() {
	connection = WifiConnection();
	inputs = { 0, false, false, false, false, false, false };
}

// Nothing to clean up after each test
void tearDown() {}

/******************************** Boot ********************************/

// A saved network is tried right away, without saying Connecting (that's for the web app's own attempts)
void test_tries_the_saved_network_at_boot() {
	inputs.hasSavedCredentials = true;
	const WifiConnectionActions actions = stepAt(0);
	TEST_ASSERT_TRUE(actions.startAttempt);
	TEST_ASSERT_FALSE(actions.startAccessPoint);
	assertState(WifiConnectionPhase::Connecting, WifiStatus::Disconnected);
}

// With nothing saved, the setup hotspot opens and no attempt starts
void test_opens_the_hotspot_without_a_saved_network() {
	for (uint32_t now = 0; now <= 60000; now += 1000) {
		const WifiConnectionActions actions = stepAt(now);
		TEST_ASSERT_FALSE(actions.startAttempt);
		TEST_ASSERT_TRUE(actions.startAccessPoint);
	}
	assertState(WifiConnectionPhase::Idle, WifiStatus::Disconnected);
}

// The attempt works: connected, mDNS on, and nothing to save (the network was already saved)
void test_connects_to_the_saved_network() {
	inputs.hasSavedCredentials = true;
	stepAt(0);
	TEST_ASSERT_FALSE(stepAt(3000).startMdns);
	inputs.isLinkUp = true;
	const WifiConnectionActions actions = stepAt(4000);
	TEST_ASSERT_TRUE(actions.hasConnected);
	TEST_ASSERT_TRUE(actions.startMdns);
	TEST_ASSERT_FALSE(actions.saveNewCredentials);
	assertState(WifiConnectionPhase::Connected, WifiStatus::Connected);
	TEST_ASSERT_EQUAL_UINT32(4000, connection.connectedSince);

	// Staying connected keeps mDNS on, without reporting a new connection
	const WifiConnectionActions later = stepAt(5000);
	TEST_ASSERT_FALSE(later.hasConnected);
	TEST_ASSERT_TRUE(later.startMdns);
	TEST_ASSERT_EQUAL_UINT32(4000, connection.connectedSince);
}

/******************************** Failures and retries ********************************/

// An attempt that doesn't connect in time fails, opens the hotspot, and is retried after the retry interval
void test_retries_after_a_timeout() {
	inputs.hasSavedCredentials = true;
	stepAt(0);
	TEST_ASSERT_FALSE(stepAt(gizmo::WIFI_CONNECT_TIMEOUT_MS - 1).hasFailed);

	// The attempt times out
	const WifiConnectionActions failed = stepAt(gizmo::WIFI_CONNECT_TIMEOUT_MS);
	TEST_ASSERT_TRUE(failed.hasFailed);
	TEST_ASSERT_TRUE(failed.startAccessPoint);
	TEST_ASSERT_FALSE(failed.dropNewCredentials);
	assertState(WifiConnectionPhase::Idle, WifiStatus::Disconnected);

	// Wait for the retry interval, with the hotspot on
	const uint32_t failedAt = gizmo::WIFI_CONNECT_TIMEOUT_MS;
	const WifiConnectionActions waiting = stepAt(failedAt + gizmo::WIFI_RETRY_INTERVAL_MS - 1);
	TEST_ASSERT_FALSE(waiting.startAttempt);
	TEST_ASSERT_TRUE(waiting.startAccessPoint);

	// Then try again
	TEST_ASSERT_TRUE(stepAt(failedAt + gizmo::WIFI_RETRY_INTERVAL_MS).startAttempt);
	assertState(WifiConnectionPhase::Connecting, WifiStatus::Disconnected);
}

// When the link drops (a router reboot), the saved network is tried again right away, even after earlier failures
void test_reconnects_right_away_when_the_link_drops() {
	inputs.hasSavedCredentials = true;
	stepAt(0);
	stepAt(gizmo::WIFI_CONNECT_TIMEOUT_MS);
	inputs.isLinkUp = true;
	stepAt(gizmo::WIFI_CONNECT_TIMEOUT_MS + 1000);
	inputs.isLinkUp = false;
	const WifiConnectionActions actions = stepAt(gizmo::WIFI_CONNECT_TIMEOUT_MS + 2000);
	TEST_ASSERT_TRUE(actions.stopMdns);
	TEST_ASSERT_TRUE(actions.startAttempt);
	assertState(WifiConnectionPhase::Connecting, WifiStatus::Disconnected);
}

// The Wi-Fi coming back on its own while waiting counts as connected, without an attempt to report
void test_takes_a_link_that_comes_back_on_its_own() {
	inputs.hasSavedCredentials = true;
	stepAt(0);
	stepAt(gizmo::WIFI_CONNECT_TIMEOUT_MS);
	inputs.isLinkUp = true;
	const WifiConnectionActions actions = stepAt(gizmo::WIFI_CONNECT_TIMEOUT_MS + 2000);
	TEST_ASSERT_FALSE(actions.hasConnected);
	TEST_ASSERT_TRUE(actions.startMdns);
	assertState(WifiConnectionPhase::Connected, WifiStatus::Connected);
}

/******************************** New credentials from the web app ********************************/

// New credentials from the setup hotspot: Connecting at once, saved once they work
void test_saves_new_credentials_that_work() {
	stepAt(0);
	const WifiConnectionActions requested = requestAt(5000);
	TEST_ASSERT_TRUE(requested.startAttempt);
	TEST_ASSERT_TRUE(requested.stopMdns);
	assertState(WifiConnectionPhase::Connecting, WifiStatus::Connecting);

	// The attempt works
	inputs.isLinkUp = true;
	const WifiConnectionActions connected = stepAt(9000);
	TEST_ASSERT_TRUE(connected.hasConnected);
	TEST_ASSERT_TRUE(connected.saveNewCredentials);
	assertState(WifiConnectionPhase::Connected, WifiStatus::Connected);
}

// Wrong credentials are dropped, the web app sees Disconnected (its "wrong password"), and the retries of the saved network don't say Connecting again
void test_drops_new_credentials_that_fail() {
	inputs.hasSavedCredentials = true;
	stepAt(0);
	stepAt(gizmo::WIFI_CONNECT_TIMEOUT_MS);
	requestAt(20000);
	assertState(WifiConnectionPhase::Connecting, WifiStatus::Connecting);

	// The attempt times out
	const WifiConnectionActions failed = stepAt(20000 + gizmo::WIFI_CONNECT_TIMEOUT_MS);
	TEST_ASSERT_TRUE(failed.hasFailed);
	TEST_ASSERT_TRUE(failed.dropNewCredentials);
	applyCredentialActions(failed);
	assertState(WifiConnectionPhase::Idle, WifiStatus::Disconnected);

	// The next retry goes back to the saved network, and stays Disconnected for the web app
	const WifiConnectionActions retry = stepAt(20000 + gizmo::WIFI_CONNECT_TIMEOUT_MS + gizmo::WIFI_RETRY_INTERVAL_MS);
	TEST_ASSERT_TRUE(retry.startAttempt);
	TEST_ASSERT_FALSE(connection.isAttemptWithNewCredentials);
	assertState(WifiConnectionPhase::Connecting, WifiStatus::Disconnected);
}

// Changing network while connected: the old link is not taken for the new one
void test_waits_for_the_old_link_to_drop() {
	connectAtBoot(2000);
	requestAt(10000);
	assertState(WifiConnectionPhase::WaitingForOldLink, WifiStatus::Connecting);

	// The old link is still up: not a success
	const WifiConnectionActions lingering = stepAt(10100);
	TEST_ASSERT_FALSE(lingering.hasConnected);
	TEST_ASSERT_FALSE(lingering.saveNewCredentials);
	TEST_ASSERT_TRUE(lingering.stopMdns);
	assertState(WifiConnectionPhase::WaitingForOldLink, WifiStatus::Connecting);

	// It drops, then the new network connects
	inputs.isLinkUp = false;
	stepAt(10500);
	assertState(WifiConnectionPhase::Connecting, WifiStatus::Connecting);
	inputs.isLinkUp = true;
	const WifiConnectionActions connected = stepAt(13000);
	TEST_ASSERT_TRUE(connected.hasConnected);
	TEST_ASSERT_TRUE(connected.saveNewCredentials);
	TEST_ASSERT_EQUAL_UINT32(13000, connection.connectedSince);
}

// If the old link never drops, the attempt times out like any other
void test_times_out_when_the_old_link_never_drops() {
	connectAtBoot(2000);
	requestAt(10000);
	const WifiConnectionActions failed = stepAt(10000 + gizmo::WIFI_CONNECT_TIMEOUT_MS);
	TEST_ASSERT_TRUE(failed.hasFailed);
	TEST_ASSERT_TRUE(failed.dropNewCredentials);
	assertState(WifiConnectionPhase::Idle, WifiStatus::Disconnected);
}

// A new request restarts the attempt, even in the middle of another one
void test_restarts_the_attempt_on_a_new_request() {
	requestAt(0);
	requestAt(8000);
	assertState(WifiConnectionPhase::Connecting, WifiStatus::Connecting);
	TEST_ASSERT_FALSE(stepAt(8000 + gizmo::WIFI_CONNECT_TIMEOUT_MS - 1).hasFailed);
	TEST_ASSERT_TRUE(stepAt(8000 + gizmo::WIFI_CONNECT_TIMEOUT_MS).hasFailed);
}

/******************************** Hotspot ********************************/

// The hotspot closes once the Wi-Fi has been back for a while, and only once
void test_closes_the_hotspot_after_the_delay() {
	inputs.isAccessPointOn = true;
	connectAtBoot(2000);
	TEST_ASSERT_FALSE(stepAt(2000 + gizmo::ACCESS_POINT_SHUTDOWN_DELAY_MS - 1).requestAccessPointShutdown);
	TEST_ASSERT_TRUE(stepAt(2000 + gizmo::ACCESS_POINT_SHUTDOWN_DELAY_MS).requestAccessPointShutdown);

	// Already requested, or already off: nothing more to ask
	inputs.isAccessPointShutdownRequested = true;
	TEST_ASSERT_FALSE(stepAt(2000 + gizmo::ACCESS_POINT_SHUTDOWN_DELAY_MS + 1).requestAccessPointShutdown);
	inputs.isAccessPointShutdownRequested = false;
	inputs.isAccessPointOn = false;
	TEST_ASSERT_FALSE(stepAt(2000 + gizmo::ACCESS_POINT_SHUTDOWN_DELAY_MS + 2).requestAccessPointShutdown);
}

/******************************** millis() rollover ********************************/

// The timeouts stay right when millis() wraps around (after about 49.7 days)
void test_times_out_across_the_millis_rollover() {
	inputs.hasSavedCredentials = true;
	const uint32_t startedAt = 0xFFFFF000UL;
	connection.retryWaitStartedAt = startedAt;
	stepAt(startedAt);
	assertState(WifiConnectionPhase::Connecting, WifiStatus::Disconnected);
	TEST_ASSERT_FALSE(stepAt(0xFFFFFFF0UL).hasFailed);
	TEST_ASSERT_FALSE(stepAt(static_cast<uint32_t>(startedAt + gizmo::WIFI_CONNECT_TIMEOUT_MS - 1)).hasFailed);
	TEST_ASSERT_TRUE(stepAt(static_cast<uint32_t>(startedAt + gizmo::WIFI_CONNECT_TIMEOUT_MS)).hasFailed);
}

// Run all the Wi-Fi connection tests
int main() {
	UNITY_BEGIN();
	RUN_TEST(test_tries_the_saved_network_at_boot);
	RUN_TEST(test_opens_the_hotspot_without_a_saved_network);
	RUN_TEST(test_connects_to_the_saved_network);
	RUN_TEST(test_retries_after_a_timeout);
	RUN_TEST(test_reconnects_right_away_when_the_link_drops);
	RUN_TEST(test_takes_a_link_that_comes_back_on_its_own);
	RUN_TEST(test_saves_new_credentials_that_work);
	RUN_TEST(test_drops_new_credentials_that_fail);
	RUN_TEST(test_waits_for_the_old_link_to_drop);
	RUN_TEST(test_times_out_when_the_old_link_never_drops);
	RUN_TEST(test_restarts_the_attempt_on_a_new_request);
	RUN_TEST(test_closes_the_hotspot_after_the_delay);
	RUN_TEST(test_times_out_across_the_millis_rollover);
	return UNITY_END();
}
