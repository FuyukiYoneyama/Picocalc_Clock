/*
 * Picocalc_ment - standalone musical instrument firmware for PicoCalc.
 * Copyright (c) 2026 Fuyuki Yoneyama
 * SPDX-License-Identifier: MIT
 */

#include "platform/picocalc_keyboard.h"

#include "config/board_config.h"
#include "config/log_config.h"
#include "hardware/gpio.h"
#include "hardware/i2c.h"

namespace picoment::keyboard {
namespace {

constexpr uint8_t kRegKey = 0x04;
constexpr uint8_t kRegFifo = 0x09;

uint32_t g_read_count = 0;
uint32_t g_error_count = 0;
uint32_t g_empty_count = 0;

bool read_reg(uint8_t reg, uint8_t* data, size_t len) {
    const int written = i2c_write_blocking(i2c1, board::kKeyboardI2cAddress, &reg, 1, true);
    if (written != 1) {
        ++g_error_count;
        return false;
    }

    const int read = i2c_read_blocking(i2c1, board::kKeyboardI2cAddress, data, len, false);
    if (read != static_cast<int>(len)) {
        ++g_error_count;
        return false;
    }

    return true;
}

}  // namespace

void init() {
    i2c_init(i2c1, board::kKeyboardI2cHz);
    gpio_set_function(board::kKeyboardI2cSda, GPIO_FUNC_I2C);
    gpio_set_function(board::kKeyboardI2cScl, GPIO_FUNC_I2C);
    gpio_pull_up(board::kKeyboardI2cSda);
    gpio_pull_up(board::kKeyboardI2cScl);

    PM_LOG_BOOT("kbd=init i2c1 addr=0x%02x hz=%lu",
                board::kKeyboardI2cAddress,
                static_cast<unsigned long>(board::kKeyboardI2cHz));
}

bool read_event(KeyEvent* event) {
    if (event == nullptr) {
        return false;
    }

    uint8_t key_info[2] = {0, 0};
    if (!read_reg(kRegKey, key_info, sizeof(key_info))) {
        return false;
    }

    if ((key_info[0] & 0x1f) == 0) {
        ++g_empty_count;
        return false;
    }

    uint8_t fifo_item[2] = {0, 0};
    if (!read_reg(kRegFifo, fifo_item, sizeof(fifo_item))) {
        return false;
    }

    event->state = static_cast<KeyState>(fifo_item[0]);
    event->key = fifo_item[1];
    ++g_read_count;
    return event->key != 0;
}

uint32_t read_count() {
    return g_read_count;
}

uint32_t error_count() {
    return g_error_count;
}

uint32_t empty_count() {
    return g_empty_count;
}

}  // namespace picoment::keyboard
