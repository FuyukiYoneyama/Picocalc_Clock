#ifndef PICOCALC_CLOCK_KEYMAP_H
#define PICOCALC_CLOCK_KEYMAP_H

#include <stdint.h>

#include "platform/picocalc_key_table.h"

enum class ClockKey : uint8_t {
    None = 0,
    Enter,
    Escape,
    Up,
    Down,
    Left,
    Right,
    Space,
    Digit,
    Other,
};

inline ClockKey clock_key_from_raw(uint8_t raw) {
    if (raw >= '0' && raw <= '9') {
        return ClockKey::Digit;
    }

    switch (raw) {
    case picoment::keys::Enter:
        return ClockKey::Enter;
    case picoment::keys::Escape:
        return ClockKey::Escape;
    case picoment::keys::Up:
        return ClockKey::Up;
    case picoment::keys::Down:
        return ClockKey::Down;
    case picoment::keys::Left:
        return ClockKey::Left;
    case picoment::keys::Right:
        return ClockKey::Right;
    case picoment::keys::Space:
        return ClockKey::Space;
    case picoment::keys::None:
        return ClockKey::None;
    default:
        return ClockKey::Other;
    }
}

inline const char *clock_key_name(ClockKey key) {
    switch (key) {
    case ClockKey::None:
        return "None";
    case ClockKey::Enter:
        return "Enter";
    case ClockKey::Escape:
        return "Esc";
    case ClockKey::Up:
        return "Up";
    case ClockKey::Down:
        return "Down";
    case ClockKey::Left:
        return "Left";
    case ClockKey::Right:
        return "Right";
    case ClockKey::Space:
        return "Space";
    case ClockKey::Digit:
        return "Digit";
    case ClockKey::Other:
        return "Other";
    default:
        return "Invalid";
    }
}

#endif
