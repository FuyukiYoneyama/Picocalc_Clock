#ifndef PICOCALC_CLOCK_ALARM_MODEL_H
#define PICOCALC_CLOCK_ALARM_MODEL_H

#include <cstdint>

constexpr uint8_t kAlarmCount = 5;

struct AlarmSettings {
    bool enabled;
    uint8_t hour;
    uint8_t minute;
};

#endif
