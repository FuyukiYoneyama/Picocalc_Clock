#ifndef PICOCALC_CLOCK_CLOCK_TIME_H
#define PICOCALC_CLOCK_CLOCK_TIME_H

#include <cstddef>
#include <cstdint>

#include "ds3231.h"

bool is_leap_year(int year);
uint8_t days_in_month(int year, int month);
int weekday_from_date(int year, int month, int day);
const char* weekday_name(int weekday);
int days_since_2000_01_01(int year, int month, int day);
bool is_valid_datetime(const ds3231_datetime_t& dt);
int moon_age_tenths(const ds3231_datetime_t& dt);
void format_clock_lines(const ds3231_datetime_t& dt,
                        bool rtc_ok,
                        bool show_seconds,
                        char* date_line,
                        size_t date_len,
                        char* time_line,
                        size_t time_len);
void format_moon_age_line(const ds3231_datetime_t& dt,
                          bool rtc_ok,
                          char* moon_line,
                          size_t moon_len);

#endif
