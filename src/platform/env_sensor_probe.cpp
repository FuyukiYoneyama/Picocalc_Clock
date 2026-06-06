#include "platform/env_sensor_probe.h"

#include <cstdio>
#include <cstdint>

#include "hardware/gpio.h"
#include "hardware/structs/i2c.h"
#include "pico/time.h"

namespace {

constexpr uint32_t kEnvTimeoutUs = 20000;
constexpr uint8_t kAht20Addr = 0x38;
constexpr uint8_t kBmp280Addr = 0x77;
constexpr uint8_t kBmp280ChipIdReg = 0xd0;

void clear_i2c_latches(i2c_inst_t* i2c);

struct Bmp280Calib {
    uint16_t dig_t1;
    int16_t dig_t2;
    int16_t dig_t3;
    uint16_t dig_p1;
    int16_t dig_p2;
    int16_t dig_p3;
    int16_t dig_p4;
    int16_t dig_p5;
    int16_t dig_p6;
    int16_t dig_p7;
    int16_t dig_p8;
    int16_t dig_p9;
};

uint16_t le_u16(const uint8_t* data) {
    return static_cast<uint16_t>(data[0]) |
           (static_cast<uint16_t>(data[1]) << 8);
}

int16_t le_s16(const uint8_t* data) {
    return static_cast<int16_t>(le_u16(data));
}

void print_fixed_2(const char* label, int32_t value_x100) {
    const bool negative = value_x100 < 0;
    uint32_t abs_value = negative ? static_cast<uint32_t>(-value_x100)
                                  : static_cast<uint32_t>(value_x100);
    std::printf("%s%s%lu.%02lu",
                label,
                negative ? "-" : "",
                static_cast<unsigned long>(abs_value / 100u),
                static_cast<unsigned long>(abs_value % 100u));
}

int32_t average_temperature_c_x100(const EnvSensorData& data) {
    return (data.aht20_temperature_c_x100 + data.bmp280_temperature_c_x100) / 2;
}

bool i2c_write_bytes(i2c_inst_t* i2c, uint8_t addr, const uint8_t* data, size_t len) {
    const int rc = i2c_write_timeout_us(i2c,
                                        addr,
                                        data,
                                        len,
                                        false,
                                        kEnvTimeoutUs);
    clear_i2c_latches(i2c);
    return rc == static_cast<int>(len);
}

bool i2c_read_bytes(i2c_inst_t* i2c, uint8_t addr, uint8_t* data, size_t len) {
    const int rc = i2c_read_timeout_us(i2c,
                                       addr,
                                       data,
                                       len,
                                       false,
                                       kEnvTimeoutUs);
    clear_i2c_latches(i2c);
    return rc == static_cast<int>(len);
}

bool i2c_read_registers(i2c_inst_t* i2c,
                        uint8_t addr,
                        uint8_t reg,
                        uint8_t* data,
                        size_t len) {
    const int written = i2c_write_timeout_us(i2c,
                                             addr,
                                             &reg,
                                             1,
                                             true,
                                             kEnvTimeoutUs);
    if (written != 1) {
        clear_i2c_latches(i2c);
        return false;
    }
    const int read = i2c_read_timeout_us(i2c,
                                         addr,
                                         data,
                                         len,
                                         false,
                                         kEnvTimeoutUs);
    clear_i2c_latches(i2c);
    return read == static_cast<int>(len);
}

bool i2c_write_register(i2c_inst_t* i2c, uint8_t addr, uint8_t reg, uint8_t value) {
    const uint8_t data[2] = {reg, value};
    return i2c_write_bytes(i2c, addr, data, sizeof(data));
}

bool read_aht20_measurement(i2c_inst_t* i2c,
                            int32_t* temperature_c_x100,
                            uint32_t* humidity_pct_x100,
                            uint8_t* status,
                            uint8_t* crc,
                            uint32_t* raw_temperature,
                            uint32_t* raw_humidity) {
    uint8_t local_status = 0;
    if (!i2c_read_bytes(i2c, kAht20Addr, &local_status, 1)) {
        return false;
    }
    if ((local_status & 0x08u) == 0) {
        const uint8_t init_cmd[3] = {0xbe, 0x08, 0x00};
        if (!i2c_write_bytes(i2c, kAht20Addr, init_cmd, sizeof(init_cmd))) {
            return false;
        }
        sleep_ms(20);
    }

    const uint8_t measure_cmd[3] = {0xac, 0x33, 0x00};
    if (!i2c_write_bytes(i2c, kAht20Addr, measure_cmd, sizeof(measure_cmd))) {
        return false;
    }
    sleep_ms(90);

    uint8_t data[7] = {};
    if (!i2c_read_bytes(i2c, kAht20Addr, data, sizeof(data))) {
        return false;
    }
    if ((data[0] & 0x80u) != 0) {
        return false;
    }

    const uint32_t humidity_raw =
        (static_cast<uint32_t>(data[1]) << 12) |
        (static_cast<uint32_t>(data[2]) << 4) |
        (static_cast<uint32_t>(data[3]) >> 4);
    const uint32_t temperature_raw =
        ((static_cast<uint32_t>(data[3]) & 0x0fu) << 16) |
        (static_cast<uint32_t>(data[4]) << 8) |
        static_cast<uint32_t>(data[5]);

    *humidity_pct_x100 = static_cast<uint32_t>(
        (static_cast<uint64_t>(humidity_raw) * 10000u) >> 20);
    *temperature_c_x100 = static_cast<int32_t>(
        ((static_cast<uint64_t>(temperature_raw) * 20000u) >> 20) - 5000);
    *status = data[0];
    *crc = data[6];
    *raw_temperature = temperature_raw;
    *raw_humidity = humidity_raw;
    return true;
}

bool read_bmp280_calibration(i2c_inst_t* i2c, Bmp280Calib* calib) {
    uint8_t data[24] = {};
    if (!i2c_read_registers(i2c, kBmp280Addr, 0x88, data, sizeof(data))) {
        return false;
    }
    calib->dig_t1 = le_u16(&data[0]);
    calib->dig_t2 = le_s16(&data[2]);
    calib->dig_t3 = le_s16(&data[4]);
    calib->dig_p1 = le_u16(&data[6]);
    calib->dig_p2 = le_s16(&data[8]);
    calib->dig_p3 = le_s16(&data[10]);
    calib->dig_p4 = le_s16(&data[12]);
    calib->dig_p5 = le_s16(&data[14]);
    calib->dig_p6 = le_s16(&data[16]);
    calib->dig_p7 = le_s16(&data[18]);
    calib->dig_p8 = le_s16(&data[20]);
    calib->dig_p9 = le_s16(&data[22]);
    return calib->dig_p1 != 0;
}

int32_t compensate_bmp280_temperature_x100(const Bmp280Calib& calib,
                                           int32_t adc_t,
                                           int32_t* t_fine) {
    const int32_t var1 = (((adc_t >> 3) -
                          (static_cast<int32_t>(calib.dig_t1) << 1)) *
                         static_cast<int32_t>(calib.dig_t2)) >>
                         11;
    const int32_t shifted = (adc_t >> 4) - static_cast<int32_t>(calib.dig_t1);
    const int32_t var2 = (((shifted * shifted) >> 12) *
                          static_cast<int32_t>(calib.dig_t3)) >>
                         14;
    *t_fine = var1 + var2;
    return (*t_fine * 5 + 128) >> 8;
}

bool compensate_bmp280_pressure_pa_x100(const Bmp280Calib& calib,
                                        int32_t adc_p,
                                        int32_t t_fine,
                                        uint32_t* pressure_pa_x100) {
    int64_t var1 = static_cast<int64_t>(t_fine) - 128000;
    int64_t var2 = var1 * var1 * static_cast<int64_t>(calib.dig_p6);
    var2 = var2 + ((var1 * static_cast<int64_t>(calib.dig_p5)) << 17);
    var2 = var2 + (static_cast<int64_t>(calib.dig_p4) << 35);
    var1 = ((var1 * var1 * static_cast<int64_t>(calib.dig_p3)) >> 8) +
           ((var1 * static_cast<int64_t>(calib.dig_p2)) << 12);
    var1 = (((static_cast<int64_t>(1) << 47) + var1) *
            static_cast<int64_t>(calib.dig_p1)) >>
           33;
    if (var1 == 0) {
        return false;
    }
    int64_t pressure = 1048576 - adc_p;
    pressure = (((pressure << 31) - var2) * 3125) / var1;
    var1 = (static_cast<int64_t>(calib.dig_p9) *
            (pressure >> 13) *
            (pressure >> 13)) >>
           25;
    var2 = (static_cast<int64_t>(calib.dig_p8) * pressure) >> 19;
    pressure = ((pressure + var1 + var2) >> 8) +
               (static_cast<int64_t>(calib.dig_p7) << 4);
    *pressure_pa_x100 = static_cast<uint32_t>((pressure * 100) / 256);
    return true;
}

bool read_bmp280_measurement(i2c_inst_t* i2c,
                             int32_t* temperature_c_x100,
                             uint32_t* pressure_pa_x100,
                             uint8_t* chip_id,
                             int32_t* raw_temperature,
                             int32_t* raw_pressure) {
    uint8_t local_chip_id = 0;
    if (!i2c_read_registers(i2c, kBmp280Addr, kBmp280ChipIdReg, &local_chip_id, 1)) {
        return false;
    }
    if (local_chip_id != 0x58) {
        *chip_id = local_chip_id;
        return false;
    }

    Bmp280Calib calib = {};
    if (!read_bmp280_calibration(i2c, &calib)) {
        *chip_id = local_chip_id;
        return false;
    }

    if (!i2c_write_register(i2c, kBmp280Addr, 0xf5, 0x00) ||
        !i2c_write_register(i2c, kBmp280Addr, 0xf4, 0x25)) {
        *chip_id = local_chip_id;
        return false;
    }
    sleep_ms(40);

    uint8_t data[6] = {};
    if (!i2c_read_registers(i2c, kBmp280Addr, 0xf7, data, sizeof(data))) {
        *chip_id = local_chip_id;
        return false;
    }

    const int32_t adc_p =
        (static_cast<int32_t>(data[0]) << 12) |
        (static_cast<int32_t>(data[1]) << 4) |
        (static_cast<int32_t>(data[2]) >> 4);
    const int32_t adc_t =
        (static_cast<int32_t>(data[3]) << 12) |
        (static_cast<int32_t>(data[4]) << 4) |
        (static_cast<int32_t>(data[5]) >> 4);
    if (adc_p == 0x80000 || adc_t == 0x80000) {
        *chip_id = local_chip_id;
        return false;
    }

    int32_t t_fine = 0;
    *temperature_c_x100 =
        compensate_bmp280_temperature_x100(calib, adc_t, &t_fine);
    if (!compensate_bmp280_pressure_pa_x100(calib,
                                            adc_p,
                                            t_fine,
                                            pressure_pa_x100)) {
        *chip_id = local_chip_id;
        return false;
    }
    *chip_id = local_chip_id;
    *raw_temperature = adc_t;
    *raw_pressure = adc_p;
    return true;
}

void print_line_levels(i2c_inst_t* i2c,
                       unsigned sda_pin,
                       unsigned scl_pin,
                       const char* label,
                       uint8_t address,
                       const char* phase) {
    std::printf("DETAIL I2C_LEVEL test=%s addr=0x%02X phase=%s sda=%u scl=%u abort=0x%08lX raw_intr=0x%08lX status=0x%08lX\r\n",
                label,
                address,
                phase,
                gpio_get(sda_pin) ? 1u : 0u,
                gpio_get(scl_pin) ? 1u : 0u,
                static_cast<unsigned long>(i2c->hw->tx_abrt_source),
                static_cast<unsigned long>(i2c->hw->raw_intr_stat),
                static_cast<unsigned long>(i2c->hw->status));
}

void clear_i2c_latches(i2c_inst_t* i2c) {
    (void)i2c->hw->clr_tx_abrt;
    (void)i2c->hw->clr_stop_det;
    (void)i2c->hw->clr_rx_under;
    (void)i2c->hw->clr_rx_over;
    (void)i2c->hw->clr_tx_over;
}

int read_aht20_status(i2c_inst_t* i2c,
                      unsigned sda_pin,
                      unsigned scl_pin,
                      uint8_t* value) {
    uint8_t local = 0;
    std::printf("TEST PLAN SHARED_AHT20_READ addr=0x%02X operation=read_1_byte bus=same_as_rtc expect=rc_1\r\n",
                kAht20Addr);
    print_line_levels(i2c, sda_pin, scl_pin, "shared_aht20_read", kAht20Addr, "before");
    const int result = i2c_read_timeout_us(i2c,
                                           kAht20Addr,
                                           &local,
                                           1,
                                           false,
                                           kEnvTimeoutUs);
    const uint32_t abort = i2c->hw->tx_abrt_source;
    print_line_levels(i2c, sda_pin, scl_pin, "shared_aht20_read", kAht20Addr, "after");
    std::printf("RESULT %s SHARED_AHT20_READ addr=0x%02X rc=%d value=0x%02X abort_after=0x%08lX\r\n",
                result == 1 ? "PASS" : "FAIL",
                kAht20Addr,
                result,
                local,
                static_cast<unsigned long>(abort));
    if (value != nullptr) {
        *value = local;
    }
    clear_i2c_latches(i2c);
    return result;
}

int read_bmp280_id(i2c_inst_t* i2c,
                   unsigned sda_pin,
                   unsigned scl_pin,
                   uint8_t* value) {
    constexpr uint8_t kChipIdReg = 0xd0;
    uint8_t local = 0;
    std::printf("TEST PLAN SHARED_BMP280_ID addr=0x%02X operation=write_reg_0x%02X_then_read_1_byte bus=same_as_rtc expect=chip_id_0x58\r\n",
                kBmp280Addr,
                kChipIdReg);
    print_line_levels(i2c, sda_pin, scl_pin, "shared_bmp280_write_reg", kBmp280Addr, "before");
    const int written = i2c_write_timeout_us(i2c,
                                             kBmp280Addr,
                                             &kChipIdReg,
                                             1,
                                             true,
                                             kEnvTimeoutUs);
    const uint32_t write_abort = i2c->hw->tx_abrt_source;
    print_line_levels(i2c, sda_pin, scl_pin, "shared_bmp280_write_reg", kBmp280Addr, "after");

    int read = -999;
    uint32_t read_abort = 0;
    if (written == 1) {
        read = i2c_read_timeout_us(i2c,
                                   kBmp280Addr,
                                   &local,
                                   1,
                                   false,
                                   kEnvTimeoutUs);
        read_abort = i2c->hw->tx_abrt_source;
        print_line_levels(i2c, sda_pin, scl_pin, "shared_bmp280_read_reg", kBmp280Addr, "after");
    }

    std::printf("RESULT %s SHARED_BMP280_ID addr=0x%02X reg=0x%02X write_rc=%d read_rc=%d value=0x%02X expected=0x58 write_abort=0x%08lX read_abort=0x%08lX\r\n",
                (written == 1 && read == 1 && local == 0x58) ? "PASS" : "FAIL",
                kBmp280Addr,
                kChipIdReg,
                written,
                read,
                local,
                static_cast<unsigned long>(write_abort),
                static_cast<unsigned long>(read_abort));
    if (value != nullptr) {
        *value = local;
    }
    clear_i2c_latches(i2c);
    return written == 1 ? read : written;
}

}  // namespace

void run_env_sensor_shared_i2c_probe(i2c_inst_t* i2c,
                                     unsigned sda_pin,
                                     unsigned scl_pin,
                                     uint32_t speed_hz) {
    std::printf("TEST PLAN SHARED_I2C_SENSOR_TEST bus=same_as_rtc sda=GP%u scl=GP%u hz=%lu expected_addrs=0x38,0x77 actions=aht20_read,bmp280_id_read expect=both_pass\r\n",
                sda_pin,
                scl_pin,
                static_cast<unsigned long>(speed_hz));

    const bool idle_ok = gpio_get(sda_pin) && gpio_get(scl_pin);
    std::printf("RESULT %s SHARED_I2C_IDLE sda=%u scl=%u expect=both_1\r\n",
                idle_ok ? "PASS" : "FAIL",
                gpio_get(sda_pin) ? 1u : 0u,
                gpio_get(scl_pin) ? 1u : 0u);

    uint8_t aht_status = 0;
    const int aht_rc = read_aht20_status(i2c, sda_pin, scl_pin, &aht_status);
    uint8_t bmp_id = 0;
    const int bmp_rc = read_bmp280_id(i2c, sda_pin, scl_pin, &bmp_id);

    const bool all_ok = idle_ok && aht_rc == 1 && bmp_rc == 1 && bmp_id == 0x58;
    std::printf("RESULT %s SHARED_I2C_SENSOR_TEST aht20_rc=%d aht20_status=0x%02X bmp280_rc=%d bmp280_id=0x%02X expected_bmp280=0x58\r\n",
                all_ok ? "PASS" : "FAIL",
                aht_rc,
                aht_status,
                bmp_rc,
                bmp_id);
}

EnvSensorData read_env_sensor_data(i2c_inst_t* i2c) {
    EnvSensorData data = {};
    uint8_t aht_status = 0;
    uint8_t aht_crc = 0;
    uint32_t aht_raw_temperature = 0;
    uint32_t aht_raw_humidity = 0;
    data.aht20_ok = read_aht20_measurement(i2c,
                                           &data.aht20_temperature_c_x100,
                                           &data.aht20_humidity_pct_x100,
                                           &aht_status,
                                           &aht_crc,
                                           &aht_raw_temperature,
                                           &aht_raw_humidity);
    uint8_t bmp_chip_id = 0;
    int32_t bmp_raw_temperature = 0;
    int32_t bmp_raw_pressure = 0;
    data.bmp280_ok = read_bmp280_measurement(i2c,
                                             &data.bmp280_temperature_c_x100,
                                             &data.bmp280_pressure_pa_x100,
                                             &bmp_chip_id,
                                             &bmp_raw_temperature,
                                             &bmp_raw_pressure);

    (void)aht_status;
    (void)aht_crc;
    (void)aht_raw_temperature;
    (void)aht_raw_humidity;
    (void)bmp_chip_id;
    (void)bmp_raw_temperature;
    (void)bmp_raw_pressure;

    return data;
}

void print_env_sensor_report(i2c_inst_t* i2c, int8_t temperature_offset_tenths_c) {
    EnvSensorData data = read_env_sensor_data(i2c);
    data.temperature_offset_c_x100 =
        static_cast<int32_t>(temperature_offset_tenths_c) * 10;
    std::puts("Sensors:");

    if (data.aht20_ok) {
        std::printf("  AHT20   ");
        print_fixed_2("temp=", data.aht20_temperature_c_x100);
        std::printf(" C  humidity=%lu.%02lu %%\r\n",
                    static_cast<unsigned long>(data.aht20_humidity_pct_x100 / 100u),
                    static_cast<unsigned long>(data.aht20_humidity_pct_x100 % 100u));
    } else {
        std::puts("  AHT20   read failed");
    }

    if (data.bmp280_ok) {
        const uint32_t pressure_hpa_x100 = data.bmp280_pressure_pa_x100 / 100u;
        std::printf("  BMP280  ");
        print_fixed_2("temp=", data.bmp280_temperature_c_x100);
        std::printf(" C  pressure=%lu.%02lu hPa\r\n",
                    static_cast<unsigned long>(pressure_hpa_x100 / 100u),
                    static_cast<unsigned long>(pressure_hpa_x100 % 100u));
    } else {
        std::puts("  BMP280  read failed");
    }

    if (data.aht20_ok && data.bmp280_ok) {
        const int32_t raw_average = average_temperature_c_x100(data);
        std::printf("  Average ");
        print_fixed_2("raw=", raw_average);
        std::printf(" C  ");
        print_fixed_2("offset=", data.temperature_offset_c_x100);
        std::printf(" C  ");
        print_fixed_2("adjusted=", raw_average + data.temperature_offset_c_x100);
        std::puts(" C");
    } else {
        std::puts("  Average read failed");
    }
}
