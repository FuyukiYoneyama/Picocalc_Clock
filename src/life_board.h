#pragma once

#include <array>
#include <stdint.h>

namespace life {

constexpr int kCellWidth = 160;
constexpr int kCellHeight = 160;
constexpr int kCellCount = kCellWidth * kCellHeight;
constexpr int kBoardBytes = kCellCount / 8;

enum class StableReason : uint8_t {
    None,
    Empty,
    Unchanged,
    PeriodHash,
};

struct StepResult {
    uint32_t live_count;
    uint32_t changed_count;
    uint64_t hash;
    bool unchanged;
};

class Board {
public:
    void clear();
    void randomize(uint32_t seed, uint8_t density_percent);
    void set_cell(int x, int y, bool alive);
    bool cell(int x, int y) const;
    bool visible_cell(int x, int y) const;
    void set_visible_cell(int x, int y, bool alive);
    void reset_visible();
    StepResult step();
    uint32_t live_count() const;
    uint64_t hash() const;

    void debug_block();
    void debug_blinker();
    void debug_empty();
    void debug_wrap_blinker();
    void debug_wrap_corner();

private:
    using Bits = std::array<uint8_t, kBoardBytes>;

    static bool bit_at(const Bits& bits, int x, int y);
    static void set_bit(Bits& bits, int x, int y, bool alive);
    int live_neighbors(int x, int y) const;

    Bits current_ = {};
    Bits next_ = {};
    Bits visible_ = {};
};

class StabilityTracker {
public:
    void reset();
    StableReason observe(const StepResult& result);

private:
    static constexpr int kHistorySize = 16;
    std::array<uint64_t, kHistorySize> hashes_ = {};
    int count_ = 0;
    int next_ = 0;
};

const char* stable_reason_name(StableReason reason);

}  // namespace life
