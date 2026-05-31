#include "alarm/alarm_ui.h"

#include <cstdio>
#include <cstring>

#include "platform/picocalc_display.h"

namespace {

constexpr uint16_t kBlack = 0x0000;
constexpr uint16_t kWhite = 0xffff;
constexpr uint16_t kDim = 0x7bef;
constexpr uint16_t kWarn = 0xfde0;
constexpr uint16_t kHighlight = 0xff80;
constexpr uint16_t kHighlightDigit = 0x07ff;
constexpr uint16_t kHighlightText = 0x0000;

void set_alarm_status(AlarmEditModel* model, const char* text) {
    std::snprintf(model->status, sizeof(model->status), "%s", text);
}

int wrap_value(int value, int min_value, int max_value) {
    if (value < min_value) {
        return max_value;
    }
    if (value > max_value) {
        return min_value;
    }
    return value;
}

bool field_has_prefix_value(int value, int digit_count, int prefix_len, int prefix) {
    int divisor = 1;
    for (int i = 0; i < digit_count - prefix_len; ++i) {
        divisor *= 10;
    }
    return value / divisor == prefix;
}

AlarmField previous_alarm_field(AlarmField field) {
    if (field == AlarmField::Hour) {
        return AlarmField::Hour;
    }
    return static_cast<AlarmField>(static_cast<uint8_t>(field) - 1u);
}

AlarmField next_alarm_field(AlarmField field) {
    if (field == AlarmField::Enabled) {
        return AlarmField::Enabled;
    }
    return static_cast<AlarmField>(static_cast<uint8_t>(field) + 1u);
}

int get_alarm_field_value(const AlarmEditModel& model, AlarmField field) {
    const AlarmSettings& alarm = model.alarms[model.selected_index];
    switch (field) {
    case AlarmField::Hour:
        return alarm.hour;
    case AlarmField::Minute:
        return alarm.minute;
    case AlarmField::Enabled:
        return alarm.enabled ? 1 : 0;
    default:
        return 0;
    }
}

void set_alarm_field_value(AlarmEditModel* model, AlarmField field, int value) {
    AlarmSettings& alarm = model->alarms[model->selected_index];
    switch (field) {
    case AlarmField::Hour:
        alarm.hour = static_cast<uint8_t>(value);
        break;
    case AlarmField::Minute:
        alarm.minute = static_cast<uint8_t>(value);
        break;
    case AlarmField::Enabled:
        alarm.enabled = value != 0;
        break;
    }
}

int alarm_field_max(AlarmField field) {
    switch (field) {
    case AlarmField::Hour:
        return 23;
    case AlarmField::Minute:
        return 59;
    case AlarmField::Enabled:
        return 1;
    default:
        return 0;
    }
}

bool find_alarm_lowest_valid_with_prefix(AlarmField field,
                                         int prefix_len,
                                         int prefix,
                                         int* value) {
    const int max_value = alarm_field_max(field);
    for (int candidate = 0; candidate <= max_value; ++candidate) {
        if (field_has_prefix_value(candidate, 2, prefix_len, prefix)) {
            *value = candidate;
            return true;
        }
    }
    return false;
}

bool replace_alarm_digit_prefix_valid(AlarmEditModel* model, uint8_t digit) {
    if (model->field == AlarmField::Enabled) {
        return false;
    }
    const int current = get_alarm_field_value(*model, model->field);
    int digits[2] = {current / 10, current % 10};
    digits[model->digit_index] = digit;
    const int candidate = digits[0] * 10 + digits[1];
    const int prefix_len = model->digit_index + 1;
    int prefix = 0;
    for (int i = 0; i < prefix_len; ++i) {
        prefix = prefix * 10 + digits[i];
    }

    int normalized = candidate;
    if (candidate > alarm_field_max(model->field)) {
        if (!find_alarm_lowest_valid_with_prefix(model->field,
                                                 prefix_len,
                                                 prefix,
                                                 &normalized)) {
            set_alarm_status(model, "Invalid digit");
            return false;
        }
    }

    set_alarm_field_value(model, model->field, normalized);
    set_alarm_status(model, "Enter=save Esc=back");
    return true;
}

}  // namespace

AlarmEditModel make_alarm_edit_model(const AlarmSettings* alarms) {
    AlarmEditModel model = {};
    for (uint8_t i = 0; i < kAlarmCount; ++i) {
        model.alarms[i] = alarms[i];
    }
    model.selected_index = 0;
    model.field = AlarmField::Hour;
    model.selection = AlarmSelectionMode::Row;
    model.digit_index = 0;
    set_alarm_status(&model, "Enter=save Esc=back");
    return model;
}

void draw_set_alarm_screen(const AlarmEditModel& model) {
    constexpr int kRowX = 44;
    constexpr int kRowY = 64;
    constexpr int kRowH = 26;
    constexpr int kCharW = 12;
    constexpr int kFieldHourStart = 3;
    constexpr int kFieldMinuteStart = 6;
    constexpr int kFieldEnabledStart = 9;

    for (uint8_t row = 0; row < kAlarmCount; ++row) {
        char line[24];
        std::snprintf(line, sizeof(line), "A%u %02u:%02u %s",
                      row + 1u,
                      model.alarms[row].hour,
                      model.alarms[row].minute,
                      model.alarms[row].enabled ? "ON" : "OFF");
        const int y = kRowY + row * kRowH;
        const bool selected_row =
            model.selected_index == row &&
            model.selection == AlarmSelectionMode::Row;
        if (selected_row) {
            picoment::display::fill_rect(32, y, 256, kRowH, kHighlight);
        } else {
            picoment::display::fill_rect(32, y, 256, kRowH, kBlack);
        }

        const int len = static_cast<int>(std::strlen(line));
        for (int i = 0; i < len; ++i) {
            bool selected_field = false;
            if (model.selected_index == row &&
                model.selection != AlarmSelectionMode::Row) {
                int start = kFieldHourStart;
                int width = 2;
                if (model.field == AlarmField::Minute) {
                    start = kFieldMinuteStart;
                } else if (model.field == AlarmField::Enabled) {
                    start = kFieldEnabledStart;
                    width = model.alarms[row].enabled ? 2 : 3;
                }
                if (model.selection == AlarmSelectionMode::Digit) {
                    start += model.digit_index;
                    width = 1;
                }
                selected_field = i >= start && i < start + width;
            }

            char ch[2] = {line[i], '\0'};
            picoment::display::draw_spleen_native_text_band(
                kRowX + i * kCharW, y, kCharW, 24, ch,
                picoment::font::SpleenNativeSize::S12x24,
                selected_row || selected_field ? kHighlightText : kWhite,
                selected_field ? (model.selection == AlarmSelectionMode::Digit
                                      ? kHighlightDigit
                                      : kHighlight)
                               : (selected_row ? kHighlight : kBlack));
        }
    }

    picoment::display::draw_text_band(
        32, 250, 256, 18, model.status, kDim, kBlack);
}

void draw_set_alarm_screen_full(const AlarmEditModel& model) {
    constexpr int kTitleY = 18;
    picoment::display::clear(kBlack);
    picoment::display::draw_spleen_native_text_band(
        68, kTitleY, 184, 24, "SET ALARM",
        picoment::font::SpleenNativeSize::S12x24, kDim, kBlack);
    draw_set_alarm_screen(model);
}

void draw_alarm_ringing_screen(const AlarmMatch& match) {
    char title[32];
    if (match.count > 1) {
        std::snprintf(title, sizeof(title), "ALARM A%u+",
                      match.first_index + 1u);
    } else {
        std::snprintf(title, sizeof(title), "ALARM A%u",
                      match.first_index + 1u);
    }
    char time_line[12];
    std::snprintf(time_line, sizeof(time_line), "%02u:%02u",
                  match.hour, match.minute);

    picoment::display::clear(kBlack);
    picoment::display::draw_spleen_native_text_band(
        62, 48, 196, 32, title,
        picoment::font::SpleenNativeSize::S16x32, kWarn, kBlack);
    picoment::display::draw_spleen_native_text_band(
        80, 124, 160, 64, time_line,
        picoment::font::SpleenNativeSize::S32x64, kWhite, kBlack);
    picoment::display::draw_text_band(
        104, 232, 112, 18, "Space: Stop", kDim, kBlack);
}

void handle_alarm_left(AlarmEditModel* model) {
    if (model->selection == AlarmSelectionMode::Row) {
        model->selection = AlarmSelectionMode::Field;
        model->field = AlarmField::Hour;
        model->digit_index = 0;
    } else if (model->selection == AlarmSelectionMode::Field) {
        if (model->field == AlarmField::Hour) {
            model->selection = AlarmSelectionMode::Row;
        } else {
            model->field = previous_alarm_field(model->field);
        }
    } else if (model->digit_index > 0) {
        --model->digit_index;
    } else {
        model->selection = AlarmSelectionMode::Field;
    }
}

void handle_alarm_right(AlarmEditModel* model) {
    if (model->selection == AlarmSelectionMode::Row) {
        model->selection = AlarmSelectionMode::Field;
        model->field = AlarmField::Hour;
        model->digit_index = 0;
    } else if (model->selection == AlarmSelectionMode::Field) {
        if (model->field == AlarmField::Enabled) {
            model->selection = AlarmSelectionMode::Row;
        } else {
            model->field = next_alarm_field(model->field);
        }
    } else if (model->digit_index == 0) {
        ++model->digit_index;
    } else {
        model->selection = AlarmSelectionMode::Field;
    }
}

void handle_alarm_up_down(AlarmEditModel* model, int delta) {
    if (model->selection == AlarmSelectionMode::Row) {
        int row = static_cast<int>(model->selected_index) - delta;
        if (row < 0) {
            row = kAlarmCount - 1;
        } else if (row >= kAlarmCount) {
            row = 0;
        }
        model->selected_index = static_cast<uint8_t>(row);
        set_alarm_status(model, "Enter=save Esc=back");
        return;
    }

    if (model->selection == AlarmSelectionMode::Field) {
        const int max_value = alarm_field_max(model->field);
        const int value = wrap_value(get_alarm_field_value(*model, model->field) + delta,
                                     0, max_value);
        set_alarm_field_value(model, model->field, value);
        set_alarm_status(model, "Enter=save Esc=back");
        return;
    }

    int current = get_alarm_field_value(*model, model->field);
    int digits[2] = {current / 10, current % 10};
    digits[model->digit_index] = wrap_value(digits[model->digit_index] + delta, 0, 9);
    int candidate = digits[0] * 10 + digits[1];
    if (candidate > alarm_field_max(model->field)) {
        candidate = alarm_field_max(model->field);
    }
    set_alarm_field_value(model, model->field, candidate);
    set_alarm_status(model, "Enter=save Esc=back");
}

void handle_alarm_digit(AlarmEditModel* model, uint8_t digit) {
    if (model->selection == AlarmSelectionMode::Row ||
        model->field == AlarmField::Enabled) {
        return;
    }
    if (model->selection == AlarmSelectionMode::Field) {
        model->selection = AlarmSelectionMode::Digit;
        model->digit_index = 0;
    }
    if (!replace_alarm_digit_prefix_valid(model, digit)) {
        return;
    }
    if (model->digit_index == 0) {
        ++model->digit_index;
    } else {
        model->selection = AlarmSelectionMode::Field;
    }
}
