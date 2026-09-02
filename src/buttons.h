#pragma once
// Debounced edge detection for the board's two buttons. Both are wired to
// ground, so they read LOW when held.

#include <stdint.h>

namespace buttons {

void begin();

// Poll from loop(). Each returns true exactly once per press, on the press edge.
bool cityPressed();
bool rangePressed();

}  // namespace buttons
