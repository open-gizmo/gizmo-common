#include "text.h"

#include <string.h>

namespace gizmo {
	// Copy a text into a buffer, cutting it to fit and always ending it (returns false if it had to be cut; a null text copies as empty)
	bool copyString(char *destination, const char *text, size_t destinationSize) {
		if (destination == nullptr || destinationSize == 0) {
			return text == nullptr || text[0] == '\0';
		}

		// A null text copies as an empty one
		if (text == nullptr) {
			destination[0] = '\0';
			return true;
		}

		// Copy what fits, and end the text
		const size_t textLength = strlen(text);
		const size_t copyLength = textLength < destinationSize ? textLength : destinationSize - 1;
		memmove(destination, text, copyLength);
		destination[copyLength] = '\0';

		// Report whether all of it fit
		return copyLength == textLength;
	}
}