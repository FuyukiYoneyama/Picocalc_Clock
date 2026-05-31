#ifndef PICOCALC_CLOCK_ALARM_UI_H
#define PICOCALC_CLOCK_ALARM_UI_H

#include <cstdint>

#include "alarm/alarm_model.h"

enum class AlarmField : uint8_t {
    Hour,
    Minute,
    Enabled,
};

enum class AlarmSelectionMode : uint8_t {
    Row,
    Field,
    Digit,
};

struct AlarmEditModel {
    AlarmSettings alarms[kAlarmCount];
    uint8_t selected_index;
    AlarmField field;
    AlarmSelectionMode selection;
    uint8_t digit_index;
    char status[32];
};

AlarmEditModel make_alarm_edit_model(const AlarmSettings* alarms);
void draw_set_alarm_screen(const AlarmEditModel& model);
void draw_set_alarm_screen_full(const AlarmEditModel& model);
void draw_alarm_ringing_screen(const AlarmMatch& match);
void handle_alarm_left(AlarmEditModel* model);
void handle_alarm_right(AlarmEditModel* model);
void handle_alarm_up_down(AlarmEditModel* model, int delta);
void handle_alarm_digit(AlarmEditModel* model, uint8_t digit);

#endif
