/*
 * Picocalc_ment - standalone musical instrument firmware for PicoCalc.
 * Copyright (c) 2026 Fuyuki Yoneyama
 * SPDX-License-Identifier: MIT
 */

#include "platform/picocalc_uart_log.h"

#include <cstdarg>
#include <cstdio>

#include "pico/stdlib.h"

namespace picoment {

void log_init() {
    stdio_init_all();
}

void log_printf(const char* category, const char* fmt, ...) {
    std::printf("[%s] ", category);

    va_list args;
    va_start(args, fmt);
    std::vprintf(fmt, args);
    va_end(args);

    std::printf("\n");
}

}  // namespace picoment
