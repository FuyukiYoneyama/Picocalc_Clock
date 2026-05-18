/*
 * Picocalc_ment - standalone musical instrument firmware for PicoCalc.
 * Copyright (c) 2026 Fuyuki Yoneyama
 * SPDX-License-Identifier: MIT
 */

#include "platform/picocalc_display.h"

#include <cstddef>
#include <cstdint>

#include "config/board_config.h"
#include "config/log_config.h"
#include "font/cozette_font.h"
#include "font/overlay_font.h"
#if PICOMENT_FONT_SAMPLE_BUILD
#include "font/spleen_native_fonts.h"
#endif
#include "hardware/gpio.h"
#include "hardware/pio.h"
#include "pico/stdlib.h"
#include "lcd_spi_min.pio.h"

namespace picoment::display {
namespace {

constexpr float kPioClkDiv = 4.0f;
PIO g_pio = pio0;
uint g_sm = 0;
uint g_pio_offset = 0;

void select() {
    gpio_put(board::kLcdPinCs, 0);
}

void deselect() {
    gpio_put(board::kLcdPinCs, 1);
}

void set_dc(bool data) {
    gpio_put(board::kLcdPinDc, data ? 1 : 0);
}

void write_bytes(const uint8_t* data, size_t len) {
    if (len == 0) {
        return;
    }
    while (len > 0) {
        lcd_spi_min_put(g_pio, g_sm, *data);
        ++data;
        --len;
    }
}

void wait_idle() {
    lcd_spi_min_wait_idle(g_pio, g_sm);
}

void write_command(uint8_t cmd) {
    select();
    set_dc(false);
    write_bytes(&cmd, 1);
    wait_idle();
    deselect();
}

void write_data(const uint8_t* data, size_t len) {
    if (len == 0) {
        return;
    }
    select();
    set_dc(true);
    write_bytes(data, len);
    wait_idle();
    deselect();
}

void write_command1(uint8_t cmd, uint8_t data0) {
    write_command(cmd);
    write_data(&data0, 1);
}

void write_commandn(uint8_t cmd, const uint8_t* data, size_t len) {
    write_command(cmd);
    write_data(data, len);
}

#if PICOMENT_SCREENSHOT_CAPTURE_BUILD
void read_io_delay() {
    busy_wait_us_32(1);
}

void set_bitbang_mode(bool enabled) {
    if (enabled) {
        pio_sm_set_enabled(g_pio, g_sm, false);
        gpio_set_function(board::kLcdPinSck, GPIO_FUNC_SIO);
        gpio_set_function(board::kLcdPinMosi, GPIO_FUNC_SIO);
        gpio_set_function(board::kLcdPinMiso, GPIO_FUNC_SIO);
        gpio_set_dir(board::kLcdPinSck, GPIO_OUT);
        gpio_set_dir(board::kLcdPinMosi, GPIO_OUT);
        gpio_set_dir(board::kLcdPinMiso, GPIO_IN);
        gpio_disable_pulls(board::kLcdPinMiso);
        gpio_put(board::kLcdPinSck, 0);
        gpio_put(board::kLcdPinMosi, 0);
        return;
    }

    gpio_set_function(board::kLcdPinSck, GPIO_FUNC_PIO0);
    gpio_set_function(board::kLcdPinMosi, GPIO_FUNC_PIO0);
    gpio_set_function(board::kLcdPinMiso, GPIO_FUNC_SIO);
    gpio_set_dir(board::kLcdPinMiso, GPIO_IN);
    gpio_disable_pulls(board::kLcdPinMiso);
    pio_sm_set_enabled(g_pio, g_sm, true);
}

void bitbang_write_byte(uint8_t value) {
    for (int bit = 7; bit >= 0; --bit) {
        gpio_put(board::kLcdPinSck, 0);
        gpio_put(board::kLcdPinMosi, (value >> bit) & 1u);
        read_io_delay();
        gpio_put(board::kLcdPinSck, 1);
        read_io_delay();
    }
    gpio_put(board::kLcdPinSck, 0);
}

uint8_t bitbang_read_byte_falling() {
    uint8_t value = 0;
    for (int bit = 7; bit >= 0; --bit) {
        gpio_put(board::kLcdPinSck, 0);
        read_io_delay();
        if (gpio_get(board::kLcdPinMiso)) {
            value |= static_cast<uint8_t>(1u << bit);
        }
        gpio_put(board::kLcdPinSck, 1);
        read_io_delay();
    }
    gpio_put(board::kLcdPinSck, 0);
    return value;
}

void bitbang_write_commandn_held(uint8_t cmd, const uint8_t* data, size_t len) {
    set_dc(false);
    bitbang_write_byte(cmd);
    if (data == nullptr || len == 0) {
        return;
    }
    set_dc(true);
    while (len-- > 0) {
        bitbang_write_byte(*data++);
    }
}

void set_read_window_held(int x, int y, int w, int h) {
    const int x0 = x < 0 ? 0 : x;
    const int y0 = y < 0 ? 0 : y;
    int x1 = x + w - 1;
    int y1 = y + h - 1;
    if (x1 >= kScreenWidth) {
        x1 = kScreenWidth - 1;
    }
    if (y1 >= kScreenHeight) {
        y1 = kScreenHeight - 1;
    }

    const uint8_t col[] = {
        static_cast<uint8_t>((x0 >> 8) & 0xff),
        static_cast<uint8_t>(x0 & 0xff),
        static_cast<uint8_t>((x1 >> 8) & 0xff),
        static_cast<uint8_t>(x1 & 0xff),
    };
    const uint8_t row[] = {
        static_cast<uint8_t>((y0 >> 8) & 0xff),
        static_cast<uint8_t>(y0 & 0xff),
        static_cast<uint8_t>((y1 >> 8) & 0xff),
        static_cast<uint8_t>(y1 & 0xff),
    };

    bitbang_write_commandn_held(0x2a, col, sizeof(col));
    bitbang_write_commandn_held(0x2b, row, sizeof(row));
}
#endif

void reset_panel() {
    gpio_put(board::kLcdPinRst, 1);
    sleep_ms(1);
    gpio_put(board::kLcdPinRst, 0);
    sleep_ms(10);
    gpio_put(board::kLcdPinRst, 1);
    sleep_ms(10);
}

void set_window(int x, int y, int w, int h) {
    const int x0 = x;
    const int y0 = y;
    const int x1 = x + w - 1;
    const int y1 = y + h - 1;

    const uint8_t col[] = {
        static_cast<uint8_t>((x0 >> 8) & 0xff),
        static_cast<uint8_t>(x0 & 0xff),
        static_cast<uint8_t>((x1 >> 8) & 0xff),
        static_cast<uint8_t>(x1 & 0xff),
    };
    const uint8_t row[] = {
        static_cast<uint8_t>((y0 >> 8) & 0xff),
        static_cast<uint8_t>(y0 & 0xff),
        static_cast<uint8_t>((y1 >> 8) & 0xff),
        static_cast<uint8_t>(y1 & 0xff),
    };

    write_commandn(0x2a, col, sizeof(col));
    write_commandn(0x2b, row, sizeof(row));
    write_command(0x2c);
}

void lcd_fill_rect(int x, int y, int w, int h, uint16_t rgb565) {
    if (w <= 0 || h <= 0) {
        return;
    }

    set_window(x, y, w, h);

    uint8_t line[kScreenWidth * 2];
    for (int px = 0; px < w; ++px) {
        line[px * 2] = static_cast<uint8_t>(rgb565 >> 8);
        line[px * 2 + 1] = static_cast<uint8_t>(rgb565 & 0xff);
    }

    select();
    set_dc(true);
    for (int row = 0; row < h; ++row) {
        write_bytes(line, static_cast<size_t>(w) * 2);
    }
    wait_idle();
    deselect();
}

void draw_char(int x, int y, char ch, uint16_t fg, uint16_t bg, int scale) {
    const uint8_t* glyph = font::cozette_glyph(ch);
    for (int row = 0; row < font::kCozetteHeight; ++row) {
        const uint8_t bits = glyph[row];
        for (int col = 0; col < font::kCozetteWidth; ++col) {
            const bool on = ((bits >> (font::kCozetteWidth - 1 - col)) & 1u) != 0;
            lcd_fill_rect(x + col * scale, y + row * scale, scale, scale, on ? fg : bg);
        }
    }
}

void lcd_draw_text(int x, int y, const char* text, uint16_t fg, uint16_t bg, int scale) {
    int cursor = x;
    while (*text != '\0') {
        draw_char(cursor, y, *text, fg, bg, scale);
        cursor += font::kCozetteWidth * scale;
        ++text;
    }
}

int lcd_text_width(const char* text, int scale) {
    int count = 0;
    while (text[count] != '\0') {
        ++count;
    }
    return count * font::kCozetteWidth * scale;
}

void lcd_draw_hline(int x, int y, int w, uint16_t color) {
    lcd_fill_rect(x, y, w, 1, color);
}

void lcd_draw_vline(int x, int y, int h, uint16_t color) {
    lcd_fill_rect(x, y, 1, h, color);
}

void lcd_draw_frame(int x, int y, int w, int h, uint16_t color) {
    lcd_draw_hline(x, y, w, color);
    lcd_draw_hline(x, y + h - 1, w, color);
    lcd_draw_vline(x, y, h, color);
    lcd_draw_vline(x + w - 1, y, h, color);
}

void lcd_draw_text_band(int x, int y, int w, int h, const char* text, uint16_t fg, uint16_t bg) {
    static uint8_t band[kScreenWidth * 18 * 2];
    if (w <= 0 || h <= 0 || w > kScreenWidth || h > 18) {
        return;
    }

    for (int i = 0; i < w * h; ++i) {
        band[i * 2] = static_cast<uint8_t>(bg >> 8);
        band[i * 2 + 1] = static_cast<uint8_t>(bg & 0xff);
    }

    int cursor = 0;
    constexpr int text_y = 2;
    while (*text != '\0' && cursor + font::kCozetteWidth <= w) {
        const uint8_t* glyph = font::cozette_glyph(*text);
        for (int row = 0; row < font::kCozetteHeight && text_y + row < h; ++row) {
            const uint8_t bits = glyph[row];
            for (int col = 0; col < font::kCozetteWidth; ++col) {
                if (((bits >> (font::kCozetteWidth - 1 - col)) & 1u) == 0) {
                    continue;
                }
                const int px = cursor + col;
                const int py = text_y + row;
                const int offset = (py * w + px) * 2;
                band[offset] = static_cast<uint8_t>(fg >> 8);
                band[offset + 1] = static_cast<uint8_t>(fg & 0xff);
            }
        }
        cursor += font::kCozetteWidth;
        ++text;
    }

    set_window(x, y, w, h);
    select();
    set_dc(true);
    write_bytes(band, static_cast<size_t>(w) * static_cast<size_t>(h) * 2);
    wait_idle();
    deselect();
}

void lcd_draw_text_large_band(int x, int y, int w, int h, const char* text, uint16_t fg, uint16_t bg) {
    static uint8_t band[kScreenWidth * 24 * 2];
    if (w <= 0 || h <= 0 || w > kScreenWidth || h > 24) {
        return;
    }

    for (int i = 0; i < w * h; ++i) {
        band[i * 2] = static_cast<uint8_t>(bg >> 8);
        band[i * 2 + 1] = static_cast<uint8_t>(bg & 0xff);
    }

    int cursor = 0;
    constexpr int text_y = 0;
    const uint16_t fg_r = static_cast<uint16_t>((fg >> 11) & 0x1f);
    const uint16_t fg_g = static_cast<uint16_t>((fg >> 5) & 0x3f);
    const uint16_t fg_b = static_cast<uint16_t>(fg & 0x1f);
    const uint16_t bg_r = static_cast<uint16_t>((bg >> 11) & 0x1f);
    const uint16_t bg_g = static_cast<uint16_t>((bg >> 5) & 0x3f);
    const uint16_t bg_b = static_cast<uint16_t>(bg & 0x1f);
    while (*text != '\0' && cursor + font::kOverlayWidth <= w) {
        const uint8_t (*glyph)[(font::kOverlayWidth + 1) / 2] = font::overlay_glyph(*text);
        for (int row = 0; row < font::kOverlayHeight && text_y + row < h; ++row) {
            for (int col = 0; col < font::kOverlayWidth; ++col) {
                const uint8_t packed = glyph[row][col / 2];
                const uint8_t alpha = (col & 1) == 0 ? (packed >> 4) : (packed & 0x0f);
                if (alpha == 0) {
                    continue;
                }
                const uint16_t inv = static_cast<uint16_t>(15 - alpha);
                const uint16_t r = static_cast<uint16_t>((fg_r * alpha + bg_r * inv + 7) / 15);
                const uint16_t g = static_cast<uint16_t>((fg_g * alpha + bg_g * inv + 7) / 15);
                const uint16_t b = static_cast<uint16_t>((fg_b * alpha + bg_b * inv + 7) / 15);
                const uint16_t color = static_cast<uint16_t>((r << 11) | (g << 5) | b);
                const int px = cursor + col;
                const int py = text_y + row;
                const int offset = (py * w + px) * 2;
                band[offset] = static_cast<uint8_t>(color >> 8);
                band[offset + 1] = static_cast<uint8_t>(color & 0xff);
            }
        }
        cursor += font::kOverlayWidth;
        ++text;
    }

    set_window(x, y, w, h);
    select();
    set_dc(true);
    write_bytes(band, static_cast<size_t>(w) * static_cast<size_t>(h) * 2);
    wait_idle();
    deselect();
}

#if PICOMENT_FONT_SAMPLE_BUILD
void lcd_draw_spleen_native_text_band(int x,
                                      int y,
                                      int w,
                                      int h,
                                      const char* text,
                                      font::SpleenNativeSize size,
                                      uint16_t fg,
                                      uint16_t bg) {
    static uint8_t band[kScreenWidth * 70 * 2];
    if (w <= 0 || h <= 0 || w > kScreenWidth || h > 70) {
        return;
    }

    for (int i = 0; i < w * h; ++i) {
        band[i * 2] = static_cast<uint8_t>(bg >> 8);
        band[i * 2 + 1] = static_cast<uint8_t>(bg & 0xff);
    }

    const font::SpleenNativeFont native_font = font::spleen_native_font(size);
    int cursor = 0;
    while (*text != '\0' && cursor + native_font.width <= w) {
        const uint8_t* glyph = font::spleen_native_glyph(native_font, *text);
        for (int row = 0; row < native_font.height && row < h; ++row) {
            for (int col = 0; col < native_font.width; ++col) {
                const uint8_t packed = glyph[row * native_font.packed_width + col / 2];
                const uint8_t alpha = (col & 1) == 0 ? (packed >> 4) : (packed & 0x0f);
                if (alpha == 0) {
                    continue;
                }
                const int px = cursor + col;
                const int py = row;
                const int offset = (py * w + px) * 2;
                band[offset] = static_cast<uint8_t>(fg >> 8);
                band[offset + 1] = static_cast<uint8_t>(fg & 0xff);
            }
        }
        cursor += native_font.width;
        ++text;
    }

    set_window(x, y, w, h);
    select();
    set_dc(true);
    write_bytes(band, static_cast<size_t>(w) * static_cast<size_t>(h) * 2);
    wait_idle();
    deselect();
}

void lcd_draw_spleen_native_text_3x2_band(int x,
                                          int y,
                                          int w,
                                          int h,
                                          const char* text,
                                          font::SpleenNativeSize size,
                                          uint16_t fg,
                                          uint16_t bg) {
    static uint8_t band[kScreenWidth * 100 * 2];
    if (w <= 0 || h <= 0 || w > kScreenWidth || h > 100) {
        return;
    }

    for (int i = 0; i < w * h; ++i) {
        band[i * 2] = static_cast<uint8_t>(bg >> 8);
        band[i * 2 + 1] = static_cast<uint8_t>(bg & 0xff);
    }

    const font::SpleenNativeFont native_font = font::spleen_native_font(size);
    const int scaled_w = native_font.width * 3 / 2;
    const int scaled_h = native_font.height * 3 / 2;
    int cursor = 0;
    while (*text != '\0' && cursor + scaled_w <= w) {
        const uint8_t* glyph = font::spleen_native_glyph(native_font, *text);
        for (int sy = 0; sy < scaled_h && sy < h; ++sy) {
            const int row = sy * 2 / 3;
            for (int sx = 0; sx < scaled_w; ++sx) {
                const int col = sx * 2 / 3;
                const uint8_t packed = glyph[row * native_font.packed_width + col / 2];
                const uint8_t alpha = (col & 1) == 0 ? (packed >> 4) : (packed & 0x0f);
                if (alpha == 0) {
                    continue;
                }
                const int px = cursor + sx;
                const int offset = (sy * w + px) * 2;
                band[offset] = static_cast<uint8_t>(fg >> 8);
                band[offset + 1] = static_cast<uint8_t>(fg & 0xff);
            }
        }
        cursor += scaled_w;
        ++text;
    }

    set_window(x, y, w, h);
    select();
    set_dc(true);
    write_bytes(band, static_cast<size_t>(w) * static_cast<size_t>(h) * 2);
    wait_idle();
    deselect();
}
#endif

void draw_chip(int x, int y, const char* text, uint16_t fg, uint16_t fill, uint16_t edge) {
    const int w = lcd_text_width(text, 1) + 12;
    lcd_fill_rect(x, y, w, 19, fill);
    lcd_draw_frame(x, y, w, 19, edge);
    lcd_draw_text(x + 6, y + 3, text, fg, fill, 1);
}

void draw_keyboard_strip() {
    constexpr uint16_t bg = 0x0000;
    constexpr uint16_t edge = 0x39e7;
    constexpr uint16_t accent = 0x07e0;
    constexpr uint16_t fg = 0xdffb;
    constexpr uint16_t dim = 0x7bef;
    const char* top = "1 2 3 4 5 6 7 8 9 0";
    const char* bottom = "Q W E R T Y U I O P";

    lcd_draw_text(18, 142, "THUMB KEYS", dim, bg, 1);
    lcd_draw_frame(18, 160, 284, 56, edge);
    lcd_draw_hline(18, 188, 284, edge);

    for (int i = 0; i < 10; ++i) {
        const int x = 20 + i * 28;
        if (i > 0) {
            lcd_draw_vline(18 + i * 28, 160, 56, 0x18c3);
        }
        char key1[2] = {top[i * 2], 0};
        char key2[2] = {bottom[i * 2], 0};
        lcd_draw_text(x + 8, 168, key1, fg, bg, 1);
        lcd_draw_text(x + 8, 196, key2, accent, bg, 1);
    }
}

}  // namespace

void init() {
    gpio_init(board::kLcdPinCs);
    gpio_init(board::kLcdPinDc);
    gpio_init(board::kLcdPinRst);
    gpio_init(board::kLcdPinRamCs);
    gpio_init(board::kLcdPinMiso);

    gpio_set_dir(board::kLcdPinCs, GPIO_OUT);
    gpio_set_dir(board::kLcdPinDc, GPIO_OUT);
    gpio_set_dir(board::kLcdPinRst, GPIO_OUT);
    gpio_set_dir(board::kLcdPinRamCs, GPIO_OUT);
    gpio_set_dir(board::kLcdPinMiso, GPIO_IN);
    gpio_disable_pulls(board::kLcdPinMiso);

    gpio_put(board::kLcdPinCs, 1);
    gpio_put(board::kLcdPinDc, 1);
    gpio_put(board::kLcdPinRst, 1);
    gpio_put(board::kLcdPinRamCs, 1);

    g_pio_offset = pio_add_program(g_pio, &lcd_spi_min_program);
    lcd_spi_min_program_init(g_pio,
                             g_sm,
                             g_pio_offset,
                             board::kLcdPinMosi,
                             board::kLcdPinSck,
                             kPioClkDiv);

    reset_panel();

    static const uint8_t b9[] = {0x02, 0xe0};
    static const uint8_t c0[] = {0x80, 0x06};
    static const uint8_t e8[] = {0x40, 0x8a, 0x00, 0x00, 0x29, 0x19, 0xaa, 0x33};
    static const uint8_t e0[] = {0xf0, 0x06, 0x0f, 0x05, 0x04, 0x20, 0x37, 0x33,
                                 0x4c, 0x37, 0x13, 0x14, 0x2b, 0x31};
    static const uint8_t e1[] = {0xf0, 0x11, 0x1b, 0x11, 0x0f, 0x0a, 0x37, 0x43,
                                 0x4c, 0x37, 0x13, 0x13, 0x2c, 0x32};

    write_command1(0xf0, 0xc3);
    write_command1(0xf0, 0x96);
    write_command1(0x36, 0x48);
    write_command1(0x3a, 0x65);
    write_command1(0xb1, 0xa0);
    write_command1(0xb4, 0x00);
    write_command1(0xb7, 0xc6);
    write_commandn(0xb9, b9, sizeof(b9));
    write_commandn(0xc0, c0, sizeof(c0));
    write_command1(0xc1, 0x15);
    write_command1(0xc2, 0xa7);
    write_command1(0xc5, 0x04);
    write_commandn(0xe8, e8, sizeof(e8));
    write_commandn(0xe0, e0, sizeof(e0));
    write_commandn(0xe1, e1, sizeof(e1));
    write_command1(0xf0, 0x3c);
    write_command1(0xf0, 0x69);
    write_command1(0x35, 0x00);

    write_command(0x11);
    sleep_ms(120);
    write_command(0x21);
    clear(0x0000);
    write_command(0x29);
    sleep_ms(120);

    PM_LOG_BOOT("lcd=init pio=pio0 sm=0 clkdiv=%.2f rgb565_cmd=0x65", static_cast<double>(kPioClkDiv));
}

void clear(uint16_t rgb565) {
    lcd_fill_rect(0, 0, kScreenWidth, kScreenHeight, rgb565);
}

void fill_rect(int x, int y, int w, int h, uint16_t rgb565) {
    lcd_fill_rect(x, y, w, h, rgb565);
}

void draw_frame(int x, int y, int w, int h, uint16_t color) {
    lcd_draw_frame(x, y, w, h, color);
}

void draw_text_band(int x, int y, int w, int h, const char* text, uint16_t fg, uint16_t bg) {
    lcd_draw_text_band(x, y, w, h, text, fg, bg);
}

void draw_text_large_band(int x, int y, int w, int h, const char* text, uint16_t fg, uint16_t bg) {
    lcd_draw_text_large_band(x, y, w, h, text, fg, bg);
}

void draw_spleen_native_text_band(int x,
                                  int y,
                                  int w,
                                  int h,
                                  const char* text,
                                  font::SpleenNativeSize size,
                                  uint16_t fg,
                                  uint16_t bg) {
#if PICOMENT_FONT_SAMPLE_BUILD
    lcd_draw_spleen_native_text_band(x, y, w, h, text, size, fg, bg);
#else
    (void)x;
    (void)y;
    (void)w;
    (void)h;
    (void)text;
    (void)size;
    (void)fg;
    (void)bg;
#endif
}

void draw_spleen_native_text_3x2_band(int x,
                                      int y,
                                      int w,
                                      int h,
                                      const char* text,
                                      font::SpleenNativeSize size,
                                      uint16_t fg,
                                      uint16_t bg) {
#if PICOMENT_FONT_SAMPLE_BUILD
    lcd_draw_spleen_native_text_3x2_band(x, y, w, h, text, size, fg, bg);
#else
    (void)x;
    (void)y;
    (void)w;
    (void)h;
    (void)text;
    (void)size;
    (void)fg;
    (void)bg;
#endif
}

void draw_boot_screen(const char* version) {
    constexpr uint16_t bg = 0x0000;
    constexpr uint16_t panel = 0x0861;
    constexpr uint16_t fg = 0xdffb;
    constexpr uint16_t accent = 0x07e0;
    constexpr uint16_t dim = 0x7bef;
    constexpr uint16_t edge = 0x39e7;

    clear(bg);
    lcd_fill_rect(0, 0, kScreenWidth, 4, accent);
    lcd_fill_rect(0, 4, kScreenWidth, 34, panel);
    lcd_draw_text(14, 14, "BUILD", fg, panel, 1);
    lcd_draw_text(56, 14, version, dim, panel, 1);

    lcd_draw_text(18, 58, "STANDALONE SYNTH", fg, bg, 1);
    lcd_draw_text(18, 78, "PLAY MODE: C MAJOR", accent, bg, 1);
    lcd_draw_text(18, 98, "AUDIO: PRA32U PWM STREAM", accent, bg, 1);

    draw_chip(18, 120, "UART", fg, 0x10a2, edge);
    draw_chip(74, 120, "KBD OK", accent, 0x10a2, edge);
    draw_chip(148, 120, "RGB565", fg, 0x10a2, edge);
    draw_chip(228, 120, "COZETTE", fg, 0x10a2, edge);

    draw_keyboard_strip();

    lcd_draw_text(18, 232, "LAST INPUT", dim, bg, 1);
    lcd_draw_frame(18, 250, 284, 35, edge);
    lcd_draw_text(30, 261, "WAITING FOR KEY", fg, bg, 1);
    lcd_draw_text(18, 300, "BOOTING PRA32U PWM", dim, bg, 1);

    PM_LOG_BOOT("ui=instrument_panel font=cozette scale=1");
}

void draw_status_line(int line, const char* text, uint16_t color) {
    constexpr uint16_t bg = 0x0000;
    constexpr uint16_t edge = 0x39e7;
    const int y = 250 + line * 18;
    if (line == 0) {
        lcd_draw_frame(18, 250, 284, 35, edge);
    }
    lcd_draw_text_band(30, y + 6, 260, 16, text, color, bg);
}

#if PICOMENT_SCREENSHOT_CAPTURE_BUILD
bool readback_probe_raw(int x, int y, uint8_t* raw, size_t raw_len) {
    if (raw == nullptr || raw_len == 0) {
        return false;
    }

    wait_idle();
    set_bitbang_mode(true);
    select();
    set_read_window_held(x, y, 2, 2);
    set_dc(false);
    bitbang_write_byte(0x2e);
    set_dc(true);
    (void)bitbang_read_byte_falling();
    for (size_t i = 0; i < raw_len; ++i) {
        raw[i] = bitbang_read_byte_falling();
    }
    deselect();
    set_bitbang_mode(false);
    return true;
}

bool readback_rect_rgb565(int x, int y, int w, int h, uint16_t* dst) {
    if (w <= 0 || h <= 0 || dst == nullptr) {
        return false;
    }
    if (x < 0 || y < 0 || x + w > kScreenWidth || y + h > kScreenHeight) {
        return false;
    }

    wait_idle();
    set_bitbang_mode(true);
    select();
    set_read_window_held(x, y, w, h);
    set_dc(false);
    bitbang_write_byte(0x2e);
    set_dc(true);
    (void)bitbang_read_byte_falling();

    const int pixel_count = w * h;
    for (int i = 0; i < pixel_count; ++i) {
        const uint8_t hi = bitbang_read_byte_falling();
        const uint8_t lo = bitbang_read_byte_falling();
        dst[i] = static_cast<uint16_t>((static_cast<uint16_t>(hi) << 8) | lo);
    }

    deselect();
    set_bitbang_mode(false);
    return true;
}

#endif

#if PICOMENT_FONT_SAMPLE_BUILD
void draw_font_sample_screen(const char* version) {
    constexpr uint16_t bg = 0x0000;
    constexpr uint16_t panel = 0x0861;
    constexpr uint16_t fg = 0xdffb;
    constexpr uint16_t dim = 0x7bef;
    constexpr uint16_t accent = 0x07ff;
    constexpr uint16_t warn = 0xfde0;

    clear(bg);
    lcd_fill_rect(0, 0, kScreenWidth, 4, accent);
    lcd_fill_rect(0, 4, kScreenWidth, 34, panel);
    lcd_draw_text_band(14, 14, 292, 18, version, fg, panel);

    lcd_draw_text_band(14, 44, 292, 18, "SPLEEN NATIVE FONT SAMPLE", accent, bg);
    lcd_draw_spleen_native_text_band(14, 66, 292, 8, "5x8  ABC[] key 123", font::SpleenNativeSize::S5x8, fg, bg);
    lcd_draw_spleen_native_text_band(14, 82, 292, 12, "6x12 ABC[] key 123", font::SpleenNativeSize::S6x12, fg, bg);
    lcd_draw_spleen_native_text_band(14, 102, 292, 16, "8x16 ABC[] key 123", font::SpleenNativeSize::S8x16, fg, bg);
    lcd_draw_spleen_native_text_band(14, 128, 292, 24, "12x24 ABC[]", font::SpleenNativeSize::S12x24, warn, bg);
    lcd_draw_spleen_native_text_band(14, 164, 292, 32, "16x32 ABC[]", font::SpleenNativeSize::S16x32, fg, bg);
    lcd_draw_spleen_native_text_band(14, 210, 292, 64, "32x64 []", font::SpleenNativeSize::S32x64, accent, bg);

    lcd_draw_text_band(14, 292, 292, 18, "NO SCALING / NATIVE BDF SIZE", dim, bg);
    PM_LOG_BOOT("ui=font_sample font=spleen sizes=5x8,6x12,8x16,12x24,16x32,32x64 native=1");
}
#endif

}  // namespace picoment::display
