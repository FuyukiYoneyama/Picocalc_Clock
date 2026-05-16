/*
 * Picocalc_ment - standalone musical instrument firmware for PicoCalc.
 * Copyright (c) 2026 Fuyuki Yoneyama
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <stdint.h>

namespace picoment::board {

constexpr uint32_t kTargetSampleRate = 48000;
constexpr uint32_t kSystemClockKhz = 250000;
constexpr uint32_t kUartBaudRate = 115200;

constexpr unsigned kLcdPinSck = 10;
constexpr unsigned kLcdPinMosi = 11;
constexpr unsigned kLcdPinMiso = 12;
constexpr unsigned kLcdPinCs = 13;
constexpr unsigned kLcdPinDc = 14;
constexpr unsigned kLcdPinRst = 15;
constexpr unsigned kLcdPinRamCs = 21;

constexpr unsigned kKeyboardI2cSda = 6;
constexpr unsigned kKeyboardI2cScl = 7;
constexpr uint32_t kKeyboardI2cHz = 400 * 1000;
constexpr uint8_t kKeyboardI2cAddress = 0x1f;

constexpr unsigned kAudioPwmLeft = 26;
constexpr unsigned kAudioPwmRight = 27;
constexpr uint16_t kAudioPwmWrap = 255;

}  // namespace picoment::board
