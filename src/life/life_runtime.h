#pragma once

#include <stdint.h>

#include "ds3231.h"
#include "life_board.h"

struct LifeHourRecord {
    uint16_t year;
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    bool valid;
};

struct LifeRuntime {
    life::Board board;
    life::StabilityTracker tracker;
    bool active;
    bool hourly;
    uint16_t cell_color;
    uint32_t started_ms;
    uint32_t generation;
    uint32_t live_count;
};

void start_life(LifeRuntime* life_state, bool hourly, uint32_t now_ms, uint16_t cell_color);
bool step_life(LifeRuntime* life_state);
void stop_life(LifeRuntime* life_state, const char* reason);
void draw_life_time_overlay(const ds3231_datetime_t& dt, bool rtc_ok, bool show_seconds);

bool same_life_hour(const LifeHourRecord& record,
                    const ds3231_datetime_t& dt);
void record_life_hour(LifeHourRecord* record,
                      const ds3231_datetime_t& dt);
