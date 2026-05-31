#ifndef PICOCALC_CLOCK_PLATFORM_STARTUP_PROBE_H
#define PICOCALC_CLOCK_PLATFORM_STARTUP_PROBE_H

#include <cstdint>

#include "hardware/i2c.h"

struct StartupProbeConfig {
    i2c_inst_t* i2c;
    uint sda_pin;
    uint scl_pin;
    uint32_t speed_hz;
    uint8_t keyboard_address;
    uint8_t eeprom_address;
    uint32_t timeout_us;
};

struct ProbeResult {
    bool rtc_ok;
    bool eeprom_ok;
    bool keyboard_ok;
    uint8_t rtc_status;
};

void i2c_bus_init(const StartupProbeConfig& config);
ProbeResult run_startup_probes(const StartupProbeConfig& config);

#endif
