#include "alarm/alarm_model.h"

#include <cstdio>

bool same_alarm_minute(const AlarmFireRecord& record,
                       const ds3231_datetime_t& dt) {
    return record.valid &&
           record.year == dt.year &&
           record.month == dt.month &&
           record.day == dt.day &&
           record.hour == dt.hour &&
           record.minute == dt.minute;
}

void record_alarm_minute(AlarmFireRecord* record, const ds3231_datetime_t& dt) {
    record->year = dt.year;
    record->month = dt.month;
    record->day = dt.day;
    record->hour = dt.hour;
    record->minute = dt.minute;
    record->valid = true;
}

AlarmMatch find_alarm_match(const AlarmSettings* alarms,
                            const ds3231_datetime_t& dt) {
    AlarmMatch match = {};
    match.first_index = 0xffu;
    match.hour = dt.hour;
    match.minute = dt.minute;
    for (uint8_t i = 0; i < kAlarmCount; ++i) {
        if (!alarms[i].enabled ||
            alarms[i].hour != dt.hour ||
            alarms[i].minute != dt.minute) {
            continue;
        }
        if (!match.found) {
            match.found = true;
            match.first_index = i;
        }
        ++match.count;
    }
    return match;
}

namespace {

int alarm_minutes_until(uint8_t now_hour,
                        uint8_t now_minute,
                        const AlarmSettings& alarm) {
    const int now_total = now_hour * 60 + now_minute;
    const int alarm_total = alarm.hour * 60 + alarm.minute;
    int delta = alarm_total - now_total;
    if (delta <= 0) {
        delta += 24 * 60;
    }
    return delta;
}

}  // namespace

AlarmMatch find_next_alarm(const AlarmSettings* alarms,
                           const ds3231_datetime_t& dt,
                           const AlarmFireRecord& last_fire) {
    AlarmMatch next = {};
    next.first_index = 0xffu;
    int best_delta = 24 * 60 + 1;

    for (uint8_t i = 0; i < kAlarmCount; ++i) {
        if (!alarms[i].enabled) {
            continue;
        }
        int delta = alarm_minutes_until(dt.hour, dt.minute, alarms[i]);
        if (delta == 24 * 60 && same_alarm_minute(last_fire, dt)) {
            continue;
        }
        if (delta < best_delta) {
            best_delta = delta;
            next.found = true;
            next.first_index = i;
            next.count = 1;
            next.hour = alarms[i].hour;
            next.minute = alarms[i].minute;
        } else if (delta == best_delta &&
                   next.found &&
                   alarms[i].hour == next.hour &&
                   alarms[i].minute == next.minute) {
            ++next.count;
        }
    }

    return next;
}

void format_alarm_label(const AlarmMatch& match, char* text, size_t len) {
    if (!match.found) {
        std::snprintf(text, len, "Alm OFF");
        return;
    }
    std::snprintf(text, len, "Next A%u%s %02u:%02u",
                  match.first_index + 1u,
                  match.count > 1 ? "+" : "",
                  match.hour,
                  match.minute);
}
