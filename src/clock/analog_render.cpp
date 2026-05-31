#include "clock/analog_render.h"

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
constexpr int kDateBandX = 52;
constexpr int kDateBandW = 216;
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

}  // namespace

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
