/*
 * Picocalc_Clock - PicoCalc clock firmware.
 * Copyright (c) 2026 Fuyuki Yoneyama
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

namespace picoment::sdcard {

bool is_present();
bool init();
bool is_initialized();
bool read_sectors(uint32_t lba, uint8_t* buffer, uint32_t count);
bool write_sectors(uint32_t lba, const uint8_t* buffer, uint32_t count);
bool get_sector_count(uint32_t* sector_count);
void reset();

}  // namespace picoment::sdcard
