#include "platform/picocalc_lcd_hw_baseline.h"

#include <stddef.h>
#include <stdint.h>

#include "config/board_config.h"
#include "hardware/gpio.h"
#include "hardware/spi.h"
#include "pico/stdlib.h"

namespace picocalc::lcd_hw_baseline {
namespace {

constexpr uint32_t kSpiWriteHz = 25 * 1000 * 1000;
constexpr int kScreenWidth = 320;
constexpr int kScreenHeight = 320;

void select() {
    gpio_put(picoment::board::kLcdPinCs, 0);
}

void deselect() {
    gpio_put(picoment::board::kLcdPinCs, 1);
}

void set_dc(bool data) {
    gpio_put(picoment::board::kLcdPinDc, data ? 1 : 0);
}

void finish_spi() {
    while (spi_is_readable(spi1)) {
        (void)spi_get_hw(spi1)->dr;
    }
    while (spi_get_hw(spi1)->sr & SPI_SSPSR_BSY_BITS) {
        tight_loop_contents();
    }
    while (spi_is_readable(spi1)) {
        (void)spi_get_hw(spi1)->dr;
    }
    spi_get_hw(spi1)->icr = SPI_SSPICR_RORIC_BITS;
}

void write_fast(const uint8_t* data, size_t len) {
    while (len > 0) {
        while (!spi_is_writable(spi1)) {
            tight_loop_contents();
        }
        spi_get_hw(spi1)->dr = *data++;
        --len;
    }
}

void write_command(uint8_t command) {
    select();
    set_dc(false);
    spi_write_blocking(spi1, &command, 1);
    deselect();
}

void write_data(const uint8_t* data, size_t len) {
    if (data == nullptr || len == 0) {
        return;
    }
    select();
    set_dc(true);
    spi_write_blocking(spi1, data, len);
    deselect();
}

void write_command1(uint8_t command, uint8_t value) {
    write_command(command);
    write_data(&value, 1);
}

void write_commandn(uint8_t command, const uint8_t* data, size_t len) {
    write_command(command);
    write_data(data, len);
}

void reset_panel() {
    gpio_put(picoment::board::kLcdPinRst, 1);
    sleep_ms(10);
    gpio_put(picoment::board::kLcdPinRst, 0);
    sleep_ms(10);
    gpio_put(picoment::board::kLcdPinRst, 1);
    sleep_ms(200);
}

void init_spi_pins() {
    spi_init(spi1, kSpiWriteHz);

    gpio_init(picoment::board::kLcdPinCs);
    gpio_init(picoment::board::kLcdPinDc);
    gpio_init(picoment::board::kLcdPinRst);
    gpio_init(picoment::board::kLcdPinRamCs);

    gpio_set_dir(picoment::board::kLcdPinCs, GPIO_OUT);
    gpio_set_dir(picoment::board::kLcdPinDc, GPIO_OUT);
    gpio_set_dir(picoment::board::kLcdPinRst, GPIO_OUT);
    gpio_set_dir(picoment::board::kLcdPinRamCs, GPIO_OUT);

    gpio_put(picoment::board::kLcdPinCs, 1);
    gpio_put(picoment::board::kLcdPinDc, 1);
    gpio_put(picoment::board::kLcdPinRst, 1);
    gpio_put(picoment::board::kLcdPinRamCs, 1);

    gpio_set_function(picoment::board::kLcdPinSck, GPIO_FUNC_SPI);
    gpio_set_function(picoment::board::kLcdPinMosi, GPIO_FUNC_SPI);
    gpio_set_function(picoment::board::kLcdPinMiso, GPIO_FUNC_SPI);
    gpio_set_input_hysteresis_enabled(picoment::board::kLcdPinMiso, true);
}

void init_panel_rgb888() {
    static const uint8_t gamma_pos[] = {
        0x00, 0x03, 0x09, 0x08, 0x16, 0x0a, 0x3f, 0x78,
        0x4c, 0x09, 0x0a, 0x08, 0x16, 0x1a, 0x0f,
    };
    static const uint8_t gamma_neg[] = {
        0x00, 0x16, 0x19, 0x03, 0x0f, 0x05, 0x32, 0x45,
        0x46, 0x04, 0x0e, 0x0d, 0x35, 0x37, 0x0f,
    };
    static const uint8_t power1[] = {0x17, 0x15};
    static const uint8_t vcom[] = {0x00, 0x12, 0x80};
    static const uint8_t display_function[] = {0x02, 0x02, 0x3b};
    static const uint8_t adjust[] = {0xa9, 0x51, 0x2c, 0x82};

    reset_panel();

    write_commandn(0xe0, gamma_pos, sizeof(gamma_pos));
    write_commandn(0xe1, gamma_neg, sizeof(gamma_neg));
    write_commandn(0xc0, power1, sizeof(power1));
    write_command1(0xc1, 0x41);
    write_commandn(0xc5, vcom, sizeof(vcom));
    write_command1(0x36, 0x48);
    write_command1(0x3a, 0x66);
    write_command1(0xb0, 0x00);
    write_command1(0xb1, 0xa0);
    write_command(0x21);
    write_command1(0xb4, 0x02);
    write_commandn(0xb6, display_function, sizeof(display_function));
    write_command1(0xb7, 0xc6);
    write_command1(0xe9, 0x00);
    write_commandn(0xf7, adjust, sizeof(adjust));
    write_command(0x11);
    sleep_ms(120);
    write_command(0x29);
    sleep_ms(120);
    write_command1(0x36, 0x48);
}

void init_panel_rgb565() {
    static const uint8_t b9[] = {0x02, 0xe0};
    static const uint8_t c0[] = {0x80, 0x06};
    static const uint8_t e8[] = {0x40, 0x8a, 0x00, 0x00, 0x29, 0x19, 0xaa, 0x33};
    static const uint8_t e0[] = {0xf0, 0x06, 0x0f, 0x05, 0x04, 0x20, 0x37, 0x33,
                                 0x4c, 0x37, 0x13, 0x14, 0x2b, 0x31};
    static const uint8_t e1[] = {0xf0, 0x11, 0x1b, 0x11, 0x0f, 0x0a, 0x37, 0x43,
                                 0x4c, 0x37, 0x13, 0x13, 0x2c, 0x32};

    reset_panel();

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
    write_command(0x29);
    sleep_ms(120);
}

void set_window_for_write(int x, int y, int w, int h) {
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

void fill_rect_rgb888(int x, int y, int w, int h, uint8_t red, uint8_t green, uint8_t blue) {
    if (w <= 0 || h <= 0) {
        return;
    }
    if (x < 0 || y < 0 || x + w > kScreenWidth || y + h > kScreenHeight) {
        return;
    }

    static uint8_t row[kScreenWidth * 3];
    for (int px = 0; px < w; ++px) {
        row[px * 3] = red;
        row[px * 3 + 1] = green;
        row[px * 3 + 2] = blue;
    }

    set_window_for_write(x, y, w, h);
    select();
    set_dc(true);
    for (int line = 0; line < h; ++line) {
        write_fast(row, static_cast<size_t>(w) * 3u);
    }
    finish_spi();
    deselect();
}

void fill_rect_rgb565(int x, int y, int w, int h, uint16_t rgb565) {
    if (w <= 0 || h <= 0) {
        return;
    }
    if (x < 0 || y < 0 || x + w > kScreenWidth || y + h > kScreenHeight) {
        return;
    }

    static uint8_t row[kScreenWidth * 2];
    for (int px = 0; px < w; ++px) {
        row[px * 2] = static_cast<uint8_t>(rgb565 >> 8);
        row[px * 2 + 1] = static_cast<uint8_t>(rgb565 & 0xff);
    }

    set_window_for_write(x, y, w, h);
    select();
    set_dc(true);
    for (int line = 0; line < h; ++line) {
        write_fast(row, static_cast<size_t>(w) * 2u);
    }
    finish_spi();
    deselect();
}

void draw_test_pattern() {
    fill_rect_rgb888(0, 0, 320, 320, 0x00, 0x00, 0x00);
    sleep_ms(150);
    fill_rect_rgb888(0, 0, 320, 80, 0xff, 0x00, 0x00);
    fill_rect_rgb888(0, 80, 320, 80, 0x00, 0xff, 0x00);
    fill_rect_rgb888(0, 160, 320, 80, 0x00, 0x00, 0xff);
    fill_rect_rgb888(0, 240, 320, 80, 0xff, 0xff, 0xff);
    fill_rect_rgb888(16, 16, 64, 64, 0xff, 0xff, 0x00);
    fill_rect_rgb888(96, 16, 64, 64, 0x00, 0xff, 0xff);
    fill_rect_rgb888(176, 16, 64, 64, 0xff, 0x00, 0xff);
    fill_rect_rgb888(256, 16, 48, 64, 0xff, 0x80, 0x00);
}

void draw_rgb565_test_pattern() {
    fill_rect_rgb565(0, 0, 320, 320, 0x0000);
    sleep_ms(150);
    fill_rect_rgb565(0, 0, 320, 80, 0xf800);
    fill_rect_rgb565(0, 80, 320, 80, 0x07e0);
    fill_rect_rgb565(0, 160, 320, 80, 0x001f);
    fill_rect_rgb565(0, 240, 320, 80, 0xffff);
    fill_rect_rgb565(16, 16, 64, 64, 0xffe0);
    fill_rect_rgb565(96, 16, 64, 64, 0x07ff);
    fill_rect_rgb565(176, 16, 64, 64, 0xf81f);
    fill_rect_rgb565(256, 16, 48, 64, 0xfc00);
}

}  // namespace

bool show_test_pattern() {
    init_spi_pins();
    init_panel_rgb888();
    draw_test_pattern();
    return true;
}

bool show_rgb565_test_pattern() {
    init_spi_pins();
    init_panel_rgb565();
    draw_rgb565_test_pattern();
    return true;
}

}  // namespace picocalc::lcd_hw_baseline
