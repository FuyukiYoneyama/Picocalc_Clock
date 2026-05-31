#ifndef PICOCALC_CLOCK_SETTINGS_STORE_H
#define PICOCALC_CLOCK_SETTINGS_STORE_H

#include <cstdint>

#include "hardware/i2c.h"
#include "settings/settings_model.h"

void set_default_alarms(AlarmSettings* alarms);
AppSettings default_app_settings();
bool alarms_equal(const AlarmSettings* a, const AlarmSettings* b);
bool app_settings_equal(const AppSettings& a, const AppSettings& b);
bool alarm_settings_valid(const AlarmSettings* alarms);
bool app_settings_valid(const AppSettings& settings);
bool load_settings_from_eeprom(i2c_inst_t* i2c,
                               AlarmSettings* alarms,
                               AppSettings* settings,
                               uint32_t* sequence);
bool save_settings_to_eeprom(i2c_inst_t* i2c,
                             const AlarmSettings* alarms,
                             const AppSettings& settings,
                             uint32_t* sequence);

#endif
