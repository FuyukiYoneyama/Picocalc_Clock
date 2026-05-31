#ifndef PICOCALC_CLOCK_SET_TIME_EDITOR_H
#define PICOCALC_CLOCK_SET_TIME_EDITOR_H

#include <cstdint>

#include "ds3231.h"

enum class TimeField : uint8_t {
    Year = 0,
    Month,
    Day,
    Hour,
    Minute,
    Second,
};

enum class SelectionMode : uint8_t {
    Field,
    Digit,
};

struct SetTimeModel {
    uint16_t year;
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
    uint8_t preferred_day;
    TimeField field;
    SelectionMode selection;
    uint8_t digit_index;
    char status[24];
};

void set_status(SetTimeModel* model, const char* text);
SetTimeModel make_set_time_model(const ds3231_datetime_t& dt);
void draw_set_time_screen(const SetTimeModel& model);
void handle_set_time_left(SetTimeModel* model);
void handle_set_time_right(SetTimeModel* model);
void handle_set_time_up_down(SetTimeModel* model, int delta);
void handle_set_time_digit(SetTimeModel* model, uint8_t digit);
ds3231_datetime_t model_to_datetime(const SetTimeModel& model);

#endif
