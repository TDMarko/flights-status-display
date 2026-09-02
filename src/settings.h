#pragma once
// Persists the two things the buttons change, so the radar comes back up
// pointing where you left it.

#include <stdint.h>

namespace settings {

void begin();

int cityIndex();
int rangeIndex();

// Both wrap around and write through to flash.
void nextCity();
void nextRange();

}  // namespace settings
