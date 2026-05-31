#pragma once

#include <stdint.h>

#include "alarm/alarm_model.h"
#include "ds3231.h"
#include "platform/battery.h"

struct AnalogHandState {
    bool valid;
    bool rtc_ok;
    bool show_second;
    uint8_t hour_index;
    uint8_t minute_index;
    uint8_t second_index;
};

void draw_analog_clock(const ds3231_datetime_t& dt,
                       bool rtc_ok,
                       const BatteryStatus& battery,
                       const AlarmSettings* alarms,
                       const AlarmFireRecord& last_fire,
                       bool show_seconds,
                       bool force_full_redraw,
                       char* previous_date,
                       char* previous_moon,
                       char* previous_battery,
                       char* previous_alarm,
                       AnalogHandState* previous_hand_state);
