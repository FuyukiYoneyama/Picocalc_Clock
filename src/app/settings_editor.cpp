#include "app/settings_editor.h"

#include <cstdio>

#include "platform/picocalc_display.h"

namespace {

constexpr uint16_t kBlack = 0x0000;
constexpr uint16_t kWhite = 0xffff;
constexpr uint16_t kDim = 0x7bef;
constexpr uint16_t kHighlight = 0xff80;
constexpr uint16_t kHighlightText = 0x0000;
constexpr int kRowCount = 4;

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
    constexpr int kRowH = 32;
    constexpr int kRowW = 232;

    picoment::display::clear(kBlack);
    picoment::display::draw_spleen_native_text_band(
        62, kTitleY, 196, 24, "SETTINGS",
        picoment::font::SpleenNativeSize::S12x24, kDim, kBlack);

    const char* seconds_value = model.settings.show_seconds ? "ON" : "OFF";
    const char* style_value = "DIGITAL";
    if (model.settings.clock_style == kClockStyleAnalog) {
        style_value = "ANALOG";
    } else if (model.settings.clock_style == kClockStyleCalendar) {
        style_value = "CALENDAR";
    }
    const char* life_value = model.settings.life_hourly_enabled ? "ON" : "OFF";
    char seconds_line[32];
    char style_line[32];
    char life_line[32];
    char offset_value[16];
    char offset_line[32];
    format_offset(offset_value, sizeof(offset_value),
                  model.settings.temperature_offset_tenths_c);
    std::snprintf(seconds_line, sizeof(seconds_line), "Seconds  %s", seconds_value);
    std::snprintf(style_line, sizeof(style_line), "Style    %s", style_value);
    std::snprintf(life_line, sizeof(life_line), "Life     %s", life_value);
    std::snprintf(offset_line, sizeof(offset_line), "TempOff  %s", offset_value);
    const char* rows[kRowCount] = {
        seconds_line,
        style_line,
        life_line,
        offset_line,
    };

    for (uint8_t row = 0; row < kRowCount; ++row) {
        const int y = kRowY + row * kRowH;
        const bool selected = model.selected_index == row;
        picoment::display::fill_rect(32, y, kRowW, 26,
                                     selected ? kHighlight : kBlack);
        picoment::display::draw_spleen_native_text_band(
            kRowX, y, kRowW - 24, 24, rows[row],
            picoment::font::SpleenNativeSize::S12x24,
            selected ? kHighlightText : kWhite,
            selected ? kHighlight : kBlack);
    }

    picoment::display::draw_text_band(
        34, 206, 252, 18, "TempOff applies to display", kDim, kBlack);
    picoment::display::draw_text_band(
        32, 250, 256, 18, model.status, kDim, kBlack);
}

void handle_settings_up_down(SettingsEditModel* model, int delta) {
    int row = static_cast<int>(model->selected_index) + delta;
    if (row < 0) {
        row = kRowCount - 1;
    } else if (row >= kRowCount) {
        row = 0;
    }
    model->selected_index = static_cast<uint8_t>(row);
    set_settings_status(model, "Left/Right changes");
}

void handle_settings_toggle(SettingsEditModel* model, int delta) {
    if (model->selected_index == 0) {
        model->settings.show_seconds = !model->settings.show_seconds;
        set_settings_status(model, "Enter=save Esc=cancel");
    } else if (model->selected_index == 1) {
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
    } else if (model->selected_index == 2) {
        model->settings.life_hourly_enabled =
            !model->settings.life_hourly_enabled;
        set_settings_status(model, "Enter=save Esc=cancel");
    } else {
        int next = static_cast<int>(model->settings.temperature_offset_tenths_c) +
                   delta;
        if (next < kTemperatureOffsetMinTenthsC) {
            next = kTemperatureOffsetMinTenthsC;
        } else if (next > kTemperatureOffsetMaxTenthsC) {
            next = kTemperatureOffsetMaxTenthsC;
        }
        model->settings.temperature_offset_tenths_c =
            static_cast<int8_t>(next);
        set_settings_status(model, "Left/Right +/-0.1C");
    }
}
