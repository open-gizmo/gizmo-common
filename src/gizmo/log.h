#ifndef GIZMO_LOG_H
#define GIZMO_LOG_H

// Serial logging, only available on Arduino targets (native test builds skip it)
#ifdef ARDUINO

#include <Arduino.h>

namespace gizmo {
	// Whether the build is a debug one: -D PIO_DEV_MODE=1 in its build flags (off by default, so released firmware never logs)
	bool isDevMode();

	// Start logging in debug builds only, the OpenGizmo convention (call first in setup, before any other task can log)
	void initLog();

	// Start logging, or not, whatever the build (call first in setup, before any other task can log)
	void initLog(bool isEnabled, uint32_t baudRate = 115200);

	// Check whether logging is enabled
	bool isLogEnabled();

	// Print log
	template <typename T>
	inline void printLog(T message) {
		if (isLogEnabled()) {
			Serial.print(message);
		}
	}

	// Print log with newline
	template <typename T>
	inline void printLogln(T message) {
		if (isLogEnabled()) {
			Serial.println(message);
		}
	}

	// Printf-style log with format string (the compiler checks the arguments against the format)
	void printLogf(const char *format, ...) __attribute__((format(printf, 1, 2)));

	// Printf-style log with format string and newline (the compiler checks the arguments against the format)
	void printLogfln(const char *format, ...) __attribute__((format(printf, 1, 2)));
}

#endif

#endif