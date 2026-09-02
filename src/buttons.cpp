#include "buttons.h"

#include <Arduino.h>

#include "config.h"

namespace buttons {

namespace {

struct Button {
    int pin;
    bool stableDown;
    bool lastRead;
    uint32_t changedAt;
};

Button gCity = {PIN_BTN_CITY, false, false, 0};
Button gRange = {PIN_BTN_RANGE, false, false, 0};

// Returns true on the transition from released to held, once the reading has
// been steady for the debounce window.
bool poll(Button& b) {
    bool down = (digitalRead(b.pin) == LOW);
    uint32_t now = millis();
    if (down != b.lastRead) {
        b.lastRead = down;
        b.changedAt = now;
        return false;
    }
    if (now - b.changedAt < BUTTON_DEBOUNCE_MS) return false;
    if (down == b.stableDown) return false;
    b.stableDown = down;
    return down;
}

}  // namespace

void begin() {
    pinMode(gCity.pin, INPUT_PULLUP);
    pinMode(gRange.pin, INPUT_PULLUP);
}

bool cityPressed() { return poll(gCity); }
bool rangePressed() { return poll(gRange); }

}  // namespace buttons
