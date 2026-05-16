#include "ui.h"

#include <cstdio>
#include "platform/picocalc_display.h"
#include "platform/picocalc_keyboard.h"

namespace {

bool g_display_initialized = false;
bool g_keyboard_initialized = false;

}  // namespace

bool ui_init() {
    picoment::display::init();
    g_display_initialized = true;

    picoment::keyboard::init();
    g_keyboard_initialized = true;
    return true;
}

bool ui_keyboard_probe() {
    return g_keyboard_initialized;
}

void ui_show_lcd_test(const char *version) {
    if (!g_display_initialized) {
        std::printf("lcd-test: FAIL reason=display_not_initialized\r\n");
        return;
    }

    std::printf("lcd-test: draw_font_sample_screen start\r\n");
    picoment::display::draw_font_sample_screen(version);
    std::printf("lcd-test: result=VISUAL_ONLY path=fonttest draw_font_sample_screen no_readback\r\n");
}
