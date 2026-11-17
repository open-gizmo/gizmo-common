#ifndef GIZMO_FACTORY_RESET_HOLD_H
#define GIZMO_FACTORY_RESET_HOLD_H

#include <stdint.h>

// The state machine of the BOOT button hold for a factory reset, kept free of hardware headers so the native tests can drive it (times are millis(), 32-bit like on the ESP32)

namespace gizmo {
	// How long the BOOT button must be held for a factory reset
	constexpr uint32_t FACTORY_RESET_HOLD_MS = 10000;

	// How often the hold progress is reported, and how long a press must last to count (against contact bounce)
	constexpr uint32_t FACTORY_RESET_REPORT_MS = 250;
	constexpr uint32_t FACTORY_RESET_DEBOUNCE_MS = 50;

	// Where the hold is
	enum class FactoryResetHoldPhase : uint8_t {
		Released,
		Held, // Held, counting down to the reset
		Handled // The hold already completed (reset or refused): nothing more until the button is released
	};

	// The hold, and the times it depends on
	struct FactoryResetHold {
		FactoryResetHoldPhase phase = FactoryResetHoldPhase::Released;
		uint32_t pressedAt = 0;
		uint32_t lastReportAt = 0;
	};

	// What happened on this loop iteration
	enum class FactoryResetHoldEvent : uint8_t {
		None,
		Progress, // Still held: report the time left
		Cancelled, // Released before the reset
		Completed // Held long enough: reset now, unless the project refuses it
	};

	// The event, and the time left for a Progress
	struct FactoryResetHoldStep {
		FactoryResetHoldEvent event;
		uint32_t remainingMs;
	};

	// Move the hold on by one loop iteration, from whether the button is pressed
	FactoryResetHoldStep stepFactoryResetHold(FactoryResetHold &hold, bool isPressed, uint32_t now);
}

#endif
