#ifndef GIZMO_TEXT_H
#define GIZMO_TEXT_H

#include <stddef.h>

namespace gizmo {
	// Copy a text into a buffer, cutting it to fit and always ending it (returns false if it had to be cut; a null text copies as empty)
	bool copyString(char *destination, const char *text, size_t destinationSize);
}

#endif