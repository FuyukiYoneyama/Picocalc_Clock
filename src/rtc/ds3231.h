#ifndef PICOCALC_RTCTEST_DS3231_H
#define PICOCALC_RTCTEST_DS3231_H

#include <stdbool.h>
#include <stdint.h>

#include "hardware/i2c.h"

#ifdef __cplusplus
extern "C" {
#endif

#define DS3231_I2C_ADDR 0x68
#define DS3231_STATUS_OSF 0x80

typedef struct {
    uint16_t year;
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
    uint8_t day_of_week;
} ds3231_datetime_t;

bool ds3231_read_time(i2c_inst_t *i2c, ds3231_datetime_t *dt);
bool ds3231_write_time(i2c_inst_t *i2c, const ds3231_datetime_t *dt);
bool ds3231_read_status(i2c_inst_t *i2c, uint8_t *status);
bool ds3231_clear_osf(i2c_inst_t *i2c);
uint8_t ds3231_calculate_day_of_week(uint16_t year, uint8_t month,
                                     uint8_t day);

#ifdef __cplusplus
}
#endif

#endif
