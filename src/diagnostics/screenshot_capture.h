/*
 * Picocalc_Clock - PicoCalc clock firmware.
 * Copyright (c) 2026 Fuyuki Yoneyama
 * SPDX-License-Identifier: MIT
 */

#pragma once

namespace picoment::diagnostics {

using ScreenshotProgressCallback = void (*)(void* context);

bool capture_screenshot(ScreenshotProgressCallback progress_callback = nullptr,
                        void* progress_context = nullptr);

}  // namespace picoment::diagnostics
