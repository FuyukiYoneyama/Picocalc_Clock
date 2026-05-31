#ifndef PICOCALC_CLOCK_PLATFORM_BATTERY_H
#define PICOCALC_CLOCK_PLATFORM_BATTERY_H

#include <cstdint>

#include "hardware/i2c.h"

struct BatteryStatus {
    bool ok;
    bool charging;
    uint8_t percent;
    uint8_t raw;
};

BatteryStatus read_battery_status(i2c_inst_t* i2c);

#endif
