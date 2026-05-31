#pragma once

#include "alarm/alarm_model.h"
#include "ds3231.h"
#include "platform/battery.h"

void draw_calendar_clock(const ds3231_datetime_t& dt,
                         bool rtc_ok,
                         const BatteryStatus& battery,
                         const AlarmSettings* alarms,
                         const AlarmFireRecord& last_fire,
                         bool show_seconds,
                         bool force_full_redraw,
                         char* previous_date,
                         char* previous_time,
                         char* previous_moon,
                         char* previous_battery,
                         char* previous_alarm);
