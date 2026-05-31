#ifndef PICOCALC_CLOCK_SETTINGS_EDITOR_H
#define PICOCALC_CLOCK_SETTINGS_EDITOR_H

#include <cstdint>

#include "settings/settings_model.h"

struct SettingsEditModel {
    AppSettings settings;
    uint8_t selected_index;
    char status[32];
};

void set_settings_status(SettingsEditModel* model, const char* text);
SettingsEditModel make_settings_edit_model(const AppSettings& settings);
void draw_settings_screen(const SettingsEditModel& model);
void handle_settings_up_down(SettingsEditModel* model, int delta);
void handle_settings_toggle(SettingsEditModel* model);

#endif
