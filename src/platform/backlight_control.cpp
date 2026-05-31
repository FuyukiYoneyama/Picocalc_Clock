#include "platform/backlight_control.h"

#include <cstdio>

#include "platform/picocalc_keyboard.h"

namespace {

bool write_lcd_backlight_checked(uint8_t value) {
    if (!picoment::keyboard::write_lcd_backlight(value)) {
        std::printf("BACKLIGHT write fail value=%u\r\n", value);
        return false;
    }
    return true;
}

}  // namespace

void remember_restore_backlight(BacklightState* state) {
    uint8_t current = 0;
    if (picoment::keyboard::read_lcd_backlight(&current) && current != 0) {
        state->restore_level = current;
    }
    if (state->restore_level == 0) {
        state->restore_level = kDefaultRestoreBacklight;
    }
}

bool backlight_turn_on(BacklightState* state) {
    if (state->restore_level == 0) {
        state->restore_level = kDefaultRestoreBacklight;
    }
    return write_lcd_backlight_checked(state->restore_level);
}

bool backlight_turn_off(BacklightState* state) {
    remember_restore_backlight(state);
    return write_lcd_backlight_checked(0);
}

bool backlight_cancel_user_off(BacklightState* state) {
    if (!backlight_turn_on(state)) {
        return false;
    }
    state->user_off = false;
    state->space_peek_active = false;
    state->alarm_forced_on = false;
    return true;
}

void backlight_alarm_started(BacklightState* state) {
    if (!state->user_off) {
        return;
    }
    if (backlight_turn_on(state)) {
        state->alarm_forced_on = true;
        state->space_peek_active = false;
        std::puts("BACKLIGHT alarm=on");
    }
}

void backlight_alarm_stopped(BacklightState* state) {
    if (state->user_off && state->alarm_forced_on) {
        if (backlight_turn_off(state)) {
            std::puts("BACKLIGHT alarm=restore-off");
            state->alarm_forced_on = false;
            state->space_peek_active = false;
        }
        return;
    }
    state->alarm_forced_on = false;
    state->space_peek_active = false;
}
