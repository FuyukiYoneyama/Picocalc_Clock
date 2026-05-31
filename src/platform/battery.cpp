#include "platform/battery.h"

#include "config/board_config.h"

namespace {

constexpr uint32_t kI2cScanTimeoutUs = 10000;
constexpr uint8_t kBatteryRegister = 0x0b;

}  // namespace

BatteryStatus read_battery_status(i2c_inst_t* i2c) {
    BatteryStatus status = {};
    uint8_t reg = kBatteryRegister;
    uint8_t raw[2] = {0};
    int written = i2c_write_timeout_us(i2c, picoment::board::kKeyboardI2cAddress,
                                       &reg, 1, true, kI2cScanTimeoutUs);
    if (written != 1) {
        return status;
    }
    int read = i2c_read_timeout_us(i2c, picoment::board::kKeyboardI2cAddress,
                                   raw, 2, false, kI2cScanTimeoutUs);
    if (read != 2) {
        return status;
    }

    status.ok = true;
    status.raw = raw[1];
    status.charging = (raw[1] & 0x80u) != 0;
    status.percent = raw[1] & 0x7fu;
    if (status.percent > 100) {
        status.percent = 100;
    }
    return status;
}
