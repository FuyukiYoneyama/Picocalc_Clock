#include "ds3231.h"

#include <stddef.h>

#define DS3231_REG_SECONDS 0x00
#define DS3231_REG_STATUS 0x0F
#define DS3231_TIMEOUT_US 10000

static uint8_t bcd_to_u8(uint8_t value) {
    return (uint8_t)(((value >> 4) * 10u) + (value & 0x0Fu));
}

static uint8_t u8_to_bcd(uint8_t value) {
    return (uint8_t)(((value / 10u) << 4) | (value % 10u));
}

static bool bcd_is_valid(uint8_t value, uint8_t max) {
    if ((value & 0x0Fu) > 9u) {
        return false;
    }
    if (((value >> 4) & 0x0Fu) > 9u) {
        return false;
    }
    return bcd_to_u8(value) <= max;
}

static bool is_leap_year(uint16_t year) {
    if ((year % 400u) == 0u) {
        return true;
    }
    if ((year % 100u) == 0u) {
        return false;
    }
    return (year % 4u) == 0u;
}

static uint8_t days_in_month(uint16_t year, uint8_t month) {
    static const uint8_t days[] = {
        31u, 28u, 31u, 30u, 31u, 30u,
        31u, 31u, 30u, 31u, 30u, 31u,
    };

    if (month < 1u || month > 12u) {
        return 0u;
    }

    if (month == 2u && is_leap_year(year)) {
        return 29u;
    }

    return days[month - 1u];
}

static bool datetime_is_valid(const ds3231_datetime_t *dt) {
    if (dt == NULL) {
        return false;
    }
    if (dt->year < 2000u || dt->year > 2099u) {
        return false;
    }
    if (dt->month < 1u || dt->month > 12u) {
        return false;
    }
    if (dt->day < 1u || dt->day > days_in_month(dt->year, dt->month)) {
        return false;
    }
    if (dt->hour > 23u || dt->minute > 59u || dt->second > 59u) {
        return false;
    }
    if (dt->day_of_week < 1u || dt->day_of_week > 7u) {
        return false;
    }
    return true;
}

uint8_t ds3231_calculate_day_of_week(uint16_t year, uint8_t month,
                                     uint8_t day) {
    if (year < 2000u || year > 2099u || month < 1u || month > 12u ||
        day < 1u || day > days_in_month(year, month)) {
        return 0u;
    }

    if (month < 3u) {
        month += 12u;
        year--;
    }

    uint16_t k = (uint16_t)(year % 100u);
    uint16_t j = (uint16_t)(year / 100u);
    uint16_t h = (uint16_t)((day + ((13u * (month + 1u)) / 5u) + k +
                             (k / 4u) + (j / 4u) + (5u * j)) %
                            7u);

    // Zeller's congruence gives 0=Sat, 1=Sun, 2=Mon ... 6=Fri.
    static const uint8_t zeller_to_ds3231[] = {
        6u, 7u, 1u, 2u, 3u, 4u, 5u,
    };
    return zeller_to_ds3231[h];
}

bool ds3231_read_time(i2c_inst_t *i2c, ds3231_datetime_t *dt) {
    if (i2c == NULL || dt == NULL) {
        return false;
    }

    uint8_t reg = DS3231_REG_SECONDS;
    uint8_t raw[7] = {0};

    int written = i2c_write_timeout_us(i2c, DS3231_I2C_ADDR, &reg, 1, true,
                                       DS3231_TIMEOUT_US);
    if (written != 1) {
        return false;
    }

    int read = i2c_read_timeout_us(i2c, DS3231_I2C_ADDR, raw, sizeof(raw),
                                   false, DS3231_TIMEOUT_US);
    if (read != (int)sizeof(raw)) {
        return false;
    }

    uint8_t seconds = raw[0] & 0x7Fu;
    uint8_t minutes = raw[1] & 0x7Fu;
    uint8_t hours = raw[2];
    uint8_t day_of_week = raw[3] & 0x07u;
    uint8_t day = raw[4] & 0x3Fu;
    uint8_t month = raw[5] & 0x1Fu;
    uint8_t year = raw[6];

    if ((hours & 0x40u) != 0u) {
        return false;
    }

    if (!bcd_is_valid(seconds, 59) || !bcd_is_valid(minutes, 59) ||
        !bcd_is_valid(hours & 0x3Fu, 23) || !bcd_is_valid(day, 31) ||
        !bcd_is_valid(month, 12) || !bcd_is_valid(year, 99)) {
        return false;
    }

    uint8_t decoded_day = bcd_to_u8(day);
    uint8_t decoded_month = bcd_to_u8(month);
    uint16_t decoded_year = (uint16_t)(2000u + bcd_to_u8(year));
    if (day_of_week < 1u || day_of_week > 7u ||
        decoded_day < 1u || decoded_month < 1u ||
        decoded_day > days_in_month(decoded_year, decoded_month)) {
        return false;
    }

    dt->second = bcd_to_u8(seconds);
    dt->minute = bcd_to_u8(minutes);
    dt->hour = bcd_to_u8(hours & 0x3Fu);
    dt->day_of_week = day_of_week;
    dt->day = decoded_day;
    dt->month = decoded_month;
    dt->year = decoded_year;
    return true;
}

bool ds3231_write_time(i2c_inst_t *i2c, const ds3231_datetime_t *dt) {
    if (i2c == NULL || !datetime_is_valid(dt)) {
        return false;
    }

    uint8_t data[8] = {
        DS3231_REG_SECONDS,
        u8_to_bcd(dt->second),
        u8_to_bcd(dt->minute),
        u8_to_bcd(dt->hour),
        u8_to_bcd(dt->day_of_week),
        u8_to_bcd(dt->day),
        u8_to_bcd(dt->month),
        u8_to_bcd((uint8_t)(dt->year - 2000u)),
    };

    int written = i2c_write_timeout_us(i2c, DS3231_I2C_ADDR, data,
                                       sizeof(data), false,
                                       DS3231_TIMEOUT_US);
    return written == (int)sizeof(data);
}

bool ds3231_read_status(i2c_inst_t *i2c, uint8_t *status) {
    if (i2c == NULL || status == NULL) {
        return false;
    }

    uint8_t reg = DS3231_REG_STATUS;
    int written = i2c_write_timeout_us(i2c, DS3231_I2C_ADDR, &reg, 1, true,
                                       DS3231_TIMEOUT_US);
    if (written != 1) {
        return false;
    }

    int read = i2c_read_timeout_us(i2c, DS3231_I2C_ADDR, status, 1, false,
                                   DS3231_TIMEOUT_US);
    return read == 1;
}

bool ds3231_clear_osf(i2c_inst_t *i2c) {
    if (i2c == NULL) {
        return false;
    }

    uint8_t status = 0;
    if (!ds3231_read_status(i2c, &status)) {
        return false;
    }

    status &= (uint8_t)~DS3231_STATUS_OSF;
    uint8_t data[2] = {DS3231_REG_STATUS, status};
    int written = i2c_write_timeout_us(i2c, DS3231_I2C_ADDR, data,
                                       sizeof(data), false,
                                       DS3231_TIMEOUT_US);
    return written == (int)sizeof(data);
}
