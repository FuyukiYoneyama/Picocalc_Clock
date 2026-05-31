#ifndef PICOCALC_CLOCK_CLOCK_RENDER_H
#define PICOCALC_CLOCK_CLOCK_RENDER_H

#include "alarm/alarm_model.h"
#include "ds3231.h"
#include "platform/battery.h"

void draw_clock_frame();
void draw_moon_age_delta(const char* moon_line,
                         char* previous_moon,
                         bool rtc_ok);
void draw_clock_delta(const char* date_line,
                      const char* time_line,
                      char* previous_date,
                      char* previous_time,
                      bool rtc_ok);
void draw_no_seconds_colon(bool visible,
                           bool rtc_ok,
                           char* previous_time);
void format_battery_text(const BatteryStatus& battery, char* text, size_t len);
void draw_battery_delta(const BatteryStatus& battery, char* previous_battery);
void draw_alarm_delta(const AlarmSettings* alarms,
                      const ds3231_datetime_t& dt,
                      const AlarmFireRecord& last_fire,
                      char* previous_alarm);

#endif
