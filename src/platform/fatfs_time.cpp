/*
 * Picocalc_Clock - FatFS timestamp provider backed by the DS3231 RTC.
 * Copyright (c) 2026 Fuyuki Yoneyama
 * SPDX-License-Identifier: MIT
 */

#include "ds3231.h"
#include "ff.h"
#include "picocalc_clock_build_info.h"

#include "hardware/i2c.h"

namespace {

bool datetime_valid_for_fatfs(const ds3231_datetime_t& dt) {
    return dt.year >= 1980 &&
           dt.year <= 2107 &&
           dt.month >= 1 &&
           dt.month <= 12 &&
           dt.day >= 1 &&
           dt.day <= 31 &&
           dt.hour <= 23 &&
           dt.minute <= 59 &&
           dt.second <= 59;
}

DWORD datetime_to_fattime(const ds3231_datetime_t& dt) {
    return ((DWORD)(dt.year - 1980) << 25) |
           ((DWORD)dt.month << 21) |
           ((DWORD)dt.day << 16) |
           ((DWORD)dt.hour << 11) |
           ((DWORD)dt.minute << 5) |
           ((DWORD)(dt.second / 2));
}

bool parse_two_digits(const char* text, uint8_t* value) {
    if (text[0] < '0' || text[0] > '9' ||
        text[1] < '0' || text[1] > '9') {
        return false;
    }
    *value = static_cast<uint8_t>((text[0] - '0') * 10 + (text[1] - '0'));
    return true;
}

bool parse_four_digits(const char* text, uint16_t* value) {
    uint16_t result = 0;
    for (int i = 0; i < 4; ++i) {
        if (text[i] < '0' || text[i] > '9') {
            return false;
        }
        result = static_cast<uint16_t>(result * 10 + (text[i] - '0'));
    }
    *value = result;
    return true;
}

DWORD build_time_fattime() {
    const char* text = PICOCALC_CLOCK_BUILD_TIME;
    ds3231_datetime_t dt = {};
    if (!parse_four_digits(&text[0], &dt.year) ||
        text[4] != '-' ||
        !parse_two_digits(&text[5], &dt.month) ||
        text[7] != '-' ||
        !parse_two_digits(&text[8], &dt.day) ||
        text[10] != ' ' ||
        !parse_two_digits(&text[11], &dt.hour) ||
        text[13] != ':' ||
        !parse_two_digits(&text[14], &dt.minute) ||
        text[16] != ':' ||
        !parse_two_digits(&text[17], &dt.second) ||
        !datetime_valid_for_fatfs(dt)) {
        return ((DWORD)(2020 - 1980) << 25) |
               ((DWORD)1 << 21) |
               ((DWORD)1 << 16);
    }
    return datetime_to_fattime(dt);
}

}  // namespace

extern "C" DWORD get_fattime(void) {
    ds3231_datetime_t dt = {};
    if (!ds3231_read_time(i2c1, &dt) || !datetime_valid_for_fatfs(dt)) {
        return build_time_fattime();
    }
    return datetime_to_fattime(dt);
}
