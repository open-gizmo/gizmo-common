#ifndef GIZMO_FORMAT_H
#define GIZMO_FORMAT_H

#include <stddef.h>
#include <stdint.h>

namespace gizmo {
	// Buffer size that fits any duration as MM:SS (up to "71582788:15" for UINT32_MAX seconds, plus the terminator)
	constexpr size_t DURATION_TEXT_SIZE = 12;

	// Number separator styles
	enum class NumberFormat : uint8_t {
		US, // 1,234.56
		EU, // 1.234,56
	};

	// Format seconds as MM:SS (minutes grow past two digits instead of being cut off)
	void formatDuration(uint32_t totalSeconds, char *buffer, size_t bufferSize);

	// Choose decimals so small values keep their significant digits (2 from 1 up, 0.08123 below with the defaults)
	int getSignificantDecimals(double value, int significantDigits = 4, int minDecimals = 2, int maxDecimals = 8);

	// Format a number with thousands separators, cutting it off safely if the buffer is too small
	void formatNumber(double value, int decimals, NumberFormat numberFormat, char *output, size_t length);
}

#endif