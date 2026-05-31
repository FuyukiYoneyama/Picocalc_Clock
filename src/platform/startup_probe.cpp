#include "platform/startup_probe.h"

#include <cstdio>

#include "ds3231.h"
#include "hardware/gpio.h"

namespace {

bool probe_keyboard_controller(const StartupProbeConfig& config) {
    uint8_t reg = 0x01;
    uint8_t buf[2] = {0};
    int written = i2c_write_timeout_us(config.i2c,
                                       config.keyboard_address,
                                       &reg,
                                       1,
                                       false,
                                       config.timeout_us);
    if (written != 1) {
        return false;
    }
    int read = i2c_read_timeout_us(config.i2c,
                                   config.keyboard_address,
                                   buf,
                                   2,
                                   false,
                                   config.timeout_us);
    return read == 2;
}

bool probe_eeprom_24c32(const StartupProbeConfig& config) {
    uint8_t ptr[2] = {0x00, 0x00};
    int written = i2c_write_timeout_us(config.i2c,
                                       config.eeprom_address,
                                       ptr,
                                       2,
                                       false,
                                       config.timeout_us);
    return written == 2;
}

}  // namespace

void i2c_bus_init(const StartupProbeConfig& config) {
    i2c_init(config.i2c, config.speed_hz);
    gpio_set_function(config.sda_pin, GPIO_FUNC_I2C);
    gpio_set_function(config.scl_pin, GPIO_FUNC_I2C);
    gpio_pull_up(config.sda_pin);
    gpio_pull_up(config.scl_pin);
}

ProbeResult run_startup_probes(const StartupProbeConfig& config) {
    ProbeResult result = {};
    result.rtc_ok = ds3231_read_status(config.i2c, &result.rtc_status);
    result.eeprom_ok = probe_eeprom_24c32(config);
    result.keyboard_ok = probe_keyboard_controller(config);
    std::printf("STARTUP PROBE rtc=%s eeprom=%s keyboard=%s rtc_status=0x%02X\r\n",
                result.rtc_ok ? "PASS" : "FAIL",
                result.eeprom_ok ? "PASS" : "FAIL",
                result.keyboard_ok ? "PASS" : "FAIL",
                result.rtc_status);
    return result;
}
