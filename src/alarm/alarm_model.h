#ifndef PICOCALC_CLOCK_ALARM_MODEL_H
#define PICOCALC_CLOCK_ALARM_MODEL_H

#include <cstddef>
#include <cstdint>

#include "ds3231.h"

constexpr uint8_t kAlarmCount = 5;

struct AlarmSettings {
    bool enabled;
    uint8_t hour;
    uint8_t minute;
};

struct AlarmFireRecord {
    uint16_t year;
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t minute;
    bool valid;
};

struct AlarmMatch {
    bool found;
    uint8_t first_index;
    uint8_t count;
    uint8_t hour;
    uint8_t minute;
};

bool same_alarm_minute(const AlarmFireRecord& record,
                       const ds3231_datetime_t& dt);
void record_alarm_minute(AlarmFireRecord* record, const ds3231_datetime_t& dt);
AlarmMatch find_alarm_match(const AlarmSettings* alarms,
                            const ds3231_datetime_t& dt);
AlarmMatch find_next_alarm(const AlarmSettings* alarms,
                           const ds3231_datetime_t& dt,
                           const AlarmFireRecord& last_fire);
void format_alarm_label(const AlarmMatch& match, char* text, size_t len);

#endif
