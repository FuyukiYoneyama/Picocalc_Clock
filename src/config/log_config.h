/*
 * Picocalc_ment - standalone musical instrument firmware for PicoCalc.
 * Copyright (c) 2026 Fuyuki Yoneyama
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "config/build_config.h"
#include "platform/picocalc_uart_log.h"

#if PICOMENT_LOG_ENABLED
#define PM_LOG_BOOT(fmt, ...) ::picoment::log_printf("BOOT", fmt, ##__VA_ARGS__)
#define PM_LOG_MAIN(fmt, ...) ::picoment::log_printf("MAIN", fmt, ##__VA_ARGS__)
#else
#define PM_LOG_BOOT(fmt, ...) ((void)0)
#define PM_LOG_MAIN(fmt, ...) ((void)0)
#endif

#if PICOMENT_METRICS_ENABLED
#define PM_METRIC_INC(counter) (++(counter))
#else
#define PM_METRIC_INC(counter) ((void)0)
#endif
