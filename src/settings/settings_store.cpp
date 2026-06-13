#include "settings/settings_store.h"

#include <cstdio>
#include <cstring>

#include "pico/stdlib.h"

namespace {

constexpr uint16_t kSettingsSlotA = 0x0000;
constexpr uint16_t kSettingsSlotB = 0x0040;
constexpr uint8_t kSettingsPageSize = 32;
constexpr uint32_t kSettingsMagic = 0x4b4c4350u;  // "PCLK"
constexpr uint16_t kSettingsVersionAlarmOnly = 2;
constexpr uint16_t kSettingsVersionAppStyle = 3;
constexpr uint16_t kSettingsVersion = 4;
constexpr uint8_t kSettingsFlagShowSeconds = 0x01;
constexpr uint8_t kSettingsFlagLifeHourly = 0x02;
constexpr uint32_t kSettingsWriteCycleTimeoutMs = 20;
constexpr uint8_t kAt24c32Address = 0x57;
constexpr uint32_t kI2cTimeoutUs = 10000;

bool time_reached(uint32_t now_ms, uint32_t target_ms) {
    return static_cast<int32_t>(now_ms - target_ms) >= 0;
}

uint32_t crc32_update(uint32_t crc, const uint8_t* data, size_t len) {
    crc = ~crc;
    for (size_t i = 0; i < len; ++i) {
        crc ^= data[i];
        for (int bit = 0; bit < 8; ++bit) {
            const uint32_t mask = 0u - (crc & 1u);
            crc = (crc >> 1) ^ (0xedb88320u & mask);
        }
    }
    return ~crc;
}

uint32_t settings_record_crc(const SettingsRecord& record) {
    return crc32_update(0u,
                        reinterpret_cast<const uint8_t*>(&record),
                        sizeof(SettingsRecord) - sizeof(record.crc32));
}

bool eeprom_read_bytes(i2c_inst_t* i2c, uint16_t address, uint8_t* data, size_t len) {
    uint8_t ptr[2] = {
        static_cast<uint8_t>(address >> 8),
        static_cast<uint8_t>(address & 0xffu),
    };
    int written = i2c_write_timeout_us(i2c, kAt24c32Address, ptr, 2, true,
                                       kI2cTimeoutUs);
    if (written != 2) {
        return false;
    }
    int read = i2c_read_timeout_us(i2c, kAt24c32Address, data, len, false,
                                   kI2cTimeoutUs);
    return read == static_cast<int>(len);
}

bool eeprom_wait_ready(i2c_inst_t* i2c) {
    const uint32_t start_ms = to_ms_since_boot(get_absolute_time());
    uint8_t ptr[2] = {0x00, 0x00};
    while (true) {
        int written = i2c_write_timeout_us(i2c, kAt24c32Address, ptr, 2, false,
                                           kI2cTimeoutUs);
        if (written == 2) {
            return true;
        }
        const uint32_t now_ms = to_ms_since_boot(get_absolute_time());
        if (time_reached(now_ms, start_ms + kSettingsWriteCycleTimeoutMs)) {
            return false;
        }
        sleep_ms(1);
    }
}

bool eeprom_write_page(i2c_inst_t* i2c,
                       uint16_t address,
                       const uint8_t* data,
                       size_t len) {
    if (len == 0 || len > kSettingsPageSize ||
        (address / kSettingsPageSize) !=
            ((address + static_cast<uint16_t>(len) - 1u) / kSettingsPageSize)) {
        return false;
    }

    uint8_t packet[2 + kSettingsPageSize] = {
        static_cast<uint8_t>(address >> 8),
        static_cast<uint8_t>(address & 0xffu),
    };
    std::memcpy(packet + 2, data, len);
    int written = i2c_write_timeout_us(i2c, kAt24c32Address, packet, len + 2,
                                       false, kI2cTimeoutUs);
    return written == static_cast<int>(len + 2) && eeprom_wait_ready(i2c);
}

bool eeprom_write_record(i2c_inst_t* i2c,
                         uint16_t slot_address,
                         const SettingsRecord& record) {
    const uint8_t* bytes = reinterpret_cast<const uint8_t*>(&record);
    for (uint16_t offset = 0; offset < kSettingsRecordSize;
         offset += kSettingsPageSize) {
        if (!eeprom_write_page(i2c, slot_address + offset, bytes + offset,
                               kSettingsPageSize)) {
            return false;
        }
    }

    SettingsRecord verify = {};
    return eeprom_read_bytes(i2c, slot_address,
                             reinterpret_cast<uint8_t*>(&verify),
                             sizeof(verify)) &&
           std::memcmp(&verify, &record, sizeof(record)) == 0;
}

void make_settings_record(const AlarmSettings* alarms,
                          const AppSettings& settings,
                          uint32_t sequence,
                          SettingsRecord* record) {
    std::memset(record, 0, sizeof(*record));
    record->magic = kSettingsMagic;
    record->version = kSettingsVersion;
    record->size = kSettingsRecordSize;
    record->sequence = sequence;
    for (uint8_t i = 0; i < kAlarmCount; ++i) {
        record->alarm_enabled[i] = alarms[i].enabled ? 1u : 0u;
        record->alarm_hour[i] = alarms[i].hour;
        record->alarm_minute[i] = alarms[i].minute;
    }
    record->app_flags = 0;
    if (settings.show_seconds) {
        record->app_flags |= kSettingsFlagShowSeconds;
    }
    if (settings.life_hourly_enabled) {
        record->app_flags |= kSettingsFlagLifeHourly;
    }
    record->clock_style = settings.clock_style;
    record->temperature_offset_tenths_c = settings.temperature_offset_tenths_c;
    record->reserved[0] = settings.life_cell_color_index;
    record->crc32 = settings_record_crc(*record);
}

bool settings_record_valid(const SettingsRecord& record) {
    if (record.magic != kSettingsMagic ||
        (record.version != kSettingsVersion &&
         record.version != kSettingsVersionAppStyle &&
         record.version != kSettingsVersionAlarmOnly) ||
        record.size != kSettingsRecordSize ||
        record.crc32 != settings_record_crc(record)) {
        return false;
    }
    for (uint8_t i = 0; i < kAlarmCount; ++i) {
        if (record.alarm_enabled[i] > 1 ||
            record.alarm_hour[i] > 23 ||
            record.alarm_minute[i] > 59) {
            return false;
        }
    }
    if (record.version >= kSettingsVersion &&
        (record.temperature_offset_tenths_c < kTemperatureOffsetMinTenthsC ||
         record.temperature_offset_tenths_c > kTemperatureOffsetMaxTenthsC)) {
        return false;
    }
    if (record.version >= kSettingsVersionAppStyle &&
        record.clock_style != kClockStyleDigital &&
        record.clock_style != kClockStyleAnalog &&
        record.clock_style != kClockStyleCalendar) {
        return false;
    }
    return true;
}

void apply_settings_record(const SettingsRecord& record,
                           AlarmSettings* alarms,
                           AppSettings* settings) {
    for (uint8_t i = 0; i < kAlarmCount; ++i) {
        alarms[i].enabled = record.alarm_enabled[i] != 0;
        alarms[i].hour = record.alarm_hour[i];
        alarms[i].minute = record.alarm_minute[i];
    }
    if (record.version >= kSettingsVersion) {
        settings->show_seconds = (record.app_flags & kSettingsFlagShowSeconds) != 0;
        settings->life_hourly_enabled =
            (record.app_flags & kSettingsFlagLifeHourly) != 0;
        settings->clock_style = record.clock_style;
        settings->temperature_offset_tenths_c =
            record.temperature_offset_tenths_c;
        const uint8_t ci = record.reserved[0];
        settings->life_cell_color_index = ci < kLifeColorCount ? ci : 0;
    } else if (record.version >= kSettingsVersionAppStyle) {
        settings->show_seconds = (record.app_flags & kSettingsFlagShowSeconds) != 0;
        settings->life_hourly_enabled =
            (record.app_flags & kSettingsFlagLifeHourly) != 0;
        settings->clock_style = record.clock_style;
        settings->temperature_offset_tenths_c =
            kTemperatureOffsetDefaultTenthsC;
    } else {
        *settings = default_app_settings();
    }
}

bool read_settings_slot(i2c_inst_t* i2c, uint16_t slot_address, SettingsRecord* record) {
    return eeprom_read_bytes(i2c, slot_address,
                             reinterpret_cast<uint8_t*>(record),
                             sizeof(*record));
}

}  // namespace

void set_default_alarms(AlarmSettings* alarms) {
    static constexpr AlarmSettings kDefaults[kAlarmCount] = {
        {false, 7, 30},
        {false, 8, 0},
        {false, 12, 0},
        {false, 18, 0},
        {false, 22, 0},
    };
    for (uint8_t i = 0; i < kAlarmCount; ++i) {
        alarms[i] = kDefaults[i];
    }
}

AppSettings default_app_settings() {
    AppSettings settings = {};
    settings.show_seconds = true;
    settings.life_hourly_enabled = false;
    settings.clock_style = kClockStyleDigital;
    settings.temperature_offset_tenths_c = kTemperatureOffsetDefaultTenthsC;
    settings.life_cell_color_index = 0;
    return settings;
}

bool alarms_equal(const AlarmSettings* a, const AlarmSettings* b) {
    for (uint8_t i = 0; i < kAlarmCount; ++i) {
        if (a[i].enabled != b[i].enabled ||
            a[i].hour != b[i].hour ||
            a[i].minute != b[i].minute) {
            return false;
        }
    }
    return true;
}

bool app_settings_equal(const AppSettings& a, const AppSettings& b) {
    return a.show_seconds == b.show_seconds &&
           a.life_hourly_enabled == b.life_hourly_enabled &&
           a.clock_style == b.clock_style &&
           a.temperature_offset_tenths_c == b.temperature_offset_tenths_c &&
           a.life_cell_color_index == b.life_cell_color_index;
}

bool alarm_settings_valid(const AlarmSettings* alarms) {
    for (uint8_t i = 0; i < kAlarmCount; ++i) {
        if (alarms[i].hour > 23 || alarms[i].minute > 59) {
            return false;
        }
    }
    return true;
}

bool app_settings_valid(const AppSettings& settings) {
    return (settings.clock_style == kClockStyleDigital ||
            settings.clock_style == kClockStyleAnalog ||
            settings.clock_style == kClockStyleCalendar) &&
           settings.temperature_offset_tenths_c >= kTemperatureOffsetMinTenthsC &&
           settings.temperature_offset_tenths_c <= kTemperatureOffsetMaxTenthsC;
}

bool load_settings_from_eeprom(i2c_inst_t* i2c,
                               AlarmSettings* alarms,
                               AppSettings* settings,
                               uint32_t* sequence) {
    SettingsRecord slot_a = {};
    SettingsRecord slot_b = {};
    const bool read_a = read_settings_slot(i2c, kSettingsSlotA, &slot_a);
    const bool read_b = read_settings_slot(i2c, kSettingsSlotB, &slot_b);
    const bool valid_a = read_a && settings_record_valid(slot_a);
    const bool valid_b = read_b && settings_record_valid(slot_b);

    if (!valid_a && !valid_b) {
        std::printf("SETTINGS eeprom load default slotA=%s slotB=%s\r\n",
                    valid_a ? "valid" : "invalid",
                    valid_b ? "valid" : "invalid");
        return false;
    }

    const SettingsRecord* selected = &slot_a;
    char selected_slot = 'A';
    if (!valid_a || (valid_b && slot_b.sequence > slot_a.sequence)) {
        selected = &slot_b;
        selected_slot = 'B';
    }

    apply_settings_record(*selected, alarms, settings);
    if (!alarm_settings_valid(alarms)) {
        std::puts("SETTINGS eeprom load default reason=invalid_alarm");
        return false;
    }
    *sequence = selected->sequence;
    std::printf("SETTINGS eeprom load ok slot=%c seq=%lu\r\n",
                selected_slot,
                static_cast<unsigned long>(*sequence));
    return true;
}

bool save_settings_to_eeprom(i2c_inst_t* i2c,
                             const AlarmSettings* alarms,
                             const AppSettings& settings,
                             uint32_t* sequence) {
    if (!alarm_settings_valid(alarms)) {
        std::puts("SETTINGS eeprom save fail reason=invalid_alarm");
        return false;
    }
    if (!app_settings_valid(settings)) {
        std::puts("SETTINGS eeprom save fail reason=invalid_app_settings");
        return false;
    }

    const uint32_t next_sequence = *sequence + 1u;
    const uint16_t slot = (next_sequence & 1u) ? kSettingsSlotA : kSettingsSlotB;
    const char slot_name = (slot == kSettingsSlotA) ? 'A' : 'B';
    SettingsRecord record = {};
    make_settings_record(alarms, settings, next_sequence, &record);

    if (!eeprom_write_record(i2c, slot, record)) {
        std::printf("SETTINGS eeprom save fail slot=%c seq=%lu\r\n",
                    slot_name,
                    static_cast<unsigned long>(next_sequence));
        return false;
    }

    *sequence = next_sequence;
    std::printf("SETTINGS eeprom save ok slot=%c seq=%lu\r\n",
                slot_name,
                static_cast<unsigned long>(*sequence));
    return true;
}
