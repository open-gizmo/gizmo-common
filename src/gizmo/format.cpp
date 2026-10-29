#include "format.h"

#include <math.h>
#include <stdio.h>

namespace {
	// Largest supported decimals (more would overflow the 64-bit rounding)
	constexpr int MAX_FORMAT_DECIMALS = 9;
}

namespace gizmo {
	// Format seconds as MM:SS (minutes grow past two digits instead of being cut off)
	void formatDuration(uint32_t totalSeconds, char *buffer, size_t bufferSize) {
		const uint32_t minutes = totalSeconds / 60;
		const uint32_t seconds = totalSeconds % 60;
		snprintf(buffer, bufferSize, "%02lu:%02lu", static_cast<unsigned long>(minutes), static_cast<unsigned long>(seconds));
	}

	// Choose decimals so small values keep their significant digits (2 from 1 up, 0.08123 below with the defaults)
	int getSignificantDecimals(double value, int significantDigits, int minDecimals, int maxDecimals) {
		const double magnitude = fabs(value);
		if (!isfinite(magnitude) || magnitude >= 1.0 || magnitude == 0.0) return minDecimals;

		// Count the zeros right after the decimal point (0.08 has one), then add the significant digits
		const int leadingZeroCount = -static_cast<int>(floor(log10(magnitude))) - 1;
		const int decimals = leadingZeroCount + significantDigits;

		// Keep the result within the requested range
		if (decimals < minDecimals) return minDecimals;
		return decimals > maxDecimals ? maxDecimals : decimals;
	}

	// Format a number with thousands separators, cutting it off safely if the buffer is too small
	void formatNumber(double value, int decimals, NumberFormat numberFormat, char *output, size_t length) {
		// Nothing to write into
		if (length == 0) return;

		// Choose the separators and normalize the input (values that aren't numbers are shown as zero)
		const char decimalSeparator = numberFormat == NumberFormat::US ? '.' : ',';
		const char thousandSeparator = numberFormat == NumberFormat::US ? ',' : '.';
		const double finiteValue = isfinite(value) ? value : 0.0;
		const int safeDecimals = decimals < 0 ? 0 : (decimals > MAX_FORMAT_DECIMALS ? MAX_FORMAT_DECIMALS : decimals);

		// Round to the requested decimals as an integer
		uint64_t scaleFactor = 1;
		for (int index = 0; index < safeDecimals; index++) {
			scaleFactor *= 10;
		}
		const int64_t roundedValue = llround(finiteValue * static_cast<double>(scaleFactor));
		const bool negative = roundedValue < 0;
		const uint64_t magnitude = negative
			? static_cast<uint64_t>(-(roundedValue + 1)) + 1
			: static_cast<uint64_t>(roundedValue);

		// Convert the rounded absolute value into a raw digit string
		char rawDigits[24];
		snprintf(rawDigits, sizeof(rawDigits), "%llu", static_cast<unsigned long long>(magnitude));

		// Determine how many digits belong to the integer and decimal parts
		size_t rawLength = 0;
		while (rawDigits[rawLength] != '\0') {
			rawLength += 1;
		}
		const size_t integerDigits = rawLength > static_cast<size_t>(safeDecimals)
			? rawLength - static_cast<size_t>(safeDecimals)
			: 0;
		size_t writeIndex = 0;

		// Append a character only when there is enough room left in the output buffer
		auto appendChar = [&](char character) {
			if (writeIndex + 1 >= length) {
				return;
			}
			output[writeIndex++] = character;
		};

		// Add the negative sign when the rounded value is below zero
		if (negative) {
			appendChar('-');
		}

		// Write the integer portion with thousands separators
		if (integerDigits == 0) {
			appendChar('0');
		} else {
			for (size_t index = 0; index < integerDigits; index++) {
				// Append the next digit
				appendChar(rawDigits[index]);

				// Append a thousands separator when appropriate
				const size_t digitsRemaining = integerDigits - index - 1;
				if (digitsRemaining > 0 && digitsRemaining % 3 == 0) {
					appendChar(thousandSeparator);
				}
			}
		}

		// Write the decimal portion, padding with zeros when needed
		if (safeDecimals > 0) {
			// Append the decimal separator
			appendChar(decimalSeparator);

			// Append leading zeros if the raw value has fewer digits than the requested decimals
			const size_t zeroPadding = rawLength < static_cast<size_t>(safeDecimals)
				? static_cast<size_t>(safeDecimals) - rawLength
				: 0;
			for (size_t index = 0; index < zeroPadding; index++) {
				appendChar('0');
			}

			// Append the existing decimal digits from the raw value
			const size_t decimalStart = rawLength > static_cast<size_t>(safeDecimals)
				? rawLength - static_cast<size_t>(safeDecimals)
				: 0;
			for (size_t index = decimalStart; index < rawLength; index++) {
				appendChar(rawDigits[index]);
			}
		}

		// Terminate the output
		output[writeIndex] = '\0';
	}
}