/*
 * Picocalc_ment - standalone musical instrument firmware for PicoCalc.
 * Copyright (c) 2026 Fuyuki Yoneyama
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "config/build_config.h"
#include "font/spleen_native_fonts.h"

#include <stddef.h>
#include <stdint.h>

namespace picoment::display {

constexpr int kScreenWidth = 320;
constexpr int kScreenHeight = 320;
constexpr int kAccentY = 0;
constexpr int kHeaderY = 4;
constexpr int kContentY = 38;
constexpr int kFooterY = 292;

void init();
void clear(uint16_t rgb565);
void fill_rect(int x, int y, int w, int h, uint16_t rgb565);
void draw_frame(int x, int y, int w, int h, uint16_t color);
void draw_line(int x0, int y0, int x1, int y1, uint16_t color);
void draw_circle(int cx, int cy, int radius, uint16_t color);
void fill_circle(int cx, int cy, int radius, uint16_t color);
void draw_text_band(int x, int y, int w, int h, const char* text, uint16_t fg, uint16_t bg);
void draw_text_large_band(int x, int y, int w, int h, const char* text, uint16_t fg, uint16_t bg);
void draw_spleen_native_text_band(int x, int y, int w, int h, const char* text, font::SpleenNativeSize size, uint16_t fg, uint16_t bg);
void draw_spleen_native_text_3x2_band(int x, int y, int w, int h, const char* text, font::SpleenNativeSize size, uint16_t fg, uint16_t bg);
// Transparent variants: draw only foreground pixels, leaving background pixels untouched.
void draw_spleen_native_text_fg_only(int x, int y, int w, int h, const char* text, font::SpleenNativeSize size, uint16_t fg);
void draw_spleen_native_text_3x2_fg_only(int x, int y, int w, int h, const char* text, font::SpleenNativeSize size, uint16_t fg);
void draw_boot_screen(const char* version);
void draw_font_sample_screen(const char* version);
void draw_status_line(int line, const char* text, uint16_t color);
#if PICOMENT_SCREENSHOT_CAPTURE_BUILD
bool readback_probe_raw(int x, int y, uint8_t* raw, size_t raw_len);
bool readback_rect_rgb565(int x, int y, int w, int h, uint16_t* dst);
#endif

}  // namespace picoment::display
