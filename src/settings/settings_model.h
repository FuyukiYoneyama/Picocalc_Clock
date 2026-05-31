#ifndef PICOCALC_CLOCK_SETTINGS_MODEL_H
#define PICOCALC_CLOCK_SETTINGS_MODEL_H

#include <cstdint>

#include "alarm/alarm_model.h"

constexpr uint16_t kSettingsRecordSize = 64;
constexpr uint8_t kClockStyleDigital = 0;
constexpr uint8_t kClockStyleAnalog = 1;
constexpr uint8_t kClockStyleCalendar = 2;

struct AppSettings {
    bool show_seconds;
    bool life_hourly_enabled;
    uint8_t clock_style;
};

struct SettingsRecord {
    uint32_t magic;
    uint16_t version;
    uint16_t size;
    uint32_t sequence;
    uint8_t alarm_enabled[kAlarmCount];
    uint8_t alarm_hour[kAlarmCount];
    uint8_t alarm_minute[kAlarmCount];
    uint8_t app_flags;
    uint8_t clock_style;
    uint8_t reserved[31];
    uint32_t crc32;
};

static_assert(sizeof(SettingsRecord) == kSettingsRecordSize,
              "SettingsRecord must fit one 64-byte EEPROM slot");

#endif
