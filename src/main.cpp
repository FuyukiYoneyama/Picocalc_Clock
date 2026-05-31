#include <cstdio>
#include <cstring>
#include <cstdlib>

#include "hardware/gpio.h"
#include "hardware/i2c.h"
#include "pico/stdlib.h"

#include "alarm_sound.h"
#include "alarm/alarm_model.h"
#include "alarm/alarm_ui.h"
#include "app/screenshot_service.h"
#include "app/settings_editor.h"
#include "app/set_time_editor.h"
#include "app/uart_commands.h"
#include "clock/clock_help.h"
#include "clock/clock_render.h"
#include "clock/clock_time.h"
#include "ds3231.h"
#include "font/cozette_font.h"
#include "life/life_runtime.h"
#include "picocalc_clock_build_info.h"
#include "platform/backlight_control.h"
#include "platform/battery.h"
#include "platform/picocalc_audio_pwm.h"
#include "platform/picocalc_display.h"
#include "platform/picocalc_key_table.h"
#include "platform/picocalc_keyboard.h"
#include "platform/startup_probe.h"
#include "settings/settings_store.h"
#include "version.h"

#define CLOCK_I2C_PORT i2c1
#define CLOCK_I2C_SDA_PIN 6
#define CLOCK_I2C_SCL_PIN 7
#define CLOCK_I2C_SPEED_HZ 400000
#define I2C_ADDR_KEYBOARD 0x1F
#define I2C_ADDR_AT24C32_EXPECTED 0x57
#define I2C_SCAN_TIMEOUT_US 10000

namespace {

constexpr uint16_t kBlack = 0x0000;
constexpr uint16_t kWhite = 0xffff;
constexpr uint16_t kDim = 0x7bef;
constexpr uint16_t kWarn = 0xfde0;
constexpr uint16_t kSecondHand = 0xf800;
constexpr uint16_t kHighlight = 0xff80;
constexpr uint16_t kHighlightDigit = 0x07ff;
constexpr uint16_t kCalendarHighlight = 0x07ff;
constexpr uint16_t kHighlightText = 0x0000;
constexpr uint32_t kRtcSearchPollMs = 47;
constexpr uint32_t kRtcRestAfterTickMs = 900;
constexpr uint32_t kRtcFailRetryMs = 200;
constexpr uint32_t kMainLoopActiveSleepMs = 10;
constexpr uint32_t kMainLoopMaxSleepMs = 900;
constexpr uint32_t kUiSleepCapMs = 20;
constexpr uint32_t kClockIdleKeySlowAfterMs = 60000;
constexpr uint32_t kClockIdleKeySleepCapMs = 100;
constexpr uint32_t kBatteryReadIntervalMs = 60000;
constexpr uint32_t kAlarmLoopSleepMs = 2;
constexpr uint32_t kAlarmAutoStopMs = 60000;
constexpr uint32_t kColonBlinkMs = 1000;
constexpr uint32_t kLifeHourlyMaxMs = 60000;
constexpr uint32_t kLifeLoopSleepMs = 30;
constexpr int kDateBandX = 52;
constexpr int kDateY = 82;
constexpr int kDateBandW = 216;
constexpr int kDateH = 24;
constexpr int kTimeX = 32;
constexpr int kTimeY = 128;
constexpr int kTimeCharW = 32;
constexpr int kTimeH = 64;
constexpr int kTimeNoSecondsY = 112;
constexpr int kTimeNoSecondsCharW = 48;
constexpr int kTimeNoSecondsH = 96;
constexpr int kMoonBandX = 76;
constexpr int kMoonBandY = 212;
constexpr int kMoonBandW = 180;
constexpr int kMoonBandH = 24;
constexpr int kHeaderY = 12;
constexpr int kHeaderH = 18;
constexpr int kBatteryBandX = 214;
constexpr int kBatteryBandW = 96;
constexpr int kAlarmBandX = 78;
constexpr int kAlarmBandY = 246;
constexpr int kAlarmBandW = 164;
constexpr int kAlarmBandH = 24;
constexpr int kAnalogCenterX = 160;
constexpr int kAnalogCenterY = 168;
constexpr int kAnalogRadius = 96;
constexpr int kAnalogDateY = 31;
constexpr int kAnalogDateH = 24;
constexpr int kAnalogMoonX = 168;
constexpr int kAnalogMoonY = 53;
constexpr int kAnalogMoonW = 80;
constexpr int kAnalogMoonH = 16;
constexpr int kAnalogAlarmX = 40;
constexpr int kAnalogAlarmY = 274;
constexpr int kAnalogAlarmW = 240;
constexpr int kAnalogAlarmH = 24;
constexpr int kAnalogAmPmX = 148;
constexpr int kAnalogAmPmY = 206;
constexpr int kAnalogAmPmW = 24;
constexpr int kAnalogAmPmH = 24;
constexpr int kAnalogHubRadius = 4;
constexpr int kAnalogHourHandLength = kAnalogRadius * 50 / 100;
constexpr int kAnalogMinuteHandLength = kAnalogRadius * 72 / 100;
constexpr int kAnalogSecondHandLength = kAnalogRadius * 82 / 100;
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

static constexpr int16_t kSin60[60] = {
        0,   107,   213,   316,   416,   512,   602,   685,   761,   828,
      887,   935,   974,  1002,  1018,  1024,  1018,  1002,   974,   935,
      887,   828,   761,   685,   602,   512,   416,   316,   213,   107,
        0,  -107,  -213,  -316,  -416,  -512,  -602,  -685,  -761,  -828,
     -887,  -935,  -974, -1002, -1018, -1024, -1018, -1002,  -974,  -935,
     -887,  -828,  -761,  -685,  -602,  -512,  -416,  -316,  -213,  -107,
};

static constexpr int16_t kCos60[60] = {
     1024,  1018,  1002,   974,   935,   887,   828,   761,   685,   602,
      512,   416,   316,   213,   107,     0,  -107,  -213,  -316,  -416,
     -512,  -602,  -685,  -761,  -828,  -887,  -935,  -974, -1002, -1018,
    -1024, -1018, -1002,  -974,  -935,  -887,  -828,  -761,  -685,  -602,
     -512,  -416,  -316,  -213,  -107,     0,   107,   213,   316,   416,
      512,   602,   685,   761,   828,   887,   935,   974,  1002,  1018,
};

enum class UiMode {
    Clock,
    ClockHelp,
    SetTime,
    SetAlarm,
    SetSettings,
    Life,
    AlarmRinging,
};

struct AnalogHandState {
    bool valid;
    bool rtc_ok;
    bool show_second;
    uint8_t hour_index;
    uint8_t minute_index;
    uint8_t second_index;
};

bool time_reached(uint32_t now_ms, uint32_t target_ms) {
    return static_cast<int32_t>(now_ms - target_ms) >= 0;
}

uint32_t ms_until(uint32_t now_ms, uint32_t target_ms) {
    if (time_reached(now_ms, target_ms)) {
        return 0;
    }
    return target_ms - now_ms;
}

void print_build_id() {
    std::printf("\r\nPicocalc_Clock version %s build %s\r\n",
                PICOCALC_CLOCK_VERSION_STRING,
                PICOCALC_CLOCK_BUILD_PROFILE);
    std::printf("BUILD ID git=%s dirty=%u time=\"%s\" purpose=\"%s\"\r\n",
                PICOCALC_CLOCK_GIT_HASH,
                PICOCALC_CLOCK_GIT_DIRTY,
                PICOCALC_CLOCK_BUILD_TIME,
                PICOCALC_CLOCK_BUILD_PURPOSE);
}

void draw_analog_moon_age_delta(const ds3231_datetime_t& dt,
                                bool rtc_ok,
                                char* previous_moon) {
    char moon_line[20];
    char date_line[40];
    format_moon_age_line(dt, rtc_ok, moon_line, sizeof(moon_line));
    if (std::strcmp(moon_line, previous_moon) == 0) {
        return;
    }

    if (rtc_ok) {
        const int weekday = weekday_from_date(dt.year, dt.month, dt.day);
        std::snprintf(date_line, sizeof(date_line), "%04u-%02u-%02u %s",
                      dt.year, dt.month, dt.day, weekday_name(weekday));
    } else {
        std::snprintf(date_line, sizeof(date_line), "---- -- -- ---");
    }

    const int date_text_w = static_cast<int>(std::strlen(date_line)) * 12;
    const int date_text_x =
        (picoment::display::kScreenWidth - date_text_w) / 2;
    const int moon_text_w =
        static_cast<int>(std::strlen(moon_line)) * picoment::font::kCozetteWidth;
    const int moon_text_x = date_text_x + date_text_w - moon_text_w;

    picoment::display::fill_rect(kAnalogMoonX, kAnalogMoonY,
                                 kAnalogMoonW, kAnalogMoonH, kBlack);
    picoment::display::draw_text_band(moon_text_x, kAnalogMoonY,
                                      moon_text_w, kAnalogMoonH,
                                      moon_line, rtc_ok ? kDim : kWarn, kBlack);
    std::snprintf(previous_moon, 20, "%s", moon_line);
}

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

int analog_x(uint8_t index, int length) {
    return kAnalogCenterX + kSin60[index % 60] * length / 1024;
}

int analog_y(uint8_t index, int length) {
    return kAnalogCenterY - kCos60[index % 60] * length / 1024;
}

AnalogHandState make_analog_hand_state(const ds3231_datetime_t& dt,
                                       bool rtc_ok,
                                       bool show_seconds) {
    AnalogHandState state = {};
    state.valid = rtc_ok;
    state.rtc_ok = rtc_ok;
    state.show_second = show_seconds;
    if (!rtc_ok) {
        return state;
    }

    state.hour_index =
        static_cast<uint8_t>(((dt.hour % 12) * 5 + dt.minute / 12) % 60);
    state.minute_index = static_cast<uint8_t>(dt.minute % 60);
    state.second_index = static_cast<uint8_t>(dt.second % 60);
    return state;
}

bool analog_hand_state_equal(const AnalogHandState& a,
                             const AnalogHandState& b) {
    if (a.valid != b.valid ||
        a.rtc_ok != b.rtc_ok ||
        a.show_second != b.show_second ||
        a.hour_index != b.hour_index ||
        a.minute_index != b.minute_index) {
        return false;
    }
    if (!a.show_second) {
        return true;
    }
    return a.second_index == b.second_index;
}

void draw_analog_static_face() {
    picoment::display::draw_circle(kAnalogCenterX, kAnalogCenterY,
                                   kAnalogRadius, kDim);
    for (uint8_t i = 0; i < 12; ++i) {
        const uint8_t index = static_cast<uint8_t>(i * 5);
        const int x0 = analog_x(index, kAnalogRadius - 8);
        const int y0 = analog_y(index, kAnalogRadius - 8);
        const int x1 = analog_x(index, kAnalogRadius);
        const int y1 = analog_y(index, kAnalogRadius);
        picoment::display::draw_line(x0, y0, x1, y1, kDim);
    }
}

void restore_analog_static_face_details() {
    draw_analog_static_face();
}

void draw_analog_hand(uint8_t index, int length, uint16_t color, bool thick) {
    const int x1 = analog_x(index, length);
    const int y1 = analog_y(index, length);
    picoment::display::draw_line(kAnalogCenterX, kAnalogCenterY, x1, y1, color);
    if (!thick) {
        return;
    }

    const int dx = x1 - kAnalogCenterX;
    const int dy = y1 - kAnalogCenterY;
    int ox = 0;
    int oy = 0;
    if (dx * dx > dy * dy) {
        oy = 1;
    } else {
        ox = 1;
    }
    picoment::display::draw_line(kAnalogCenterX + ox, kAnalogCenterY + oy,
                                 x1 + ox, y1 + oy, color);
    picoment::display::draw_line(kAnalogCenterX - ox, kAnalogCenterY - oy,
                                 x1 - ox, y1 - oy, color);
}

void draw_analog_hands(const AnalogHandState& state, bool erase) {
    if (!state.valid || !state.rtc_ok) {
        return;
    }

    const uint16_t hour_minute_color = erase ? kBlack : kWhite;
    const uint16_t second_color = erase ? kBlack : kSecondHand;
    draw_analog_hand(state.hour_index, kAnalogHourHandLength, hour_minute_color, true);
    draw_analog_hand(state.minute_index, kAnalogMinuteHandLength, hour_minute_color, true);
    if (state.show_second) {
        draw_analog_hand(state.second_index, kAnalogSecondHandLength, second_color, false);
    }
}

void draw_analog_hub() {
    picoment::display::fill_circle(kAnalogCenterX, kAnalogCenterY,
                                   kAnalogHubRadius, kWhite);
}

void draw_analog_ampm_label(const ds3231_datetime_t& dt, bool rtc_ok) {
    const char* label = "--";
    uint16_t color = kWarn;
    if (rtc_ok) {
        label = dt.hour < 12 ? "AM" : "PM";
        color = kDim;
    }

    picoment::display::draw_spleen_native_text_band(
        kAnalogAmPmX, kAnalogAmPmY,
        kAnalogAmPmW, kAnalogAmPmH,
        label,
        picoment::font::SpleenNativeSize::S12x24,
        color,
        kBlack);
}

void draw_analog_date_delta(const ds3231_datetime_t& dt,
                            bool rtc_ok,
                            char* previous_date) {
    char text[40];
    if (rtc_ok) {
        const int weekday = weekday_from_date(dt.year, dt.month, dt.day);
        std::snprintf(text, sizeof(text), "%04u-%02u-%02u %s",
                      dt.year, dt.month, dt.day, weekday_name(weekday));
    } else {
        std::snprintf(text, sizeof(text), "---- -- -- ---");
    }

    if (std::strcmp(text, previous_date) == 0) {
        return;
    }

    const int text_w = static_cast<int>(std::strlen(text)) * 12;
    const int text_x = (picoment::display::kScreenWidth - text_w) / 2;
    picoment::display::fill_rect(kDateBandX, kAnalogDateY,
                                 kDateBandW, kAnalogDateH, kBlack);
    picoment::display::draw_spleen_native_text_band(
        text_x, kAnalogDateY, text_w, kAnalogDateH, text,
        picoment::font::SpleenNativeSize::S12x24,
        rtc_ok ? kWhite : kWarn, kBlack);
    std::snprintf(previous_date, 40, "%s", text);
}

void draw_analog_alarm_delta(const AlarmSettings* alarms,
                             const ds3231_datetime_t& dt,
                             const AlarmFireRecord& last_fire,
                             char* previous_alarm) {
    char text[24];
    const AlarmMatch next = find_next_alarm(alarms, dt, last_fire);
    format_alarm_label(next, text, sizeof(text));
    if (std::strcmp(text, previous_alarm) == 0) {
        return;
    }

    const int text_w = static_cast<int>(std::strlen(text)) * 12;
    const int text_x = (picoment::display::kScreenWidth - text_w) / 2;
    picoment::display::fill_rect(kAnalogAlarmX, kAnalogAlarmY,
                                 kAnalogAlarmW, kAnalogAlarmH, kBlack);
    picoment::display::draw_spleen_native_text_band(
        text_x, kAnalogAlarmY, text_w, kAnalogAlarmH, text,
        picoment::font::SpleenNativeSize::S12x24, kDim, kBlack);
    std::snprintf(previous_alarm, 24, "%s", text);
}

void draw_analog_rtc_failure_label() {
    constexpr int kLabelW = 72;
    constexpr int kLabelX = (picoment::display::kScreenWidth - kLabelW) / 2;
    picoment::display::draw_spleen_native_text_band(
        kLabelX, kAnalogAlarmY, kLabelW, kAnalogAlarmH, "RTC --",
        picoment::font::SpleenNativeSize::S12x24, kWarn, kBlack);
}

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
                       AnalogHandState* previous_hand_state) {
    const AnalogHandState new_state =
        make_analog_hand_state(dt, rtc_ok, show_seconds);

    if (force_full_redraw) {
        draw_clock_frame();
        previous_date[0] = '\0';
        previous_moon[0] = '\0';
        previous_battery[0] = '\0';
        previous_alarm[0] = '\0';
        previous_hand_state->valid = false;
        draw_analog_static_face();
    }

    draw_analog_date_delta(dt, rtc_ok, previous_date);
    draw_analog_moon_age_delta(dt, rtc_ok, previous_moon);
    draw_battery_delta(battery, previous_battery);

    if (rtc_ok) {
        draw_analog_alarm_delta(alarms, dt, last_fire, previous_alarm);
        if (!analog_hand_state_equal(*previous_hand_state, new_state)) {
            if (previous_hand_state->valid) {
                draw_analog_hands(*previous_hand_state, true);
                restore_analog_static_face_details();
            }
            draw_analog_ampm_label(dt, true);
            draw_analog_hands(new_state, false);
            draw_analog_hub();
            *previous_hand_state = new_state;
        }
    } else {
        if (previous_hand_state->valid) {
            draw_analog_hands(*previous_hand_state, true);
            restore_analog_static_face_details();
        }
        picoment::display::fill_rect(kAnalogAlarmX, kAnalogAlarmY,
                                     kAnalogAlarmW, kAnalogAlarmH, kBlack);
        draw_analog_ampm_label(dt, false);
        draw_analog_rtc_failure_label();
        previous_alarm[0] = '\0';
        previous_hand_state->valid = false;
    }
}

void handle_alarm_escape(AlarmEditModel* model, UiMode* ui_mode, bool* redraw_clock) {
    if (model->selection == AlarmSelectionMode::Digit) {
        model->selection = AlarmSelectionMode::Field;
        return;
    }
    if (model->selection == AlarmSelectionMode::Field) {
        model->selection = AlarmSelectionMode::Row;
        return;
    }
    *ui_mode = UiMode::Clock;
    *redraw_clock = true;
    std::puts("ALARM edit cancel");
}

const char* raw_key_name(uint8_t key) {
    return picoment::keys::name(key);
}

bool handle_backlight_key_event(const picoment::keyboard::KeyEvent& event,
                                UiMode ui_mode,
                                BacklightState* state) {
    const bool pressed =
        event.state == picoment::keyboard::KeyState::Pressed;
    const bool released =
        event.state == picoment::keyboard::KeyState::Released;

    if (pressed && event.key == picoment::keys::Power) {
        if (ui_mode == UiMode::AlarmRinging) {
            state->user_off = !state->user_off;
            state->space_peek_active = false;
            state->alarm_forced_on = state->user_off;
            std::printf("BACKLIGHT user=%s\r\n",
                        state->user_off ? "off" : "on");
            return true;
        }

        if (!state->user_off) {
            if (backlight_turn_off(state)) {
                state->user_off = true;
                state->space_peek_active = false;
                state->alarm_forced_on = false;
                std::puts("BACKLIGHT user=off");
            }
        } else if (backlight_cancel_user_off(state)) {
            std::puts("BACKLIGHT user=on");
        }
        return true;
    }

    if (pressed &&
        (event.key == picoment::keys::F6 ||
         event.key == picoment::keys::F7 ||
         event.key == picoment::keys::F8 ||
         event.key == 'L' ||
         event.key == 'l')) {
        if (state->user_off || state->space_peek_active ||
            state->alarm_forced_on) {
            if (backlight_cancel_user_off(state)) {
                std::printf("BACKLIGHT interactive=on key=%s\r\n",
                            raw_key_name(event.key));
            }
        }
        return false;
    }

    if (pressed && event.key == picoment::keys::Space && state->user_off &&
        !state->space_peek_active) {
        if (backlight_turn_on(state)) {
            state->space_peek_active = true;
            std::puts("BACKLIGHT peek=on");
        }
        return false;
    }

    if (released && event.key == picoment::keys::Space &&
        state->space_peek_active) {
        if (state->user_off && ui_mode != UiMode::AlarmRinging &&
            !state->alarm_forced_on) {
            if (backlight_turn_off(state)) {
                state->space_peek_active = false;
                std::puts("BACKLIGHT peek=off");
            }
        } else {
            state->space_peek_active = false;
        }
        return false;
    }

    return false;
}

uint32_t main_loop_sleep_ms(uint32_t next_rtc_read_ms,
                            bool uart_poll_enabled,
                            uint32_t ui_sleep_cap_ms) {
    const uint32_t now_ms = to_ms_since_boot(get_absolute_time());
    uint32_t wait_ms = ms_until(now_ms, next_rtc_read_ms);

    if (ui_sleep_cap_ms != 0 && wait_ms > ui_sleep_cap_ms) {
        wait_ms = ui_sleep_cap_ms;
    }
    if (uart_poll_enabled && wait_ms > kMainLoopActiveSleepMs) {
        wait_ms = kMainLoopActiveSleepMs;
    }
    if (!uart_poll_enabled && wait_ms > kMainLoopMaxSleepMs) {
        wait_ms = kMainLoopMaxSleepMs;
    }
    if (wait_ms == 0) {
        wait_ms = 1;
    }
    return wait_ms;
}

bool uart_poll_should_run(const BatteryStatus& battery) {
    return uart_should_stay_awake() || (battery.ok && battery.charging);
}

void update_uart_poll_enabled(bool* enabled, const BatteryStatus& battery) {
    const bool next = uart_poll_should_run(battery);
    if (next != *enabled) {
        std::printf("UART poll=%s reason=vbus_or_charging charging=%u\r\n",
                    next ? "on" : "off",
                    battery.charging ? 1u : 0u);
    }
    *enabled = next;
}

}  // namespace

int main() {
    stdio_init_all();
    sleep_ms(200);

    print_build_id();
    std::puts("Picocalc_Clock init display -> keyboard -> i2c probes -> rtc display");

    picoment::display::init();
    picoment::keyboard::init();
#if defined(PICO_VBUS_PIN)
    gpio_init(PICO_VBUS_PIN);
    gpio_set_dir(PICO_VBUS_PIN, GPIO_IN);
#endif
    const StartupProbeConfig startup_probe_config = {
        CLOCK_I2C_PORT,
        CLOCK_I2C_SDA_PIN,
        CLOCK_I2C_SCL_PIN,
        CLOCK_I2C_SPEED_HZ,
        I2C_ADDR_KEYBOARD,
        I2C_ADDR_AT24C32_EXPECTED,
        I2C_SCAN_TIMEOUT_US,
    };
    i2c_bus_init(startup_probe_config);
    ProbeResult probes = run_startup_probes(startup_probe_config);
    BatteryStatus startup_battery = read_battery_status(CLOCK_I2C_PORT);
    BacklightState backlight = {
        false,
        false,
        false,
        kDefaultRestoreBacklight,
    };
    remember_restore_backlight(&backlight);
    std::printf("STARTUP BATTERY %s raw=0x%02X percent=%u charging=%u\r\n",
                startup_battery.ok ? "PASS" : "FAIL",
                startup_battery.raw,
                startup_battery.percent,
                startup_battery.charging ? 1u : 0u);
#if defined(PICO_VBUS_PIN)
    std::printf("STARTUP VBUS gpio=%u present=%u\r\n",
                static_cast<unsigned>(PICO_VBUS_PIN),
                gpio_get(PICO_VBUS_PIN) ? 1u : 0u);
#endif
    AlarmSettings alarms[kAlarmCount];
    set_default_alarms(alarms);
    AppSettings app_settings = default_app_settings();
    uint32_t settings_sequence = 0;
    if (probes.eeprom_ok) {
        (void)load_settings_from_eeprom(CLOCK_I2C_PORT,
                                        alarms,
                                        &app_settings,
                                        &settings_sequence);
    } else {
        std::puts("SETTINGS eeprom load skip reason=probe_fail");
    }
    print_uart_help(CLOCK_I2C_PORT);
    draw_clock_frame();

    char previous_date[40] = "";
    char previous_time[9] = "        ";
    char previous_moon[20] = "";
    char previous_battery[16] = "";
    char previous_alarm[24] = "";
    uint8_t previous_style = 0xff;
    AnalogHandState previous_analog_hand = {};
    uint8_t last_second = 255;
    bool have_rtc_sample = false;
    ds3231_datetime_t latest_dt = {};
    bool latest_dt_valid = false;
    bool latest_rtc_ok = false;
    BatteryStatus latest_battery = startup_battery;
    bool colon_visible = true;
    uint32_t next_rtc_read_ms = 0;
    uint32_t next_colon_blink_ms = 0;
    bool uart_poll_enabled = uart_poll_should_run(latest_battery);
    std::printf("UART poll=%s reason=startup charging=%u\r\n",
                uart_poll_enabled ? "on" : "off",
                latest_battery.charging ? 1u : 0u);
    UiMode ui_mode = UiMode::Clock;
    SetTimeModel set_time = {};
    AlarmEditModel alarm_edit = {};
    SettingsEditModel settings_edit = {};
    AlarmFireRecord last_alarm_fire = {};
    LifeHourRecord last_life_hour = {};
    LifeRuntime life_runtime = {};
    AlarmMatch ringing_alarm = {};
    ds3231_datetime_t ringing_dt = {};
    uint32_t alarm_started_ms = 0;
    size_t clock_help_page = 0;
    bool home_active = false;
    uint32_t last_keyboard_activity_ms = to_ms_since_boot(get_absolute_time());
    uint32_t next_battery_read_ms = 0;
    bool calendar_peek_active = false;

    auto force_clock_redraw = [&]() {
        draw_clock_frame();
        previous_date[0] = '\0';
        std::snprintf(previous_time, sizeof(previous_time), "        ");
        previous_moon[0] = '\0';
        previous_battery[0] = '\0';
        previous_alarm[0] = '\0';
        previous_style = 0xff;
        previous_analog_hand.valid = false;
        life_runtime.active = false;
        have_rtc_sample = false;
        latest_dt_valid = false;
        latest_rtc_ok = false;
        last_second = 255;
        next_rtc_read_ms = 0;
        next_colon_blink_ms = 0;
        colon_visible = true;
    };

    // Home captures the current screen without changing the drawn UI. If an
    // alarm is already due, screenshot tones are muted so the alarm can take
    // over cleanly on the next loop.
    auto screenshot_alarm_pending = [&]() {
        ds3231_datetime_t dt = {};
        if (!(ds3231_read_time(CLOCK_I2C_PORT, &dt) &&
              is_valid_datetime(dt))) {
            return false;
        }
        const AlarmMatch alarm_match = find_alarm_match(alarms, dt);
        return alarm_match.found && !same_alarm_minute(last_alarm_fire, dt);
    };

    auto enter_life = [&](bool hourly) {
        calendar_peek_active = false;
        ui_mode = UiMode::Life;
        std::printf("UI mode=life source=%s\r\n", hourly ? "hourly" : "manual");
        start_life(&life_runtime, hourly, to_ms_since_boot(get_absolute_time()));
    };

    auto exit_life = [&](const char* reason) {
        stop_life(&life_runtime, reason);
        ui_mode = UiMode::Clock;
        std::puts("UI mode=clock");
        force_clock_redraw();
    };

    while (true) {
        const uint32_t now_ms = to_ms_since_boot(get_absolute_time());

        alarm_sound_service(now_ms);
        if (ui_mode == UiMode::AlarmRinging &&
            time_reached(now_ms, alarm_started_ms + kAlarmAutoStopMs)) {
            alarm_sound_stop();
            record_alarm_minute(&last_alarm_fire, ringing_dt);
            std::puts("ALARM auto stop timeout=60s");
            alarm_sound_shutdown();
            ui_mode = UiMode::Clock;
            backlight_alarm_stopped(&backlight);
            std::puts("UI mode=clock");
            force_clock_redraw();
        }

        if (uart_poll_enabled) {
            (void)poll_uart_commands(CLOCK_I2C_PORT);
        }

        picoment::keyboard::KeyEvent event = {};
        while (picoment::keyboard::read_event(&event)) {
            last_keyboard_activity_ms = now_ms;
            if (ui_mode == UiMode::Life &&
                event.key == picoment::keys::Space &&
                event.state == picoment::keyboard::KeyState::Pressed) {
                exit_life("space");
                continue;
            }
            if (handle_backlight_key_event(event, ui_mode, &backlight)) {
                continue;
            }
            if (event.key == picoment::keys::Home &&
                event.state == picoment::keyboard::KeyState::Released) {
                home_active = false;
                continue;
            }
            if (event.key == picoment::keys::Home &&
                event.state == picoment::keyboard::KeyState::Pressed &&
                !home_active) {
                home_active = true;
                const bool suppress_sounds =
                    alarm_sound_active() ||
                    ui_mode == UiMode::AlarmRinging ||
                    screenshot_alarm_pending();
                (void)capture_screenshot_with_sounds(suppress_sounds);
                continue;
            }
            if (ui_mode == UiMode::Clock &&
                (event.key == 'c' || event.key == 'C')) {
                if (event.state == picoment::keyboard::KeyState::Pressed &&
                    !calendar_peek_active) {
                    calendar_peek_active = true;
                    std::puts("CLOCK calendar peek=on");
                    force_clock_redraw();
                } else if (event.state == picoment::keyboard::KeyState::Released &&
                           calendar_peek_active) {
                    calendar_peek_active = false;
                    std::puts("CLOCK calendar peek=off");
                    force_clock_redraw();
                }
                continue;
            }
            if (event.state != picoment::keyboard::KeyState::Pressed) {
                continue;
            }

            if (ui_mode == UiMode::ClockHelp) {
                if (event.key == picoment::keys::F10 ||
                    event.key == picoment::keys::Escape) {
                    ui_mode = UiMode::Clock;
                    std::puts("UI mode=clock");
                    force_clock_redraw();
                } else if ((event.key == picoment::keys::Right ||
                            event.key == picoment::keys::Down) &&
                           clock_help_page + 1 < kClockHelpPageCount) {
                    ++clock_help_page;
                    draw_clock_help_screen(clock_help_page);
                } else if ((event.key == picoment::keys::Left ||
                            event.key == picoment::keys::Up) &&
                           clock_help_page > 0) {
                    --clock_help_page;
                    draw_clock_help_screen(clock_help_page);
                }
                continue;
            }

            if (ui_mode == UiMode::Clock) {
                if (event.key == picoment::keys::F10) {
                    calendar_peek_active = false;
                    clock_help_page = 0;
                    ui_mode = UiMode::ClockHelp;
                    std::puts("UI mode=clock-help");
                    draw_clock_help_screen(clock_help_page);
                } else if (event.key == 'L' || event.key == 'l') {
                    enter_life(false);
                } else if (event.key == picoment::keys::F6) {
                    calendar_peek_active = false;
                    alarm_edit = make_alarm_edit_model(alarms);
                    ui_mode = UiMode::SetAlarm;
                    std::puts("UI mode=set-alarm");
                    draw_set_alarm_screen_full(alarm_edit);
                } else if (event.key == picoment::keys::F7) {
                    calendar_peek_active = false;
                    settings_edit = make_settings_edit_model(app_settings);
                    ui_mode = UiMode::SetSettings;
                    std::puts("UI mode=settings");
                    draw_settings_screen(settings_edit);
                } else if (event.key == picoment::keys::F8) {
                    ds3231_datetime_t dt = {};
                    if (ds3231_read_time(CLOCK_I2C_PORT, &dt) &&
                        is_valid_datetime(dt)) {
                        set_time = make_set_time_model(dt);
                        calendar_peek_active = false;
                        ui_mode = UiMode::SetTime;
                        std::puts("UI mode=set-time");
                        draw_set_time_screen(set_time);
                    } else {
                        std::puts("SETTIME enter fail: RTC read failed");
                    }
                }
                continue;
            }

            if (ui_mode == UiMode::SetTime) {
                bool redraw_set_time = true;
                switch (event.key) {
                case picoment::keys::Left:
                    handle_set_time_left(&set_time);
                    break;
                case picoment::keys::Right:
                    handle_set_time_right(&set_time);
                    break;
                case picoment::keys::Up:
                    handle_set_time_up_down(&set_time, 1);
                    break;
                case picoment::keys::Down:
                    handle_set_time_up_down(&set_time, -1);
                    break;
                case picoment::keys::Enter: {
                    ds3231_datetime_t dt = model_to_datetime(set_time);
                    ds3231_datetime_t after = {};
                    std::printf("SETTIME write start %04u-%02u-%02u %02u:%02u:%02u\r\n",
                                dt.year, dt.month, dt.day,
                                dt.hour, dt.minute, dt.second);
                    if (dt.day_of_week != 0 &&
                        ds3231_write_time(CLOCK_I2C_PORT, &dt) &&
                        ds3231_read_time(CLOCK_I2C_PORT, &after) &&
                        is_valid_datetime(after)) {
                        std::puts("SETTIME write ok");
                        ui_mode = UiMode::Clock;
                        std::puts("UI mode=clock");
                        force_clock_redraw();
                        redraw_set_time = false;
                    } else {
                        std::puts("SETTIME write fail");
                        set_status(&set_time, "SET FAIL");
                    }
                    break;
                }
                case picoment::keys::Escape:
                    ui_mode = UiMode::Clock;
                    std::puts("SETTIME cancel");
                    std::puts("UI mode=clock");
                    force_clock_redraw();
                    redraw_set_time = false;
                    break;
                default:
                    if (event.key >= '0' && event.key <= '9') {
                        handle_set_time_digit(&set_time,
                                              static_cast<uint8_t>(event.key - '0'));
                    } else {
                        redraw_set_time = false;
                    }
                    break;
                }

                if (ui_mode == UiMode::SetTime && redraw_set_time) {
                    draw_set_time_screen(set_time);
                }
                continue;
            }

            if (ui_mode == UiMode::SetAlarm) {
                bool redraw_alarm = true;
                bool redraw_clock = false;
                switch (event.key) {
                case picoment::keys::Left:
                    handle_alarm_left(&alarm_edit);
                    break;
                case picoment::keys::Right:
                    handle_alarm_right(&alarm_edit);
                    break;
                case picoment::keys::Up:
                    handle_alarm_up_down(&alarm_edit, 1);
                    break;
                case picoment::keys::Down:
                    handle_alarm_up_down(&alarm_edit, -1);
                    break;
                case picoment::keys::Enter: {
                    const bool changed = !alarms_equal(alarms, alarm_edit.alarms);
                    for (uint8_t i = 0; i < kAlarmCount; ++i) {
                        alarms[i] = alarm_edit.alarms[i];
                    }
                    if (changed) {
                        if (probes.eeprom_ok) {
                            (void)save_settings_to_eeprom(CLOCK_I2C_PORT,
                                                          alarms,
                                                          app_settings,
                                                          &settings_sequence);
                        } else {
                            std::puts("SETTINGS eeprom save skip reason=probe_fail");
                        }
                    } else {
                        std::puts("SETTINGS eeprom save skip reason=unchanged");
                    }
                    if (latest_dt_valid &&
                        find_alarm_match(alarms, latest_dt).found) {
                        record_alarm_minute(&last_alarm_fire, latest_dt);
                        std::puts("ALARM suppress same minute");
                    }
                    std::printf("ALARM settings A1=%u %02u:%02u A2=%u %02u:%02u A3=%u %02u:%02u A4=%u %02u:%02u A5=%u %02u:%02u\r\n",
                                alarms[0].enabled ? 1u : 0u, alarms[0].hour, alarms[0].minute,
                                alarms[1].enabled ? 1u : 0u, alarms[1].hour, alarms[1].minute,
                                alarms[2].enabled ? 1u : 0u, alarms[2].hour, alarms[2].minute,
                                alarms[3].enabled ? 1u : 0u, alarms[3].hour, alarms[3].minute,
                                alarms[4].enabled ? 1u : 0u, alarms[4].hour, alarms[4].minute);
                    ui_mode = UiMode::Clock;
                    std::puts("UI mode=clock");
                    force_clock_redraw();
                    redraw_alarm = false;
                    break;
                }
                case picoment::keys::Escape:
                    handle_alarm_escape(&alarm_edit, &ui_mode, &redraw_clock);
                    if (redraw_clock) {
                        std::puts("UI mode=clock");
                        force_clock_redraw();
                        redraw_alarm = false;
                    }
                    break;
                default:
                    if (event.key >= '0' && event.key <= '9') {
                        handle_alarm_digit(&alarm_edit,
                                           static_cast<uint8_t>(event.key - '0'));
                    } else {
                        redraw_alarm = false;
                    }
                    break;
                }

                if (ui_mode == UiMode::SetAlarm && redraw_alarm) {
                    draw_set_alarm_screen(alarm_edit);
                }
                continue;
            }

            if (ui_mode == UiMode::SetSettings) {
                bool redraw_settings = true;
                switch (event.key) {
                case picoment::keys::Up:
                    handle_settings_up_down(&settings_edit, -1);
                    break;
                case picoment::keys::Down:
                    handle_settings_up_down(&settings_edit, 1);
                    break;
                case picoment::keys::Left:
                case picoment::keys::Right:
                case picoment::keys::Space:
                    handle_settings_toggle(&settings_edit);
                    break;
                case picoment::keys::Enter: {
                    const bool changed =
                        !app_settings_equal(app_settings, settings_edit.settings);
                    app_settings = settings_edit.settings;
                    if (changed) {
                        if (probes.eeprom_ok) {
                            (void)save_settings_to_eeprom(CLOCK_I2C_PORT,
                                                          alarms,
                                                          app_settings,
                                                          &settings_sequence);
                        } else {
                            std::puts("SETTINGS eeprom save skip reason=probe_fail");
                        }
                    } else {
                        std::puts("SETTINGS eeprom save skip reason=unchanged");
                    }
                    const char* style_name = "digital";
                    if (app_settings.clock_style == kClockStyleAnalog) {
                        style_name = "analog";
                    } else if (app_settings.clock_style == kClockStyleCalendar) {
                        style_name = "calendar";
                    }
                    std::printf("SETTINGS app seconds=%u style=%s life=%u\r\n",
                                app_settings.show_seconds ? 1u : 0u,
                                style_name,
                                app_settings.life_hourly_enabled ? 1u : 0u);
                    ui_mode = UiMode::Clock;
                    std::puts("UI mode=clock");
                    force_clock_redraw();
                    redraw_settings = false;
                    break;
                }
                case picoment::keys::Escape:
                    ui_mode = UiMode::Clock;
                    std::puts("SETTINGS cancel");
                    std::puts("UI mode=clock");
                    force_clock_redraw();
                    redraw_settings = false;
                    break;
                default:
                    redraw_settings = false;
                    break;
                }

                if (ui_mode == UiMode::SetSettings && redraw_settings) {
                    draw_settings_screen(settings_edit);
                }
                continue;
            }

            if (ui_mode == UiMode::AlarmRinging) {
                if (event.key == picoment::keys::Space) {
                    alarm_sound_stop();
                    record_alarm_minute(&last_alarm_fire, ringing_dt);
                    std::puts("ALARM stopped by Space");
                    alarm_sound_shutdown();
                    ui_mode = UiMode::Clock;
                    backlight_alarm_stopped(&backlight);
                    std::puts("UI mode=clock");
                    force_clock_redraw();
                }
                continue;
            }
        }

        if (ui_mode == UiMode::Life && life_runtime.active) {
            if (step_life(&life_runtime)) {
                exit_life("stable");
            } else if (life_runtime.hourly &&
                       time_reached(now_ms,
                                    life_runtime.started_ms + kLifeHourlyMaxMs)) {
                exit_life("timeout");
            }
        }

        if (ui_mode == UiMode::Clock && time_reached(now_ms, next_rtc_read_ms)) {

            ds3231_datetime_t dt = {};
            const bool rtc_ok = ds3231_read_time(CLOCK_I2C_PORT, &dt) &&
                                is_valid_datetime(dt);
            if (rtc_ok && !have_rtc_sample) {
                latest_dt = dt;
                latest_dt_valid = true;
                latest_rtc_ok = true;
                last_second = dt.second;
                have_rtc_sample = true;
                next_rtc_read_ms = now_ms + kRtcSearchPollMs;
            } else if (rtc_ok && dt.second != last_second) {
                latest_dt = dt;
                latest_dt_valid = true;
                latest_rtc_ok = true;
                if (time_reached(now_ms, next_battery_read_ms)) {
                    latest_battery = read_battery_status(CLOCK_I2C_PORT);
                    next_battery_read_ms = now_ms + kBatteryReadIntervalMs;
                }
                update_uart_poll_enabled(&uart_poll_enabled, latest_battery);
                const uint8_t active_clock_style =
                    calendar_peek_active ? kClockStyleCalendar
                                         : app_settings.clock_style;
                if (active_clock_style == kClockStyleAnalog) {
                    const bool force_full_redraw =
                        previous_style != active_clock_style;
                    draw_analog_clock(dt, true, latest_battery, alarms, last_alarm_fire,
                                      app_settings.show_seconds,
                                      force_full_redraw,
                                      previous_date, previous_moon,
                                      previous_battery,
                                      previous_alarm, &previous_analog_hand);
                    previous_style = active_clock_style;
                } else if (active_clock_style == kClockStyleCalendar) {
                    const bool force_full_redraw =
                        previous_style != active_clock_style;
                    draw_calendar_clock(dt, true, latest_battery,
                                        alarms, last_alarm_fire,
                                        app_settings.show_seconds,
                                        force_full_redraw,
                                        previous_date, previous_time,
                                        previous_moon, previous_battery,
                                        previous_alarm);
                    previous_style = active_clock_style;
                } else {
                    if (previous_style != active_clock_style) {
                        draw_clock_frame();
                        previous_date[0] = '\0';
                        std::snprintf(previous_time, sizeof(previous_time), "        ");
                        previous_moon[0] = '\0';
                        previous_battery[0] = '\0';
                        previous_alarm[0] = '\0';
                        previous_style = active_clock_style;
                    }
                    char date_line[40];
                    char time_line[24];
                    char moon_line[20];
                    format_clock_lines(dt, true, app_settings.show_seconds,
                                       date_line, sizeof(date_line),
                                       time_line, sizeof(time_line));
                    format_moon_age_line(dt, true,
                                         moon_line, sizeof(moon_line));
                    if (!app_settings.show_seconds && !colon_visible) {
                        time_line[2] = ' ';
                    }
                    draw_clock_delta(date_line, time_line,
                                     previous_date, previous_time, true);
                    draw_moon_age_delta(moon_line, previous_moon, true);
                    draw_battery_delta(latest_battery, previous_battery);
                    draw_alarm_delta(alarms, dt, last_alarm_fire, previous_alarm);
                }
                AlarmMatch alarm_match = find_alarm_match(alarms, dt);
                if (alarm_match.found && !same_alarm_minute(last_alarm_fire, dt)) {
                    calendar_peek_active = false;
                    ringing_alarm = alarm_match;
                    ringing_dt = dt;
                    alarm_started_ms = now_ms;
                    ui_mode = UiMode::AlarmRinging;
                    alarm_sound_start(now_ms);
                    draw_alarm_ringing_screen(ringing_alarm);
                    backlight_alarm_started(&backlight);
                    std::printf("ALARM fire date=%04u-%02u-%02u time=%02u:%02u alarms=A%u%s\r\n",
                                dt.year, dt.month, dt.day,
                                dt.hour, dt.minute,
                                alarm_match.first_index + 1u,
                                alarm_match.count > 1 ? "+" : "");
                }
                if (ui_mode == UiMode::Clock &&
                    app_settings.life_hourly_enabled &&
                    dt.minute == 0 &&
                    dt.second == 0 &&
                    !same_life_hour(last_life_hour, dt)) {
                    record_life_hour(&last_life_hour, dt);
                    enter_life(true);
                }
                last_second = dt.second;
                next_rtc_read_ms = now_ms + kRtcRestAfterTickMs;
            } else if (rtc_ok) {
                latest_dt = dt;
                latest_dt_valid = true;
                latest_rtc_ok = true;
                next_rtc_read_ms = now_ms + kRtcSearchPollMs;
            } else if (!rtc_ok) {
                have_rtc_sample = false;
                latest_dt_valid = false;
                latest_rtc_ok = false;
                last_second = 255;
                if (time_reached(now_ms, next_battery_read_ms)) {
                    latest_battery = read_battery_status(CLOCK_I2C_PORT);
                    next_battery_read_ms = now_ms + kBatteryReadIntervalMs;
                }
                update_uart_poll_enabled(&uart_poll_enabled, latest_battery);
                const uint8_t active_clock_style =
                    calendar_peek_active ? kClockStyleCalendar
                                         : app_settings.clock_style;
                if (active_clock_style == kClockStyleAnalog) {
                    const bool force_full_redraw =
                        previous_style != active_clock_style;
                    draw_analog_clock(dt, false, latest_battery, alarms, last_alarm_fire,
                                      app_settings.show_seconds,
                                      force_full_redraw,
                                      previous_date, previous_moon,
                                      previous_battery,
                                      previous_alarm, &previous_analog_hand);
                    previous_style = active_clock_style;
                } else if (active_clock_style == kClockStyleCalendar) {
                    const bool force_full_redraw =
                        previous_style != active_clock_style;
                    draw_calendar_clock(dt, false, latest_battery,
                                        alarms, last_alarm_fire,
                                        app_settings.show_seconds,
                                        force_full_redraw,
                                        previous_date, previous_time,
                                        previous_moon, previous_battery,
                                        previous_alarm);
                    previous_style = active_clock_style;
                } else {
                    if (previous_style != active_clock_style) {
                        draw_clock_frame();
                        previous_date[0] = '\0';
                        std::snprintf(previous_time, sizeof(previous_time), "        ");
                        previous_moon[0] = '\0';
                        previous_battery[0] = '\0';
                        previous_alarm[0] = '\0';
                        previous_style = active_clock_style;
                    }
                    char date_line[40];
                    char time_line[24];
                    char moon_line[20];
                    format_clock_lines(dt, false, app_settings.show_seconds,
                                       date_line, sizeof(date_line),
                                       time_line, sizeof(time_line));
                    format_moon_age_line(dt, false,
                                         moon_line, sizeof(moon_line));
                    if (!app_settings.show_seconds && !colon_visible) {
                        time_line[2] = ' ';
                    }
                    draw_clock_delta(date_line, time_line,
                                     previous_date, previous_time, false);
                    draw_moon_age_delta(moon_line, previous_moon, false);
                    draw_battery_delta(latest_battery, previous_battery);
                    picoment::display::fill_rect(kAlarmBandX, kAlarmBandY,
                                                 kAlarmBandW, kAlarmBandH, kBlack);
                    previous_alarm[0] = '\0';
                }
                next_rtc_read_ms = now_ms + kRtcFailRetryMs;
            }
        }

        if (ui_mode == UiMode::Clock &&
            !calendar_peek_active &&
            app_settings.clock_style == kClockStyleDigital &&
            !app_settings.show_seconds &&
            time_reached(now_ms, next_colon_blink_ms)) {
            if (std::strlen(previous_time) == 5) {
                colon_visible = !colon_visible;
                draw_no_seconds_colon(colon_visible, latest_rtc_ok, previous_time);
            }
            next_colon_blink_ms = now_ms + kColonBlinkMs;
        }

        if (ui_mode == UiMode::Life) {
            sleep_ms(kLifeLoopSleepMs);
        } else if (ui_mode == UiMode::AlarmRinging) {
            sleep_ms(kAlarmLoopSleepMs);
        } else if (ui_mode != UiMode::Clock) {
            sleep_ms(uart_poll_enabled ? kMainLoopActiveSleepMs : kUiSleepCapMs);
        } else {
            const bool clock_key_poll_idle =
                time_reached(now_ms,
                             last_keyboard_activity_ms +
                                 kClockIdleKeySlowAfterMs);
            const uint32_t clock_ui_sleep_cap_ms =
                clock_key_poll_idle ? kClockIdleKeySleepCapMs : kUiSleepCapMs;
            uint32_t sleep_ms_value =
                main_loop_sleep_ms(next_rtc_read_ms, uart_poll_enabled,
                                   clock_ui_sleep_cap_ms);
            if (!calendar_peek_active &&
                app_settings.clock_style == kClockStyleDigital &&
                !app_settings.show_seconds) {
                const uint32_t blink_wait_ms = ms_until(now_ms, next_colon_blink_ms);
                if (blink_wait_ms < sleep_ms_value) {
                    sleep_ms_value = blink_wait_ms;
                }
                if (sleep_ms_value == 0) {
                    sleep_ms_value = 1;
                }
            }
            sleep_ms(sleep_ms_value);
        }
    }
}
