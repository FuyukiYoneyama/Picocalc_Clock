#ifndef PICOCALC_CLOCK_PLATFORM_ENV_SENSOR_PROBE_H
#define PICOCALC_CLOCK_PLATFORM_ENV_SENSOR_PROBE_H

#include <cstdint>

#include "hardware/i2c.h"

struct EnvSensorData {
    bool aht20_ok;
    bool bmp280_ok;
    int32_t aht20_temperature_c_x100;
    uint32_t aht20_humidity_pct_x100;
    int32_t bmp280_temperature_c_x100;
    uint32_t bmp280_pressure_pa_x100;
    int32_t temperature_offset_c_x100;
};

void run_env_sensor_shared_i2c_probe(i2c_inst_t* i2c,
                                     unsigned sda_pin,
                                     unsigned scl_pin,
                                     uint32_t speed_hz);

EnvSensorData read_env_sensor_data(i2c_inst_t* i2c);
void print_env_sensor_report(i2c_inst_t* i2c, int8_t temperature_offset_tenths_c);

#endif
