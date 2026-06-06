#include "clock/clock_render.h"

#include <cstdio>
#include <cstring>

#include "font/cozette_font.h"
#include "platform/picocalc_display.h"
#include "version.h"

namespace {

constexpr uint16_t kBlack = 0x0000;
constexpr uint16_t kWhite = 0xffff;
constexpr uint16_t kDim = 0x7bef;
constexpr uint16_t kWarn = 0xfde0;
constexpr int kDateBandX = 52;
constexpr int kDateY = 58;
constexpr int kDateBandW = 216;
constexpr int kDateH = 24;
constexpr int kTimeY = 128;
constexpr int kTimeCharW = 32;
constexpr int kTimeH = 64;
constexpr int kTimeNoSecondsY = 112;
constexpr int kTimeNoSecondsCharW = 48;
constexpr int kTimeNoSecondsH = 96;
constexpr int kMoonBandX = 76;
constexpr int kMoonBandY = 86;
constexpr int kMoonBandW = 180;
constexpr int kMoonBandH = 24;
constexpr int kSensorBandX = 8;
constexpr int kSensorBandY = 212;
constexpr int kSensorBandW = 304;
constexpr int kSensorBandH = 24;
constexpr int kHeaderY = 12;
constexpr int kHeaderH = 18;
constexpr int kBatteryBandX = 214;
constexpr int kBatteryBandW = 96;
constexpr int kAlarmBandX = 78;
constexpr int kAlarmBandY = 246;
constexpr int kAlarmBandW = 164;
constexpr int kAlarmBandH = 24;

void format_app_label(char* text, size_t len) {
    std::snprintf(text, len, "Clock v%s", PICOCALC_CLOCK_VERSION_STRING);
}

void format_fixed_1(char* text, size_t len, int32_t value_x100, const char* unit) {
    const bool negative = value_x100 < 0;
    uint32_t abs_value = negative ? static_cast<uint32_t>(-value_x100)
                                  : static_cast<uint32_t>(value_x100);
    const uint32_t rounded = (abs_value + 5u) / 10u;
    std::snprintf(text, len, "%s%lu.%lu%s",
                  negative ? "-" : "",
                  static_cast<unsigned long>(rounded / 10u),
                  static_cast<unsigned long>(rounded % 10u),
                  unit);
}

void format_env_sensor_line(const EnvSensorData& sensors, char* text, size_t len) {
    char temp[16] = "--.-C";
    char humidity[16] = "--.-%";
    char pressure[16] = "----.-hPa";
    if (sensors.aht20_ok && sensors.bmp280_ok) {
        const int32_t raw_average_temp =
            (sensors.aht20_temperature_c_x100 +
             sensors.bmp280_temperature_c_x100) /
            2;
        const int32_t average_temp =
            raw_average_temp + sensors.temperature_offset_c_x100;
        format_fixed_1(temp, sizeof(temp), average_temp, "C");
    }
    if (sensors.aht20_ok) {
        format_fixed_1(humidity, sizeof(humidity),
                       static_cast<int32_t>(sensors.aht20_humidity_pct_x100),
                       "%");
    }
    if (sensors.bmp280_ok) {
        const uint32_t pressure_hpa_x100 = sensors.bmp280_pressure_pa_x100 / 100u;
        const uint32_t pressure_hpa = (pressure_hpa_x100 + 50u) / 100u;
        std::snprintf(pressure, sizeof(pressure), "%luhPa",
                      static_cast<unsigned long>(pressure_hpa));
    }
    std::snprintf(text, len, "%s  %s  %s", temp, humidity, pressure);
}

}  // namespace

void draw_clock_frame() {
    char header[64];
    format_app_label(header, sizeof(header));

    picoment::display::clear(kBlack);
    picoment::display::draw_text_band(8, 304, 304, 16, header, kDim, kBlack);
    const char* help = "F10:Help";
    const int help_w =
        static_cast<int>(std::strlen(help)) * picoment::font::kCozetteWidth;
    picoment::display::draw_text_band(312 - help_w, 304, help_w, 16,
                                      help, kDim, kBlack);
}

void draw_moon_age_delta(const char* moon_line,
                         char* previous_moon,
                         bool rtc_ok) {
    if (std::strcmp(moon_line, previous_moon) == 0) {
        return;
    }

    const int moon_text_w = static_cast<int>(std::strlen(moon_line)) * 12;
    const int moon_text_x =
        (picoment::display::kScreenWidth - moon_text_w) / 2;
    picoment::display::fill_rect(kMoonBandX, kMoonBandY,
                                 kMoonBandW, kMoonBandH, kBlack);
    picoment::display::draw_spleen_native_text_band(
        moon_text_x, kMoonBandY, moon_text_w, kMoonBandH, moon_line,
        picoment::font::SpleenNativeSize::S12x24,
        rtc_ok ? kDim : kWarn, kBlack);
    std::snprintf(previous_moon, 20, "%s", moon_line);
}

void draw_env_sensor_delta(const EnvSensorData& sensors,
                           char* previous_sensor) {
    char sensor_line[48];
    format_env_sensor_line(sensors, sensor_line, sizeof(sensor_line));
    if (std::strcmp(sensor_line, previous_sensor) == 0) {
        return;
    }

    const int sensor_text_w = static_cast<int>(std::strlen(sensor_line)) * 12;
    const int sensor_text_x =
        (picoment::display::kScreenWidth - sensor_text_w) / 2;
    picoment::display::fill_rect(kSensorBandX, kSensorBandY,
                                 kSensorBandW, kSensorBandH, kBlack);
    picoment::display::draw_spleen_native_text_band(
        sensor_text_x, kSensorBandY, sensor_text_w, kSensorBandH, sensor_line,
        picoment::font::SpleenNativeSize::S12x24, kDim, kBlack);
    std::snprintf(previous_sensor, 48, "%s", sensor_line);
}

void draw_clock_delta(const char* date_line,
                      const char* time_line,
                      char* previous_date,
                      char* previous_time,
                      bool rtc_ok) {
    if (std::strcmp(date_line, previous_date) != 0) {
        const int date_text_w = static_cast<int>(std::strlen(date_line)) * 12;
        const int date_text_x = (picoment::display::kScreenWidth - date_text_w) / 2;
        picoment::display::fill_rect(kDateBandX, kDateY, kDateBandW, kDateH, kBlack);
        picoment::display::draw_spleen_native_text_band(
            date_text_x, kDateY, date_text_w, kDateH, date_line,
            picoment::font::SpleenNativeSize::S12x24, kWhite, kBlack);
        std::snprintf(previous_date, 40, "%s", date_line);
    }

    const int len = static_cast<int>(std::strlen(time_line));
    const int old_len = static_cast<int>(std::strlen(previous_time));
    const bool large_time = len == 5;
    const int char_w = large_time ? kTimeNoSecondsCharW : kTimeCharW;
    const int time_h = large_time ? kTimeNoSecondsH : kTimeH;
    const int time_y = large_time ? kTimeNoSecondsY : kTimeY;
    const int time_x = (picoment::display::kScreenWidth - len * char_w) / 2;
    if (len != old_len) {
        picoment::display::fill_rect(0, kTimeNoSecondsY,
                                     picoment::display::kScreenWidth,
                                     kTimeNoSecondsH, kBlack);
        std::snprintf(previous_time, 9, "        ");
    }

    for (int i = 0; i < len; ++i) {
        if (time_line[i] != previous_time[i]) {
            char ch[2] = {time_line[i], '\0'};
            if (large_time) {
                picoment::display::draw_spleen_native_text_3x2_band(
                    time_x + i * char_w, time_y, char_w, time_h, ch,
                    picoment::font::SpleenNativeSize::S32x64,
                    rtc_ok ? kWhite : kWarn, kBlack);
            } else {
                picoment::display::draw_spleen_native_text_band(
                    time_x + i * char_w, time_y, char_w, time_h, ch,
                    picoment::font::SpleenNativeSize::S32x64,
                    rtc_ok ? kWhite : kWarn, kBlack);
            }
            previous_time[i] = time_line[i];
        }
    }
    previous_time[len] = '\0';
}

void draw_no_seconds_colon(bool visible,
                           bool rtc_ok,
                           char* previous_time) {
    if (std::strlen(previous_time) != 5) {
        return;
    }

    const char colon = visible ? ':' : ' ';
    if (previous_time[2] == colon) {
        return;
    }

    const int time_x =
        (picoment::display::kScreenWidth - 5 * kTimeNoSecondsCharW) / 2;
    char ch[2] = {colon, '\0'};
    picoment::display::draw_spleen_native_text_3x2_band(
        time_x + 2 * kTimeNoSecondsCharW,
        kTimeNoSecondsY,
        kTimeNoSecondsCharW,
        kTimeNoSecondsH,
        ch,
        picoment::font::SpleenNativeSize::S32x64,
        rtc_ok ? kWhite : kWarn,
        kBlack);
    previous_time[2] = colon;
}

void format_battery_text(const BatteryStatus& battery, char* text, size_t len) {
    if (!battery.ok) {
        std::snprintf(text, len, "Bat. --%%");
    } else if (battery.charging) {
        std::snprintf(text, len, "Bat. %u%%+", battery.percent);
    } else {
        std::snprintf(text, len, "Bat. %u%%", battery.percent);
    }
}

void draw_battery_delta(const BatteryStatus& battery, char* previous_battery) {
    char text[16];
    format_battery_text(battery, text, sizeof(text));
    if (std::strcmp(text, previous_battery) == 0) {
        return;
    }

    const int text_w = static_cast<int>(std::strlen(text)) * picoment::font::kCozetteWidth;
    int text_x = kBatteryBandX + kBatteryBandW - text_w;
    if (text_x < kBatteryBandX) {
        text_x = kBatteryBandX;
    }
    picoment::display::fill_rect(kBatteryBandX, kHeaderY, kBatteryBandW, kHeaderH, kBlack);
    picoment::display::draw_text_band(text_x, kHeaderY,
                                      kBatteryBandW - (text_x - kBatteryBandX),
                                      kHeaderH, text, kDim, kBlack);
    std::snprintf(previous_battery, 16, "%s", text);
}

void draw_alarm_delta(const AlarmSettings* alarms,
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
    picoment::display::fill_rect(kAlarmBandX, kAlarmBandY,
                                 kAlarmBandW, kAlarmBandH, kBlack);
    picoment::display::draw_spleen_native_text_band(
        text_x, kAlarmBandY, text_w, kAlarmBandH, text,
        picoment::font::SpleenNativeSize::S12x24, kDim, kBlack);
    std::snprintf(previous_alarm, 24, "%s", text);
}
