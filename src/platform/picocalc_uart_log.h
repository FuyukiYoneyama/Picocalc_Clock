/*
 * Picocalc_ment - standalone musical instrument firmware for PicoCalc.
 * Copyright (c) 2026 Fuyuki Yoneyama
 * SPDX-License-Identifier: MIT
 */

#pragma once

namespace picoment {

void log_init();
void log_printf(const char* category, const char* fmt, ...);

}  // namespace picoment
