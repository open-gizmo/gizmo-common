#include <stdint.h>
#include <unity.h>

#include <gizmo/factory_reset_hold.h>

using gizmo::FactoryResetHold;
using gizmo::FactoryResetHoldEvent;
using gizmo::FactoryResetHoldPhase;
using gizmo::FactoryResetHoldStep;

namespace {
	// The hold under test
	FactoryResetHold hold;

	// Run one loop iteration with the button pressed or not
	FactoryResetHoldStep stepAt(bool isPressed, uint32_t now) {
		return gizmo::stepFactoryResetHold(hold, isPressed, now);
	}

	// Check the event of a step
	void assertEvent(FactoryResetHoldEvent expected, const FactoryResetHoldStep &step) {
		TEST_ASSERT_EQUAL(static_cast<int>(expected), static_cast<int>(step.event));
	}

	// Hold the button from a time to another, polling every 10 ms like a busy loop, and count the events
	struct HoldCounts {
		uint32_t progress;
		uint32_t completed;
		uint32_t lastRemainingMs;
	};
	HoldCounts holdFromTo(uint32_t from, uint32_t to) {
		HoldCounts counts = { 0, 0, 0 };
		for (uint32_t now = from; now != to + 10; now += 10) {
			const FactoryResetHoldStep step = stepAt(true, now);
			if (step.event == FactoryResetHoldEvent::Progress) {
				counts.progress++;
				counts.lastRemainingMs = step.remainingMs;
			}
			if (step.event == FactoryResetHoldEvent::Completed) {
				counts.completed++;
			}
		}
		return counts;
	}
}

// Start each test with the button released
void setUp() {
	hold = FactoryResetHold();
}

// Nothing to clean up after each test
void tearDown() {}

// A press shorter than the debounce time is contact bounce: no event at all
void test_ignores_contact_bounce() {
	assertEvent(FactoryResetHoldEvent::None, stepAt(true, 1000));
	assertEvent(FactoryResetHoldEvent::None, stepAt(true, 1000 + gizmo::FACTORY_RESET_DEBOUNCE_MS - 1));
	assertEvent(FactoryResetHoldEvent::None, stepAt(false, 1000 + gizmo::FACTORY_RESET_DEBOUNCE_MS - 1));
	TEST_ASSERT_EQUAL(static_cast<int>(FactoryResetHoldPhase::Released), static_cast<int>(hold.phase));
}

// While held, the time left is reported about every 250 ms
void test_reports_the_time_left_while_held() {
	stepAt(true, 0);
	assertEvent(FactoryResetHoldEvent::None, stepAt(true, gizmo::FACTORY_RESET_REPORT_MS - 1));
	const FactoryResetHoldStep step = stepAt(true, gizmo::FACTORY_RESET_REPORT_MS);
	assertEvent(FactoryResetHoldEvent::Progress, step);
	TEST_ASSERT_EQUAL_UINT32(gizmo::FACTORY_RESET_HOLD_MS - gizmo::FACTORY_RESET_REPORT_MS, step.remainingMs);
	assertEvent(FactoryResetHoldEvent::None, stepAt(true, gizmo::FACTORY_RESET_REPORT_MS + 10));
}

// Releasing the button before the end cancels the reset
void test_cancels_when_released_early() {
	holdFromTo(0, 3000);
	assertEvent(FactoryResetHoldEvent::Cancelled, stepAt(false, 3010));
	assertEvent(FactoryResetHoldEvent::None, stepAt(false, 3020));
}

// Held for the whole time, the reset completes once, then nothing until the button is released (no cancel either)
void test_completes_once_after_the_full_hold() {
	const HoldCounts counts = holdFromTo(0, gizmo::FACTORY_RESET_HOLD_MS + 5000);
	TEST_ASSERT_EQUAL_UINT32(1, counts.completed);
	TEST_ASSERT_EQUAL_UINT32(gizmo::FACTORY_RESET_HOLD_MS / gizmo::FACTORY_RESET_REPORT_MS - 1, counts.progress);
	TEST_ASSERT_LESS_OR_EQUAL_UINT32(gizmo::FACTORY_RESET_REPORT_MS, counts.lastRemainingMs);
	TEST_ASSERT_EQUAL(static_cast<int>(FactoryResetHoldPhase::Handled), static_cast<int>(hold.phase));
	assertEvent(FactoryResetHoldEvent::None, stepAt(false, gizmo::FACTORY_RESET_HOLD_MS + 6000));
}

// After a refused reset, the button must be released before a new hold counts, which starts from zero
void test_starts_again_after_a_release() {
	holdFromTo(0, gizmo::FACTORY_RESET_HOLD_MS);
	stepAt(false, 20000);
	stepAt(true, 30000);
	assertEvent(FactoryResetHoldEvent::Progress, stepAt(true, 30000 + gizmo::FACTORY_RESET_HOLD_MS - 1));
	assertEvent(FactoryResetHoldEvent::Completed, stepAt(true, 30000 + gizmo::FACTORY_RESET_HOLD_MS));
}

// The hold time stays right when millis() wraps around
void test_completes_across_the_millis_rollover() {
	const uint32_t pressedAt = 0xFFFFF000UL;
	const HoldCounts counts = holdFromTo(pressedAt, static_cast<uint32_t>(pressedAt + gizmo::FACTORY_RESET_HOLD_MS));
	TEST_ASSERT_EQUAL_UINT32(1, counts.completed);
}

// Run all the factory reset hold tests
int main() {
	UNITY_BEGIN();
	RUN_TEST(test_ignores_contact_bounce);
	RUN_TEST(test_reports_the_time_left_while_held);
	RUN_TEST(test_cancels_when_released_early);
	RUN_TEST(test_completes_once_after_the_full_hold);
	RUN_TEST(test_starts_again_after_a_release);
	RUN_TEST(test_completes_across_the_millis_rollover);
	return UNITY_END();
}
