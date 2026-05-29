#include "life_board.h"

#include <algorithm>

namespace life {
namespace {

uint32_t xorshift32(uint32_t* state) {
    uint32_t x = *state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *state = x;
    return x;
}

uint64_t fnv1a64(const uint8_t* data, int len) {
    uint64_t hash = 1469598103934665603ull;
    for (int i = 0; i < len; ++i) {
        hash ^= data[i];
        hash *= 1099511628211ull;
    }
    return hash;
}

}  // namespace

void Board::clear() {
    current_.fill(0);
    next_.fill(0);
}

void Board::randomize(uint32_t seed, uint8_t density_percent) {
    if (seed == 0) {
        seed = 0x6d2b79f5u;
    }
    clear();
    for (int y = 0; y < kCellHeight; ++y) {
        for (int x = 0; x < kCellWidth; ++x) {
            set_cell(x, y, (xorshift32(&seed) % 100u) < density_percent);
        }
    }
}

void Board::set_cell(int x, int y, bool alive) {
    set_bit(current_, x, y, alive);
}

bool Board::cell(int x, int y) const {
    return bit_at(current_, x, y);
}

bool Board::visible_cell(int x, int y) const {
    return bit_at(visible_, x, y);
}

void Board::set_visible_cell(int x, int y, bool alive) {
    set_bit(visible_, x, y, alive);
}

void Board::reset_visible() {
    visible_.fill(0);
}

StepResult Board::step() {
    next_.fill(0);
    uint32_t live = 0;
    uint32_t changed = 0;

    for (int y = 0; y < kCellHeight; ++y) {
        for (int x = 0; x < kCellWidth; ++x) {
            const bool was_alive = cell(x, y);
            const int neighbors = live_neighbors(x, y);
            const bool alive = was_alive ? (neighbors == 2 || neighbors == 3) : (neighbors == 3);
            set_bit(next_, x, y, alive);
            if (alive) {
                ++live;
            }
            if (alive != was_alive) {
                ++changed;
            }
        }
    }

    std::swap(current_, next_);
    return {live, changed, hash(), changed == 0};
}

uint32_t Board::live_count() const {
    uint32_t live = 0;
    for (uint8_t byte : current_) {
        live += static_cast<uint32_t>(__builtin_popcount(byte));
    }
    return live;
}

uint64_t Board::hash() const {
    return fnv1a64(current_.data(), static_cast<int>(current_.size()));
}

void Board::debug_block() {
    clear();
    set_cell(79, 79, true);
    set_cell(80, 79, true);
    set_cell(79, 80, true);
    set_cell(80, 80, true);
}

void Board::debug_blinker() {
    clear();
    set_cell(79, 80, true);
    set_cell(80, 80, true);
    set_cell(81, 80, true);
}

void Board::debug_empty() {
    clear();
}

void Board::debug_wrap_blinker() {
    clear();
    set_cell(159, 80, true);
    set_cell(0, 80, true);
    set_cell(1, 80, true);
}

void Board::debug_wrap_corner() {
    clear();
    set_cell(159, 159, true);
    set_cell(0, 159, true);
    set_cell(159, 0, true);
}

bool Board::bit_at(const Bits& bits, int x, int y) {
    const int index = y * kCellWidth + x;
    return (bits[static_cast<size_t>(index >> 3)] & (1u << (index & 7))) != 0;
}

void Board::set_bit(Bits& bits, int x, int y, bool alive) {
    const int index = y * kCellWidth + x;
    const uint8_t mask = static_cast<uint8_t>(1u << (index & 7));
    uint8_t& byte = bits[static_cast<size_t>(index >> 3)];
    if (alive) {
        byte |= mask;
    } else {
        byte &= static_cast<uint8_t>(~mask);
    }
}

int Board::live_neighbors(int x, int y) const {
    int count = 0;
    for (int dy = -1; dy <= 1; ++dy) {
        for (int dx = -1; dx <= 1; ++dx) {
            if (dx == 0 && dy == 0) {
                continue;
            }
            const int nx = (x + dx + kCellWidth) % kCellWidth;
            const int ny = (y + dy + kCellHeight) % kCellHeight;
            if (cell(nx, ny)) {
                ++count;
            }
        }
    }
    return count;
}

void StabilityTracker::reset() {
    hashes_.fill(0);
    count_ = 0;
    next_ = 0;
}

StableReason StabilityTracker::observe(const StepResult& result) {
    if (result.live_count == 0) {
        return StableReason::Empty;
    }
    if (result.unchanged) {
        return StableReason::Unchanged;
    }
    for (int i = 0; i < count_; ++i) {
        if (hashes_[static_cast<size_t>(i)] == result.hash) {
            return StableReason::PeriodHash;
        }
    }

    hashes_[static_cast<size_t>(next_)] = result.hash;
    next_ = (next_ + 1) % kHistorySize;
    if (count_ < kHistorySize) {
        ++count_;
    }
    return StableReason::None;
}

const char* stable_reason_name(StableReason reason) {
    switch (reason) {
        case StableReason::Empty:
            return "empty";
        case StableReason::Unchanged:
            return "unchanged";
        case StableReason::PeriodHash:
            return "period_hash";
        case StableReason::None:
        default:
            return "none";
    }
}

}  // namespace life
