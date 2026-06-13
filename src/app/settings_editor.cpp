#include "app/settings_editor.h"

#include <cstdio>

#include "platform/picocalc_display.h"
#include "settings/settings_model.h"

namespace {

constexpr uint16_t kBlack = 0x0000;
constexpr uint16_t kWhite = 0xffff;
constexpr uint16_t kDim = 0x7bef;
constexpr uint16_t kHighlight = 0xff80;
constexpr uint16_t kHighlightText = 0x0000;
constexpr int kRowCountBase = 4;

int effective_row_count(const AppSettings& s) {
    return s.life_hourly_enabled ? kRowCountBase + 1 : kRowCountBase;
}

// Logical slot for each row. When Life is OFF, LifeColor row is absent and
// TempOff appears at index 3. When Life is ON, LifeColor is at index 3 and
// TempOff at index 4.
enum class SettingsSlot { Seconds, Style, Life, LifeColor, TempOff };

SettingsSlot row_to_slot(int row, bool life_on) {
    if (row == 0) return SettingsSlot::Seconds;
    if (row == 1) return SettingsSlot::Style;
    if (row == 2) return SettingsSlot::Life;
    if (life_on && row == 3) return SettingsSlot::LifeColor;
    return SettingsSlot::TempOff;
}

void format_offset(char* text, size_t len, int8_t offset_tenths_c) {
    const char sign = offset_tenths_c < 0 ? '-' : '+';
    const int abs_value = offset_tenths_c < 0 ? -offset_tenths_c : offset_tenths_c;
    std::snprintf(text, len, "%c%d.%dC", sign, abs_value / 10, abs_value % 10);
}

}  // namespace

void set_settings_status(SettingsEditModel* model, const char* text) {
    std::snprintf(model->status, sizeof(model->status), "%s", text);
}

SettingsEditModel make_settings_edit_model(const AppSettings& settings) {
    SettingsEditModel model = {};
    model.settings = settings;
    model.selected_index = 0;
    set_settings_status(&model, "Enter=save Esc=cancel");
    return model;
}

void draw_settings_screen(const SettingsEditModel& model) {
    constexpr int kTitleY = 28;
    constexpr int kRowX = 44;
    constexpr int kRowY = 70;
    constexpr int kRowH = 28;
    constexpr int kRowW = 232;

    picoment::display::clear(kBlack);
    picoment::display::draw_spleen_native_text_band(
        62, kTitleY, 196, 24, "SETTINGS",
        picoment::font::SpleenNativeSize::S12x24, kDim, kBlack);

    const bool life_on = model.settings.life_hourly_enabled;
    const int row_count = effective_row_count(model.settings);

    const char* seconds_value = model.settings.show_seconds ? "ON" : "OFF";
    const char* style_value = "DIGITAL";
    if (model.settings.clock_style == kClockStyleAnalog) {
        style_value = "ANALOG";
    } else if (model.settings.clock_style == kClockStyleCalendar) {
        style_value = "CALENDAR";
    }
    const char* life_value = life_on ? "ON" : "OFF";
    const uint8_t ci = model.settings.life_cell_color_index < kLifeColorCount
                           ? model.settings.life_cell_color_index : 0;
    char seconds_line[32], style_line[32], life_line[32];
    char life_col_line[32], offset_value[16], offset_line[32];
    format_offset(offset_value, sizeof(offset_value),
                  model.settings.temperature_offset_tenths_c);
    std::snprintf(seconds_line, sizeof(seconds_line), "Seconds  %s", seconds_value);
    std::snprintf(style_line,   sizeof(style_line),   "Style    %s", style_value);
    std::snprintf(life_line,    sizeof(life_line),    "Life     %s", life_value);
    std::snprintf(life_col_line, sizeof(life_col_line), "LifeCol  %s", kLifeColorNames[ci]);
    std::snprintf(offset_line,  sizeof(offset_line),  "TempOff  %s", offset_value);

    for (int row = 0; row < row_count; ++row) {
        const SettingsSlot slot = row_to_slot(row, life_on);
        const char* label = "";
        switch (slot) {
        case SettingsSlot::Seconds:  label = seconds_line;  break;
        case SettingsSlot::Style:    label = style_line;    break;
        case SettingsSlot::Life:     label = life_line;     break;
        case SettingsSlot::LifeColor: label = life_col_line; break;
        case SettingsSlot::TempOff:  label = offset_line;  break;
        }
        const int y = kRowY + row * kRowH;
        const bool selected = model.selected_index == row;
        picoment::display::fill_rect(32, y, kRowW, 24,
                                     selected ? kHighlight : kBlack);
        picoment::display::draw_spleen_native_text_band(
            kRowX, y, kRowW - 24, 24, label,
            picoment::font::SpleenNativeSize::S12x24,
            selected ? kHighlightText : kWhite,
            selected ? kHighlight : kBlack);
    }

    picoment::display::draw_text_band(
        32, 242, 256, 16, model.status, kDim, kBlack);
}

void handle_settings_up_down(SettingsEditModel* model, int delta) {
    const int row_count = effective_row_count(model->settings);
    int row = static_cast<int>(model->selected_index) + delta;
    if (row < 0) {
        row = row_count - 1;
    } else if (row >= row_count) {
        row = 0;
    }
    model->selected_index = static_cast<uint8_t>(row);
    set_settings_status(model, "Left/Right changes");
}

void handle_settings_toggle(SettingsEditModel* model, int delta) {
    const bool life_on = model->settings.life_hourly_enabled;
    const SettingsSlot slot = row_to_slot(model->selected_index, life_on);

    switch (slot) {
    case SettingsSlot::Seconds:
        model->settings.show_seconds = !model->settings.show_seconds;
        set_settings_status(model, "Enter=save Esc=cancel");
        break;
    case SettingsSlot::Style:
        if (delta < 0) {
            if (model->settings.clock_style == kClockStyleDigital) {
                model->settings.clock_style = kClockStyleCalendar;
            } else if (model->settings.clock_style == kClockStyleAnalog) {
                model->settings.clock_style = kClockStyleDigital;
            } else {
                model->settings.clock_style = kClockStyleAnalog;
            }
        } else if (model->settings.clock_style == kClockStyleDigital) {
            model->settings.clock_style = kClockStyleAnalog;
        } else if (model->settings.clock_style == kClockStyleAnalog) {
            model->settings.clock_style = kClockStyleCalendar;
        } else {
            model->settings.clock_style = kClockStyleDigital;
        }
        set_settings_status(model, "Enter=save Esc=cancel");
        break;
    case SettingsSlot::Life:
        model->settings.life_hourly_enabled = !life_on;
        // If Life was just turned OFF and cursor was beyond the Life row, pull it back.
        if (life_on && model->selected_index > 2) {
            model->selected_index = 2;
        }
        set_settings_status(model, "Enter=save Esc=cancel");
        break;
    case SettingsSlot::LifeColor: {
        const int count = static_cast<int>(kLifeColorCount);
        int ci = static_cast<int>(model->settings.life_cell_color_index) + delta;
        ci = ((ci % count) + count) % count;
        model->settings.life_cell_color_index = static_cast<uint8_t>(ci);
        set_settings_status(model, "Enter=save Esc=cancel");
        break;
    }
    case SettingsSlot::TempOff: {
        int next = static_cast<int>(model->settings.temperature_offset_tenths_c) + delta;
        if (next < kTemperatureOffsetMinTenthsC) next = kTemperatureOffsetMinTenthsC;
        if (next > kTemperatureOffsetMaxTenthsC) next = kTemperatureOffsetMaxTenthsC;
        model->settings.temperature_offset_tenths_c = static_cast<int8_t>(next);
        set_settings_status(model, "Left/Right +/-0.1C");
        break;
    }
    }
}
