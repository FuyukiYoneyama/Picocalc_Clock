#include "clock/calendar_render.h"

#include <cstdio>
#include <cstring>

#include "clock/clock_render.h"
#include "clock/clock_time.h"
#include "font/cozette_font.h"
#include "platform/picocalc_display.h"

namespace {

constexpr uint16_t kBlack = 0x0000;
constexpr uint16_t kWhite = 0xffff;
constexpr uint16_t kDim = 0x7bef;
constexpr uint16_t kWarn = 0xfde0;
constexpr uint16_t kSecondHand = 0xf800;
constexpr uint16_t kCalendarHighlight = 0x07ff;
constexpr uint16_t kHighlightText = 0x0000;
constexpr int kTimeCharW = 32;
constexpr int kTimeH = 64;
constexpr int kCalendarHeaderY = 32;
constexpr int kCalendarWeekdayY = 60;
constexpr int kCalendarGridX = 6;
constexpr int kCalendarGridY = 80;
constexpr int kCalendarCellW = 44;
constexpr int kCalendarCellH = 20;
constexpr int kCalendarMoonY = 202;
constexpr int kCalendarTimeY = 220;
constexpr int kCalendarAlarmY = 284;
constexpr int kCalendarAlarmH = 24;

void draw_calendar_header_delta(const ds3231_datetime_t& dt,
                                bool rtc_ok,
                                char* previous_date,
                                char* previous_moon) {
    char month_text[16];
    char moon_text[20];
    if (rtc_ok) {
        std::snprintf(month_text, sizeof(month_text), "%04u-%02u",
                      dt.year, dt.month);
    } else {
        std::snprintf(month_text, sizeof(month_text), "---- --");
    }
    format_moon_age_line(dt, rtc_ok, moon_text, sizeof(moon_text));

    if (std::strncmp(month_text, previous_date, std::strlen(month_text)) != 0) {
        picoment::display::fill_rect(8, kCalendarHeaderY, 128, 24, kBlack);
        picoment::display::draw_spleen_native_text_band(
            8, kCalendarHeaderY, 128, 24, month_text,
            picoment::font::SpleenNativeSize::S12x24,
            rtc_ok ? kWhite : kWarn, kBlack);
    }

    if (std::strcmp(moon_text, previous_moon) != 0) {
        constexpr int kMoonBandW = 96;
        const int text_w =
            static_cast<int>(std::strlen(moon_text)) * picoment::font::kCozetteWidth;
        const int band_x =
            kCalendarGridX + kCalendarCellW * 7 - kMoonBandW;
        const int text_x = band_x + kMoonBandW - text_w;
        picoment::display::fill_rect(band_x, kCalendarMoonY,
                                     kMoonBandW, 16, kBlack);
        picoment::display::draw_text_band(text_x, kCalendarMoonY,
                                          text_w, 16, moon_text,
                                          rtc_ok ? kDim : kWarn, kBlack);
        std::snprintf(previous_moon, 20, "%s", moon_text);
    }
}

void draw_calendar_month(const ds3231_datetime_t& dt, bool rtc_ok) {
    picoment::display::fill_rect(0, kCalendarWeekdayY,
                                 picoment::display::kScreenWidth,
                                 kCalendarGridY + kCalendarCellH * 6 -
                                     kCalendarWeekdayY,
                                 kBlack);

    static constexpr const char* kWeekdays[7] = {
        "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat",
    };
    for (int col = 0; col < 7; ++col) {
        picoment::display::draw_text_band(
            kCalendarGridX + col * kCalendarCellW + 8,
            kCalendarWeekdayY,
            28, 16, kWeekdays[col],
            col == 0 ? kSecondHand : kDim,
            kBlack);
    }

    const int grid_w = kCalendarCellW * 7;
    const int grid_h = kCalendarCellH * 6;
    for (int col = 0; col <= 7; ++col) {
        const int x = kCalendarGridX + col * kCalendarCellW;
        picoment::display::draw_line(x, kCalendarGridY,
                                     x, kCalendarGridY + grid_h,
                                     kDim);
    }
    for (int row = 0; row <= 6; ++row) {
        const int y = kCalendarGridY + row * kCalendarCellH;
        picoment::display::draw_line(kCalendarGridX, y,
                                     kCalendarGridX + grid_w, y,
                                     kDim);
    }

    if (!rtc_ok) {
        picoment::display::draw_text_band(
            104, kCalendarGridY + 48, 112, 16,
            "RTC ----", kWarn, kBlack);
        return;
    }

    const int first_weekday =
        weekday_from_date(dt.year, dt.month, 1) % 7;
    const int days = days_in_month(dt.year, dt.month);
    for (int day = 1; day <= days; ++day) {
        const int cell = first_weekday + day - 1;
        const int row = cell / 7;
        const int col = cell % 7;
        const int x = kCalendarGridX + col * kCalendarCellW;
        const int y = kCalendarGridY + row * kCalendarCellH;
        const bool today = day == dt.day;
        char text[4];
        std::snprintf(text, sizeof(text), "%2d", day);
        if (today) {
            picoment::display::fill_rect(x + 1, y + 1,
                                         kCalendarCellW - 1,
                                         kCalendarCellH - 1,
                                         kCalendarHighlight);
        }
        picoment::display::draw_text_band(
            x + 14, y + 3, 20, 16, text,
            today ? kHighlightText : kWhite,
            today ? kCalendarHighlight : kBlack);
    }
}

void draw_calendar_time_delta(const ds3231_datetime_t& dt,
                              bool rtc_ok,
                              bool show_seconds,
                              char* previous_time) {
    char text[12];
    if (rtc_ok) {
        if (show_seconds) {
            std::snprintf(text, sizeof(text), "%02u:%02u:%02u",
                          dt.hour, dt.minute, dt.second);
        } else {
            std::snprintf(text, sizeof(text), "%02u:%02u",
                          dt.hour, dt.minute);
        }
    } else {
        std::snprintf(text, sizeof(text), show_seconds ? "--:--:--" : "--:--");
    }

    if (std::strcmp(text, previous_time) == 0) {
        return;
    }

    const int len = static_cast<int>(std::strlen(text));
    const int text_x = (picoment::display::kScreenWidth - len * kTimeCharW) / 2;
    if (std::strlen(previous_time) != std::strlen(text)) {
        picoment::display::fill_rect(0, kCalendarTimeY,
                                     picoment::display::kScreenWidth,
                                     kTimeH, kBlack);
    }
    for (int i = 0; i < len; ++i) {
        if (text[i] == previous_time[i]) {
            continue;
        }
        char ch[2] = {text[i], '\0'};
        picoment::display::draw_spleen_native_text_band(
            text_x + i * kTimeCharW, kCalendarTimeY,
            kTimeCharW, kTimeH, ch,
            picoment::font::SpleenNativeSize::S32x64,
            rtc_ok ? kWhite : kWarn, kBlack);
    }
    std::snprintf(previous_time, 9, "%s", text);
}

void draw_calendar_alarm_delta(const AlarmSettings* alarms,
                               const ds3231_datetime_t& dt,
                               const AlarmFireRecord& last_fire,
                               char* previous_alarm,
                               bool rtc_ok) {
    char text[24];
    if (rtc_ok) {
        const AlarmMatch next = find_next_alarm(alarms, dt, last_fire);
        format_alarm_label(next, text, sizeof(text));
    } else {
        std::snprintf(text, sizeof(text), "Next -- --:--");
    }
    if (std::strcmp(text, previous_alarm) == 0) {
        return;
    }

    const int text_w = static_cast<int>(std::strlen(text)) * 12;
    const int text_x = (picoment::display::kScreenWidth - text_w) / 2;
    picoment::display::fill_rect(0, kCalendarAlarmY,
                                 picoment::display::kScreenWidth,
                                 kCalendarAlarmH, kBlack);
    picoment::display::draw_spleen_native_text_band(
        text_x, kCalendarAlarmY, text_w, kCalendarAlarmH, text,
        picoment::font::SpleenNativeSize::S12x24,
        rtc_ok ? kDim : kWarn, kBlack);
    std::snprintf(previous_alarm, 24, "%s", text);
}

}  // namespace

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
                         char* previous_alarm) {
    if (force_full_redraw) {
        draw_clock_frame();
        previous_date[0] = '\0';
        std::snprintf(previous_time, 9, "        ");
        previous_moon[0] = '\0';
        previous_battery[0] = '\0';
        previous_alarm[0] = '\0';
    }

    char date_key[16];
    if (rtc_ok) {
        std::snprintf(date_key, sizeof(date_key), "%04u-%02u-%02u",
                      dt.year, dt.month, dt.day);
    } else {
        std::snprintf(date_key, sizeof(date_key), "---- -- --");
    }
    draw_calendar_header_delta(dt, rtc_ok, previous_date, previous_moon);
    if (std::strcmp(date_key, previous_date) != 0) {
        draw_calendar_month(dt, rtc_ok);
        std::snprintf(previous_date, 40, "%s", date_key);
    }
    draw_battery_delta(battery, previous_battery);
    draw_calendar_time_delta(dt, rtc_ok, show_seconds, previous_time);
    draw_calendar_alarm_delta(alarms, dt, last_fire,
                              previous_alarm, rtc_ok);
}
