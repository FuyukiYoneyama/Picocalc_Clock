#include "life/life_runtime.h"

#include <cstdio>

#include "hardware/timer.h"
#include "platform/picocalc_display.h"

namespace {

constexpr uint16_t kBlack = 0x0000;
constexpr uint16_t kAlive = 0x07e0;  // green
constexpr int kLifeCellPixels = 2;

// Time overlay geometry — shared between draw_life_cell (clip) and draw_life_time_overlay (draw)
constexpr int kOverlayW = 5 * 12;
constexpr int kOverlayH = 24;
constexpr int kOverlayX = picoment::display::kScreenWidth - kOverlayW - 4;
constexpr int kOverlayY = picoment::display::kFooterY;

enum class LifeInitialMode : uint8_t {
    FullRandom,
    CenterBurst,
    QuadBurst,
    MirroredQuadrants,
};

void draw_life_cell(int x, int y, bool alive) {
    picoment::display::fill_rect(x * kLifeCellPixels,
                                 y * kLifeCellPixels,
                                 kLifeCellPixels,
                                 kLifeCellPixels,
                                 alive ? kAlive : kBlack);
}

void draw_life_initial_board(LifeRuntime* life_state) {
    life_state->board.reset_visible();
    picoment::display::clear(kBlack);
    for (int y = 0; y < life::kCellHeight; ++y) {
        for (int x = 0; x < life::kCellWidth; ++x) {
            const bool alive = life_state->board.cell(x, y);
            if (alive) {
                draw_life_cell(x, y, true);
                life_state->board.set_visible_cell(x, y, true);
            }
        }
    }
}

uint32_t draw_life_diff(LifeRuntime* life_state) {
    uint32_t drawn = 0;
    for (int y = 0; y < life::kCellHeight; ++y) {
        for (int x = 0; x < life::kCellWidth; ++x) {
            const bool alive = life_state->board.cell(x, y);
            if (alive == life_state->board.visible_cell(x, y)) {
                continue;
            }
            draw_life_cell(x, y, alive);
            life_state->board.set_visible_cell(x, y, alive);
            ++drawn;
        }
    }
    return drawn;
}

uint32_t mix_life_seed(uint32_t seed) {
    if (seed == 0) {
        seed = 0x6d2b79f5u;
    }
    seed ^= seed << 13;
    seed ^= seed >> 17;
    seed ^= seed << 5;
    return seed;
}

LifeInitialMode choose_life_initial_mode(uint32_t seed) {
    switch (mix_life_seed(seed) & 0x03u) {
    case 0:
        return LifeInitialMode::FullRandom;
    case 1:
        return LifeInitialMode::CenterBurst;
    case 2:
        return LifeInitialMode::QuadBurst;
    default:
        return LifeInitialMode::MirroredQuadrants;
    }
}

const char* life_initial_mode_name(LifeInitialMode mode) {
    switch (mode) {
    case LifeInitialMode::FullRandom:
        return "full";
    case LifeInitialMode::CenterBurst:
        return "center";
    case LifeInitialMode::QuadBurst:
        return "quad";
    case LifeInitialMode::MirroredQuadrants:
        return "mirrored";
    }
    return "unknown";
}

void initialize_life_board(life::Board* board,
                           LifeInitialMode mode,
                           uint32_t seed) {
    switch (mode) {
    case LifeInitialMode::FullRandom:
        board->randomize(seed, 30);
        break;
    case LifeInitialMode::CenterBurst:
        board->randomize_center_burst(seed);
        break;
    case LifeInitialMode::QuadBurst:
        board->randomize_quad_burst(seed);
        break;
    case LifeInitialMode::MirroredQuadrants:
        board->randomize_mirrored_quadrants(seed);
        break;
    }
}

}  // namespace

void start_life(LifeRuntime* life_state, bool hourly, uint32_t now_ms) {
    const uint32_t seed = time_us_32() ^ now_ms ^
                          (hourly ? 0x51f15eedu : 0x1a2b3c4du);
    const LifeInitialMode mode = choose_life_initial_mode(seed);
    initialize_life_board(&life_state->board, mode, seed);
    life_state->tracker.reset();
    life_state->active = true;
    life_state->hourly = hourly;
    life_state->started_ms = now_ms;
    life_state->generation = 0;
    life_state->live_count = life_state->board.live_count();
    std::printf("LIFE start source=%s mode=%s live=%lu\r\n",
                hourly ? "hourly" : "manual",
                life_initial_mode_name(mode),
                static_cast<unsigned long>(life_state->live_count));
    draw_life_initial_board(life_state);
}

bool step_life(LifeRuntime* life_state) {
    const life::StepResult result = life_state->board.step();
    ++life_state->generation;
    life_state->live_count = result.live_count;
    const uint32_t drawn = draw_life_diff(life_state);
    const life::StableReason reason = life_state->tracker.observe(result);
    if (reason == life::StableReason::None) {
        return false;
    }

    std::printf("LIFE end reason=%s gen=%lu live=%lu drawn=%lu\r\n",
                life::stable_reason_name(reason),
                static_cast<unsigned long>(life_state->generation),
                static_cast<unsigned long>(life_state->live_count),
                static_cast<unsigned long>(drawn));
    return true;
}

void draw_life_time_overlay(const ds3231_datetime_t& dt, bool rtc_ok) {
    char text[9];
    uint16_t color;
    if (rtc_ok) {
        std::snprintf(text, sizeof(text), "%02u:%02u", dt.hour, dt.minute);
        color = 0x07ffu;
    } else {
        std::snprintf(text, sizeof(text), "--:--");
        color = 0xfde0u;
    }
    picoment::display::fill_rect(kOverlayX, kOverlayY, kOverlayW, kOverlayH, kBlack);
    picoment::display::draw_spleen_native_text_band(
        kOverlayX, kOverlayY, kOverlayW, kOverlayH, text,
        picoment::font::SpleenNativeSize::S12x24, color, kBlack);
}

void stop_life(LifeRuntime* life_state, const char* reason) {
    life_state->active = false;
    std::printf("LIFE stop reason=%s gen=%lu live=%lu\r\n",
                reason,
                static_cast<unsigned long>(life_state->generation),
                static_cast<unsigned long>(life_state->live_count));
}

bool same_life_hour(const LifeHourRecord& record,
                    const ds3231_datetime_t& dt) {
    return record.valid &&
           record.year == dt.year &&
           record.month == dt.month &&
           record.day == dt.day &&
           record.hour == dt.hour;
}

void record_life_hour(LifeHourRecord* record,
                      const ds3231_datetime_t& dt) {
    record->year = dt.year;
    record->month = dt.month;
    record->day = dt.day;
    record->hour = dt.hour;
    record->valid = true;
}
