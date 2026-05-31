#ifndef PICOCALC_CLOCK_PLATFORM_BACKLIGHT_CONTROL_H
#define PICOCALC_CLOCK_PLATFORM_BACKLIGHT_CONTROL_H

#include <cstdint>

constexpr uint8_t kDefaultRestoreBacklight = 32;

struct BacklightState {
    bool user_off;
    bool space_peek_active;
    bool alarm_forced_on;
    uint8_t restore_level;
};

void remember_restore_backlight(BacklightState* state);
bool backlight_turn_on(BacklightState* state);
bool backlight_turn_off(BacklightState* state);
bool backlight_cancel_user_off(BacklightState* state);
void backlight_alarm_started(BacklightState* state);
void backlight_alarm_stopped(BacklightState* state);

#endif
