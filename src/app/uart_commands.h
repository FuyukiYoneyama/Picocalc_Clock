#pragma once

#include "hardware/i2c.h"
#include "settings/settings_model.h"

bool uart_should_stay_awake();
void print_uart_help(i2c_inst_t* i2c, const AppSettings& settings);
bool poll_uart_commands(i2c_inst_t* i2c, const AppSettings& settings);
