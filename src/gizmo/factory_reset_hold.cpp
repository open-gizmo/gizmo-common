#include "factory_reset_hold.h"

namespace gizmo {
	// Move the hold on by one loop iteration, from whether the button is pressed
	FactoryResetHoldStep stepFactoryResetHold(FactoryResetHold &hold, bool isPressed, uint32_t now) {
		// Released: tell the project if a hold was under way (a press shorter than the debounce time was only bounce)
		if (!isPressed) {
			const bool wasCounting = hold.phase == FactoryResetHoldPhase::Held && now - hold.pressedAt >= FACTORY_RESET_DEBOUNCE_MS;
			hold.phase = FactoryResetHoldPhase::Released;
			return { wasCounting ? FactoryResetHoldEvent::Cancelled : FactoryResetHoldEvent::None, 0 };
		}

		// Just pressed: start timing
		if (hold.phase == FactoryResetHoldPhase::Released) {
			hold.phase = FactoryResetHoldPhase::Held;
			hold.pressedAt = now;
			hold.lastReportAt = now;
			return { FactoryResetHoldEvent::None, 0 };
		}

		// Already reset or refused: wait for the release
		if (hold.phase == FactoryResetHoldPhase::Handled) {
			return { FactoryResetHoldEvent::None, 0 };
		}

		// Still held: report the time left
		const uint32_t heldMs = now - hold.pressedAt;
		if (heldMs < FACTORY_RESET_HOLD_MS) {
			if (heldMs >= FACTORY_RESET_DEBOUNCE_MS && now - hold.lastReportAt >= FACTORY_RESET_REPORT_MS) {
				hold.lastReportAt = now;
				return { FactoryResetHoldEvent::Progress, FACTORY_RESET_HOLD_MS - heldMs };
			}
			return { FactoryResetHoldEvent::None, 0 };
		}

		// Held long enough: the project resets, or refuses it
		hold.phase = FactoryResetHoldPhase::Handled;
		return { FactoryResetHoldEvent::Completed, 0 };
	}
}
