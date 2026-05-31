#include "clock/clock_time.h"

#include <cstdio>

bool is_leap_year(int year) {
    return ((year % 4) == 0 && (year % 100) != 0) || ((year % 400) == 0);
}

uint8_t days_in_month(int year, int month) {
    static constexpr uint8_t kMonthDays[] = {
        31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
    };
    if (month < 1 || month > 12) {
        return 0;
    }
    if (month == 2 && is_leap_year(year)) {
        return 29;
    }
    return kMonthDays[month - 1];
}

namespace {

int days_before_month(int year, int month) {
    static constexpr int kDays[] = {
        0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334
    };
    int days = kDays[month - 1];
    if (month > 2 && is_leap_year(year)) {
        ++days;
    }
    return days;
}

}  // namespace

int weekday_from_date(int year, int month, int day) {
    const int y = year - 1;
    const int days_before_year = y * 365 + y / 4 - y / 100 + y / 400;
    const int ordinal = days_before_year + days_before_month(year, month) + day;
    return ordinal % 7;
}

const char* weekday_name(int weekday) {
    static constexpr const char* kNames[] = {
        "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
    };
    if (weekday < 0 || weekday > 6) {
        return "---";
    }
    return kNames[weekday];
}

int days_since_2000_01_01(int year, int month, int day) {
    int days = 0;
    for (int y = 2000; y < year; ++y) {
        days += is_leap_year(y) ? 366 : 365;
    }
    return days + days_before_month(year, month) + day - 1;
}

bool is_valid_datetime(const ds3231_datetime_t& dt) {
    if (dt.year < 2000 || dt.year > 2099 || dt.month < 1 || dt.month > 12 ||
        dt.day < 1 || dt.hour > 23 || dt.minute > 59 || dt.second > 59) {
        return false;
    }

    return dt.day <= days_in_month(dt.year, dt.month);
}

int moon_age_tenths(const ds3231_datetime_t& dt) {
    constexpr int kNewMoonEpochMinutes = 6 * 1440 + 3 * 60 + 14;
    constexpr int kSynodicMonthMinutes = 42524;
    const int days = days_since_2000_01_01(dt.year, dt.month, dt.day);
    int phase_minutes =
        (days * 1440 + dt.hour * 60 + dt.minute - kNewMoonEpochMinutes) %
        kSynodicMonthMinutes;
    if (phase_minutes < 0) {
        phase_minutes += kSynodicMonthMinutes;
    }
    int age = (phase_minutes * 10 + 720) / 1440;
    if (age >= 295) {
        age = 0;
    }
    return age;
}

void format_clock_lines(const ds3231_datetime_t& dt,
                        bool rtc_ok,
                        bool show_seconds,
                        char* date_line,
                        size_t date_len,
                        char* time_line,
                        size_t time_len) {

    if (rtc_ok) {
        const int weekday = weekday_from_date(dt.year, dt.month, dt.day);
        std::snprintf(date_line, date_len, "%04u-%02u-%02u %s",
                      dt.year, dt.month, dt.day, weekday_name(weekday));
        if (show_seconds) {
            std::snprintf(time_line, time_len, "%02u:%02u:%02u",
                          dt.hour, dt.minute, dt.second);
        } else {
            std::snprintf(time_line, time_len, "%02u:%02u",
                          dt.hour, dt.minute);
        }
    } else {
        std::snprintf(date_line, date_len, "---- -- -- ---");
        std::snprintf(time_line, time_len, show_seconds ? "--:--:--" : "--:--");
    }
}

void format_moon_age_line(const ds3231_datetime_t& dt,
                          bool rtc_ok,
                          char* moon_line,
                          size_t moon_len) {
    if (!rtc_ok) {
        std::snprintf(moon_line, moon_len, "Moon --.-d");
        return;
    }

    const int age = moon_age_tenths(dt);
    std::snprintf(moon_line, moon_len, "Moon %02d.%01dd", age / 10, age % 10);
}
