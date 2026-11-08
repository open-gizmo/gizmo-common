#include "log.h"

// Serial logging, only available on Arduino targets (native test builds skip it)
#ifdef ARDUINO

#include <cstdarg>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

namespace {
	// One shared buffer for formatted log lines (a template version used one buffer per call signature)
	char logBuffer[256];

	// Whether logging is enabled
	bool logEnabled = false;

	// Guard the shared buffer, so log lines from different tasks can't mix
	SemaphoreHandle_t logMutex = nullptr;

	// Format a log line into the shared buffer and print it, with or without a newline
	void printFormattedLog(bool addNewline, const char *format, va_list arguments) {
		// Skip everything when logging is off or the mutex is missing
		if (!logEnabled || logMutex == nullptr) {
			return;
		}

		// Format and print the line while holding the buffer
		xSemaphoreTake(logMutex, portMAX_DELAY);
		vsnprintf(logBuffer, sizeof(logBuffer), format, arguments);
		if (addNewline) {
			Serial.println(logBuffer);
		} else {
			Serial.print(logBuffer);
		}
		xSemaphoreGive(logMutex);
	}
}

// Debug builds set PIO_DEV_MODE=1 (PlatformIO passes a project's build flags to its libraries too)
#ifndef PIO_DEV_MODE
#define PIO_DEV_MODE 0
#endif

// How long the log waits at boot for a USB host to open the serial port
constexpr unsigned long SERIAL_WAIT_MS = 1500;

namespace gizmo {
	// Start logging (call first in setup, before any other task can log)
	void initLog(bool isEnabled, uint32_t baudRate) {
		// Create the log mutex before any task can log
		logMutex = xSemaphoreCreateMutex();
		logEnabled = isEnabled;

		// With logging off, silence ESP-IDF's own messages too (its Wi-Fi and system libraries print their errors to the console, which the chip's USB port mirrors)
		if (!logEnabled) {
			esp_log_level_set("*", ESP_LOG_NONE);
			return;
		}

		// Start the serial port
		Serial.begin(baudRate);
#if ARDUINO_USB_CDC_ON_BOOT && ARDUINO_USB_MODE
		Serial.setTxTimeoutMs(0); // The chip's own USB: drop log lines when no one reads them, instead of waiting for room (it slowed the loop on a computer with no serial monitor open)
#endif

		// Give a USB host a moment to open the port, so the boot logs reach it, but never wait for one: on boards whose Serial is the chip's own USB (ESP32-S3, C3), it stays closed on a charger, and waiting for it stopped the device at boot
		const unsigned long startedAt = millis();
		while (!Serial && millis() - startedAt < SERIAL_WAIT_MS) {
			delay(10);
		}
	}

	// Whether the build is a debug one
	bool isDevMode() {
		return PIO_DEV_MODE != 0;
	}

	// Start logging in debug builds only
	void initLog() {
		initLog(isDevMode());
	}

	// Check whether logging is enabled
	bool isLogEnabled() {
		return logEnabled;
	}

	// Printf-style log with format string
	void printLogf(const char *format, ...) {
		va_list arguments;
		va_start(arguments, format);
		printFormattedLog(false, format, arguments);
		va_end(arguments);
	}

	// Printf-style log with format string and newline
	void printLogfln(const char *format, ...) {
		va_list arguments;
		va_start(arguments, format);
		printFormattedLog(true, format, arguments);
		va_end(arguments);
	}
}

#endif