#pragma once

#include "hardware/i2c.h"

bool uart_should_stay_awake();
void print_uart_help(i2c_inst_t* i2c);
bool poll_uart_commands(i2c_inst_t* i2c);
