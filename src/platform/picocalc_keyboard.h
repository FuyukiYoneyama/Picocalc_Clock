/*
 * Picocalc_ment - standalone musical instrument firmware for PicoCalc.
 * Copyright (c) 2026 Fuyuki Yoneyama
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <stdint.h>

namespace picoment::keyboard {

enum class KeyState : uint8_t {
    Idle = 0,
    Pressed = 1,
    Hold = 2,
    Released = 3,
};

struct KeyEvent {
    KeyState state;
    uint8_t key;
};

void init();
bool read_event(KeyEvent* event);
bool read_lcd_backlight(uint8_t* value);
bool write_lcd_backlight(uint8_t value);
uint32_t read_count();
uint32_t error_count();
uint32_t empty_count();

}  // namespace picoment::keyboard
