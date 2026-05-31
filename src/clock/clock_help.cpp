#include "clock/clock_help.h"

#include <cstdio>

#include "platform/picocalc_display.h"

namespace {

constexpr uint16_t kBlack = 0x0000;
constexpr uint16_t kWhite = 0xffff;
constexpr uint16_t kDim = 0x7bef;
constexpr uint16_t kHighlightDigit = 0x07ff;

}  // namespace

void draw_clock_help_screen(size_t page) {
    picoment::display::clear(kBlack);
    if (page >= kClockHelpPageCount) {
        page = kClockHelpPageCount - 1;
    }

    char header[48];
    std::snprintf(header, sizeof(header), "Clock Help %u/%u",
                  static_cast<unsigned>(page + 1),
                  static_cast<unsigned>(kClockHelpPageCount));
    picoment::display::draw_text_band(8, 8, 304, 18, header,
                                      kHighlightDigit, kBlack);
    if (page == 0) {
        picoment::display::draw_text_band(8, 36, 304, 16, "[F6] alarm settings", kWhite, kBlack);
        picoment::display::draw_text_band(8, 56, 304, 16, "[F7] clock settings", kWhite, kBlack);
        picoment::display::draw_text_band(8, 76, 304, 16, "[F8] set date and time", kWhite, kBlack);
        picoment::display::draw_text_band(8, 96, 304, 16, "[Power] short: backlight off/on", kWhite, kBlack);
        picoment::display::draw_text_band(8, 116, 304, 16, "[Space] peek backlight while off", kWhite, kBlack);
        picoment::display::draw_text_band(8, 136, 304, 16, "[Home] screenshot clk_####.BMP", kWhite, kBlack);
        picoment::display::draw_text_band(8, 156, 304, 16, "[C] hold: calendar", kWhite, kBlack);
        picoment::display::draw_text_band(8, 176, 304, 16, "[F10] close help", kWhite, kBlack);
    } else {
        picoment::display::draw_text_band(8, 36, 304, 16, "License", kHighlightDigit, kBlack);
        picoment::display::draw_text_band(8, 60, 304, 16, "Picocalc_Clock: MIT", kWhite, kBlack);
        picoment::display::draw_text_band(8, 84, 304, 16, "Bundled modules keep their", kWhite, kBlack);
        picoment::display::draw_text_band(8, 100, 304, 16, "original licenses.", kWhite, kBlack);
        picoment::display::draw_text_band(8, 124, 304, 16, "See LICENSE and docs.", kDim, kBlack);
    }
}
