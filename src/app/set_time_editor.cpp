#include "app/set_time_editor.h"

#include <cstdio>
#include <cstring>

#include "clock/clock_time.h"
#include "platform/picocalc_display.h"

namespace {

constexpr uint16_t kBlack = 0x0000;
constexpr uint16_t kWhite = 0xffff;
constexpr uint16_t kDim = 0x7bef;
constexpr uint16_t kHighlight = 0xff80;
constexpr uint16_t kHighlightDigit = 0x07ff;
constexpr uint16_t kHighlightText = 0x0000;

TimeField next_field(TimeField field) {
    if (field == TimeField::Second) {
        return TimeField::Second;
    }
    return static_cast<TimeField>(static_cast<uint8_t>(field) + 1u);
}

TimeField previous_field(TimeField field) {
    if (field == TimeField::Year) {
        return TimeField::Year;
    }
    return static_cast<TimeField>(static_cast<uint8_t>(field) - 1u);
}

uint8_t field_digit_count(TimeField) {
    return 2;
}

int wrap_value(int value, int min_value, int max_value) {
    if (value > max_value) {
        return min_value;
    }
    if (value < min_value) {
        return max_value;
    }
    return value;
}

int field_min(const SetTimeModel&, TimeField field) {
    switch (field) {
    case TimeField::Year:
    case TimeField::Hour:
    case TimeField::Minute:
    case TimeField::Second:
        return 0;
    case TimeField::Month:
    case TimeField::Day:
        return 1;
    default:
        return 0;
    }
}

int field_max(const SetTimeModel& model, TimeField field) {
    switch (field) {
    case TimeField::Year:
        return 99;
    case TimeField::Month:
        return 12;
    case TimeField::Day:
        return days_in_month(model.year, model.month);
    case TimeField::Hour:
        return 23;
    case TimeField::Minute:
    case TimeField::Second:
        return 59;
    default:
        return 0;
    }
}

int get_field_value(const SetTimeModel& model, TimeField field) {
    switch (field) {
    case TimeField::Year:
        return model.year - 2000;
    case TimeField::Month:
        return model.month;
    case TimeField::Day:
        return model.day;
    case TimeField::Hour:
        return model.hour;
    case TimeField::Minute:
        return model.minute;
    case TimeField::Second:
        return model.second;
    default:
        return 0;
    }
}

void apply_preferred_day(SetTimeModel* model) {
    const uint8_t max_day = days_in_month(model->year, model->month);
    model->day = model->preferred_day > max_day ? max_day : model->preferred_day;
}

void set_field_value(SetTimeModel* model, TimeField field, int value) {
    switch (field) {
    case TimeField::Year:
        model->year = static_cast<uint16_t>(2000 + value);
        apply_preferred_day(model);
        break;
    case TimeField::Month:
        model->month = static_cast<uint8_t>(value);
        apply_preferred_day(model);
        break;
    case TimeField::Day:
        model->day = static_cast<uint8_t>(value);
        model->preferred_day = model->day;
        break;
    case TimeField::Hour:
        model->hour = static_cast<uint8_t>(value);
        break;
    case TimeField::Minute:
        model->minute = static_cast<uint8_t>(value);
        break;
    case TimeField::Second:
        model->second = static_cast<uint8_t>(value);
        break;
    }
}

void format_set_time_lines(const SetTimeModel& model,
                           char* date_line,
                           size_t date_len,
                           char* time_line,
                           size_t time_len) {
    std::snprintf(date_line, date_len, "%04u-%02u-%02u",
                  model.year, model.month, model.day);
    std::snprintf(time_line, time_len, "%02u:%02u:%02u",
                  model.hour, model.minute, model.second);
}

void field_position(TimeField field, int* row, int* start, int* width) {
    switch (field) {
    case TimeField::Year:
        *row = 0;
        *start = 2;
        *width = 2;
        break;
    case TimeField::Month:
        *row = 0;
        *start = 5;
        *width = 2;
        break;
    case TimeField::Day:
        *row = 0;
        *start = 8;
        *width = 2;
        break;
    case TimeField::Hour:
        *row = 1;
        *start = 0;
        *width = 2;
        break;
    case TimeField::Minute:
        *row = 1;
        *start = 3;
        *width = 2;
        break;
    case TimeField::Second:
        *row = 1;
        *start = 6;
        *width = 2;
        break;
    }
}

bool field_has_prefix_value(int value, int digit_count, int prefix_len, int prefix) {
    int divisor = 1;
    for (int i = 0; i < digit_count - prefix_len; ++i) {
        divisor *= 10;
    }
    return value / divisor == prefix;
}

bool find_lowest_valid_with_prefix(const SetTimeModel& model,
                                   TimeField field,
                                   int prefix_len,
                                   int prefix,
                                   int* value) {
    const int min_value = field_min(model, field);
    const int max_value = field_max(model, field);
    const int digit_count = field_digit_count(field);
    for (int candidate = min_value; candidate <= max_value; ++candidate) {
        if (field_has_prefix_value(candidate, digit_count, prefix_len, prefix)) {
            *value = candidate;
            return true;
        }
    }
    return false;
}

bool replace_digit_prefix_valid(SetTimeModel* model, uint8_t digit) {
    const TimeField field = model->field;
    const int index = model->digit_index;
    const int current = get_field_value(*model, field);
    const int tens = current / 10;
    const int ones = current % 10;
    int digits[2] = {tens, ones};
    digits[index] = digit;
    const int candidate = digits[0] * 10 + digits[1];
    const int prefix_len = index + 1;
    int prefix = 0;
    for (int i = 0; i < prefix_len; ++i) {
        prefix = prefix * 10 + digits[i];
    }

    const int min_value = field_min(*model, field);
    const int max_value = field_max(*model, field);
    int normalized = candidate;
    if (candidate < min_value || candidate > max_value) {
        if (!find_lowest_valid_with_prefix(*model, field, prefix_len, prefix,
                                           &normalized)) {
            set_status(model, "Invalid digit");
            return false;
        }
    }

    set_field_value(model, field, normalized);
    set_status(model, "Enter=save Esc=cancel");
    return true;
}

void select_field(SetTimeModel* model, TimeField field) {
    model->field = field;
    model->selection = SelectionMode::Field;
    model->digit_index = 0;
}

void advance_after_digit(SetTimeModel* model) {
    const uint8_t count = field_digit_count(model->field);
    if (model->digit_index + 1u < count) {
        model->selection = SelectionMode::Digit;
        ++model->digit_index;
        return;
    }
    if (model->field == TimeField::Second) {
        select_field(model, TimeField::Second);
    } else {
        select_field(model, next_field(model->field));
    }
}

void commit_day_if_needed(SetTimeModel* model) {
    if (model->field == TimeField::Day) {
        model->preferred_day = model->day;
    }
}

}  // namespace

void set_status(SetTimeModel* model, const char* text) {
    std::snprintf(model->status, sizeof(model->status), "%s", text);
}

SetTimeModel make_set_time_model(const ds3231_datetime_t& dt) {
    SetTimeModel model = {};
    model.year = dt.year;
    model.month = dt.month;
    model.day = dt.day;
    model.hour = dt.hour;
    model.minute = dt.minute;
    model.second = dt.second;
    model.preferred_day = dt.day;
    model.field = TimeField::Year;
    model.selection = SelectionMode::Field;
    model.digit_index = 0;
    set_status(&model, "Enter=save Esc=cancel");
    return model;
}

void draw_set_time_screen(const SetTimeModel& model) {
    char date_line[16];
    char time_line[12];
    format_set_time_lines(model, date_line, sizeof(date_line),
                          time_line, sizeof(time_line));

    constexpr int kTitleY = 22;
    constexpr int kDateX = 80;
    constexpr int kDateYSet = 94;
    constexpr int kTimeXSet = 96;
    constexpr int kTimeYSet = 150;
    constexpr int kCharW = 16;
    constexpr int kCharH = 32;

    picoment::display::clear(kBlack);
    picoment::display::draw_spleen_native_text_band(
        62, kTitleY, 196, 24, "SET TIME",
        picoment::font::SpleenNativeSize::S12x24, kDim, kBlack);

    int selected_row = 0;
    int selected_start = 0;
    int selected_width = 0;
    field_position(model.field, &selected_row, &selected_start, &selected_width);
    if (model.selection == SelectionMode::Digit) {
        selected_start += model.digit_index;
        selected_width = 1;
    }

    const char* rows[2] = {date_line, time_line};
    const int row_x[2] = {kDateX, kTimeXSet};
    const int row_y[2] = {kDateYSet, kTimeYSet};
    for (int row = 0; row < 2; ++row) {
        const int len = static_cast<int>(std::strlen(rows[row]));
        for (int i = 0; i < len; ++i) {
            const bool selected =
                row == selected_row &&
                i >= selected_start &&
                i < selected_start + selected_width;
            char ch[2] = {rows[row][i], '\0'};
            picoment::display::draw_spleen_native_text_band(
                row_x[row] + i * kCharW, row_y[row],
                kCharW, kCharH, ch,
                picoment::font::SpleenNativeSize::S16x32,
                selected ? kHighlightText : kWhite,
                selected ? (model.selection == SelectionMode::Digit ? kHighlightDigit
                                                                     : kHighlight)
                         : kBlack);
        }
    }

    picoment::display::draw_text_band(
        32, 250, 256, 18, model.status, kDim, kBlack);
}

void handle_set_time_left(SetTimeModel* model) {
    commit_day_if_needed(model);
    if (model->selection == SelectionMode::Digit) {
        if (model->digit_index > 0) {
            --model->digit_index;
        } else {
            select_field(model, previous_field(model->field));
        }
    } else {
        select_field(model, previous_field(model->field));
    }
}

void handle_set_time_right(SetTimeModel* model) {
    commit_day_if_needed(model);
    if (model->selection == SelectionMode::Digit) {
        if (model->digit_index + 1u < field_digit_count(model->field)) {
            ++model->digit_index;
        } else {
            select_field(model, next_field(model->field));
        }
    } else {
        select_field(model, next_field(model->field));
    }
}

void handle_set_time_up_down(SetTimeModel* model, int delta) {
    if (model->selection == SelectionMode::Field) {
        const int min_value = field_min(*model, model->field);
        const int max_value = field_max(*model, model->field);
        const int value = wrap_value(get_field_value(*model, model->field) + delta,
                                     min_value, max_value);
        set_field_value(model, model->field, value);
        set_status(model, "Enter=save Esc=cancel");
        return;
    }

    const int current = get_field_value(*model, model->field);
    int digits[2] = {current / 10, current % 10};
    digits[model->digit_index] = wrap_value(digits[model->digit_index] + delta, 0, 9);
    const int candidate = digits[0] * 10 + digits[1];
    const int min_value = field_min(*model, model->field);
    const int max_value = field_max(*model, model->field);
    set_field_value(model, model->field,
                    candidate < min_value ? min_value :
                    candidate > max_value ? max_value : candidate);
    commit_day_if_needed(model);
    set_status(model, "Enter=save Esc=cancel");
}

void handle_set_time_digit(SetTimeModel* model, uint8_t digit) {
    if (model->selection == SelectionMode::Field) {
        model->selection = SelectionMode::Digit;
        model->digit_index = 0;
    }
    if (replace_digit_prefix_valid(model, digit)) {
        advance_after_digit(model);
    }
}

ds3231_datetime_t model_to_datetime(const SetTimeModel& model) {
    ds3231_datetime_t dt = {};
    dt.year = model.year;
    dt.month = model.month;
    dt.day = model.day;
    dt.hour = model.hour;
    dt.minute = model.minute;
    dt.second = model.second;
    dt.day_of_week = ds3231_calculate_day_of_week(dt.year, dt.month, dt.day);
    return dt;
}
