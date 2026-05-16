/*
 * Picocalc_ment - standalone musical instrument firmware for PicoCalc.
 * Copyright (c) 2026 Fuyuki Yoneyama
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

namespace picoment::keys {

constexpr uint8_t None = 0x00;

constexpr uint8_t JoyUp = 0x01;
constexpr uint8_t JoyDown = 0x02;
constexpr uint8_t JoyLeft = 0x03;
constexpr uint8_t JoyRight = 0x04;
constexpr uint8_t JoyCenter = 0x05;
constexpr uint8_t ButtonLeft1 = 0x06;
constexpr uint8_t ButtonRight1 = 0x07;

constexpr uint8_t Backspace = 0x08;
constexpr uint8_t Tab = 0x09;
constexpr uint8_t Enter = 0x0a;
constexpr uint8_t ButtonLeft2 = 0x11;
constexpr uint8_t ButtonRight2 = 0x12;
constexpr uint8_t Space = 0x20;

constexpr uint8_t Alt = 0xa1;
constexpr uint8_t ShiftLeft = 0xa2;
constexpr uint8_t ShiftRight = 0xa3;
constexpr uint8_t Sym = 0xa4;
constexpr uint8_t Ctrl = 0xa5;

constexpr uint8_t F1 = 0x81;
constexpr uint8_t F2 = 0x82;
constexpr uint8_t F3 = 0x83;
constexpr uint8_t F4 = 0x84;
constexpr uint8_t F5 = 0x85;
constexpr uint8_t F6 = 0x86;
constexpr uint8_t F7 = 0x87;
constexpr uint8_t F8 = 0x88;
constexpr uint8_t F9 = 0x89;
constexpr uint8_t F10 = 0x90;

constexpr uint8_t Escape = 0xb1;
constexpr uint8_t Left = 0xb4;
constexpr uint8_t Up = 0xb5;
constexpr uint8_t Down = 0xb6;
constexpr uint8_t Right = 0xb7;

constexpr uint8_t CapsLock = 0xc1;

constexpr uint8_t Break = 0xd0;
constexpr uint8_t Insert = 0xd1;
constexpr uint8_t Home = 0xd2;
constexpr uint8_t Delete = 0xd4;
constexpr uint8_t End = 0xd5;
constexpr uint8_t PageUp = 0xd6;
constexpr uint8_t PageDown = 0xd7;

constexpr uint8_t Power = 0x91;

struct PerformanceKeyCell {
    uint8_t key;
    uint8_t canonical_key;
    char label;
    uint8_t row;
    uint8_t col;
    uint8_t base_note;
    bool enabled;
};

constexpr uint8_t kLayoutKalimba = 0;
constexpr uint8_t kLayoutPiano = 1;
constexpr uint8_t kLayoutKalimbaPentatonic = 2;
constexpr uint8_t kLayoutKalimbaArabic = 3;

inline const PerformanceKeyCell* performance_cells_for_layout(uint8_t layout, uint8_t* count) {
    static constexpr PerformanceKeyCell kKalimbaCells[] = {
        {'1', '1', '1', 0, 0, 86, true},   // D6
        {'2', '2', '2', 0, 1, 79, true},   // G5
        {'3', '3', '3', 0, 2, 72, true},   // C5
        {'4', '4', '4', 0, 3, 65, true},   // F4
        {'5', '5', '5', 0, 4, 60, true},   // C4
        {'6', '6', '6', 0, 5, 67, true},   // G4
        {'7', '7', '7', 0, 6, 74, true},   // D5
        {'8', '8', '8', 0, 7, 81, true},   // A5
        {'9', '9', '9', 0, 8, 88, true},   // E6
        {'0', '0', '0', 0, 9, 0, false},
        {'Q', 'Q', 'Q', 1, 0, 83, true},   // B5
        {'W', 'W', 'W', 1, 1, 76, true},   // E5
        {'E', 'E', 'E', 1, 2, 69, true},   // A4
        {'R', 'R', 'R', 1, 3, 62, true},   // D4
        {'T', 'T', 'T', 1, 4, 60, true},   // C4
        {'Y', 'Y', 'Y', 1, 5, 64, true},   // E4
        {'U', 'U', 'U', 1, 6, 71, true},   // B4
        {'I', 'I', 'I', 1, 7, 77, true},   // F5
        {'O', 'O', 'O', 1, 8, 84, true},   // C6
        {'P', 'P', 'P', 1, 9, 0, false},
    };
    static constexpr PerformanceKeyCell kKalimbaPentatonicCells[] = {
        {'1', '1', '1', 0, 0, 96, true},   // C7
        {'2', '2', '2', 0, 1, 86, true},   // D6
        {'3', '3', '3', 0, 2, 76, true},   // E5
        {'4', '4', '4', 0, 3, 67, true},   // G4
        {'5', '5', '5', 0, 4, 60, true},   // C4
        {'6', '6', '6', 0, 5, 69, true},   // A4
        {'7', '7', '7', 0, 6, 79, true},   // G5
        {'8', '8', '8', 0, 7, 88, true},   // E6
        {'9', '9', '9', 0, 8, 98, true},   // D7
        {'0', '0', '0', 0, 9, 0, false},
        {'Q', 'Q', 'Q', 1, 0, 91, true},   // G6
        {'W', 'W', 'W', 1, 1, 81, true},   // A5
        {'E', 'E', 'E', 1, 2, 72, true},   // C5
        {'R', 'R', 'R', 1, 3, 62, true},   // D4
        {'T', 'T', 'T', 1, 4, 60, true},   // C4
        {'Y', 'Y', 'Y', 1, 5, 64, true},   // E4
        {'U', 'U', 'U', 1, 6, 74, true},   // D5
        {'I', 'I', 'I', 1, 7, 84, true},   // C6
        {'O', 'O', 'O', 1, 8, 93, true},   // A6
        {'P', 'P', 'P', 1, 9, 0, false},
    };
    static constexpr PerformanceKeyCell kKalimbaArabicCells[] = {
        {'1', '1', '1', 0, 0, 85, true},   // C#6
        {'2', '2', '2', 0, 1, 79, true},   // G5
        {'3', '3', '3', 0, 2, 72, true},   // C5
        {'4', '4', '4', 0, 3, 65, true},   // F4
        {'5', '5', '5', 0, 4, 60, true},   // C4
        {'6', '6', '6', 0, 5, 67, true},   // G4
        {'7', '7', '7', 0, 6, 73, true},   // C#5
        {'8', '8', '8', 0, 7, 80, true},   // G#5
        {'9', '9', '9', 0, 8, 88, true},   // E6
        {'0', '0', '0', 0, 9, 0, false},
        {'Q', 'Q', 'Q', 1, 0, 83, true},   // B5
        {'W', 'W', 'W', 1, 1, 77, true},   // F5
        {'E', 'E', 'E', 1, 2, 68, true},   // G#4
        {'R', 'R', 'R', 1, 3, 61, true},   // C#4
        {'T', 'T', 'T', 1, 4, 60, true},   // C4
        {'Y', 'Y', 'Y', 1, 5, 64, true},   // E4
        {'U', 'U', 'U', 1, 6, 71, true},   // B4
        {'I', 'I', 'I', 1, 7, 76, true},   // E5
        {'O', 'O', 'O', 1, 8, 84, true},   // C6
        {'P', 'P', 'P', 1, 9, 0, false},
    };
    static constexpr PerformanceKeyCell kPianoCells[] = {
        {'1', '1', '1', 0, 0, 0, false},
        {'2', '2', '2', 0, 1, 0, false},
        {'3', '3', '3', 0, 2, 61, true},   // C#4
        {'4', '4', '4', 0, 3, 63, true},   // D#4
        {'5', '5', '5', 0, 4, 0, false},
        {'6', '6', '6', 0, 5, 66, true},   // F#4
        {'7', '7', '7', 0, 6, 68, true},   // G#4
        {'8', '8', '8', 0, 7, 70, true},   // A#4
        {'9', '9', '9', 0, 8, 0, false},
        {'0', '0', '0', 0, 9, 0, false},
        {'Q', 'Q', 'Q', 1, 0, 59, true},   // B3
        {'W', 'W', 'W', 1, 1, 60, true},   // C4
        {'E', 'E', 'E', 1, 2, 62, true},   // D4
        {'R', 'R', 'R', 1, 3, 64, true},   // E4
        {'T', 'T', 'T', 1, 4, 65, true},   // F4
        {'Y', 'Y', 'Y', 1, 5, 67, true},   // G4
        {'U', 'U', 'U', 1, 6, 69, true},   // A4
        {'I', 'I', 'I', 1, 7, 71, true},   // B4
        {'O', 'O', 'O', 1, 8, 72, true},   // C5
        {'P', 'P', 'P', 1, 9, 74, true},   // D5
    };

    const PerformanceKeyCell* cells = kKalimbaCells;
    uint8_t cell_count = static_cast<uint8_t>(sizeof(kKalimbaCells) / sizeof(kKalimbaCells[0]));
    if (layout == kLayoutPiano) {
        cells = kPianoCells;
        cell_count = static_cast<uint8_t>(sizeof(kPianoCells) / sizeof(kPianoCells[0]));
    } else if (layout == kLayoutKalimbaPentatonic) {
        cells = kKalimbaPentatonicCells;
        cell_count = static_cast<uint8_t>(sizeof(kKalimbaPentatonicCells) / sizeof(kKalimbaPentatonicCells[0]));
    } else if (layout == kLayoutKalimbaArabic) {
        cells = kKalimbaArabicCells;
        cell_count = static_cast<uint8_t>(sizeof(kKalimbaArabicCells) / sizeof(kKalimbaArabicCells[0]));
    }

    if (count != nullptr) {
        *count = cell_count;
    }
    return cells;
}

inline const PerformanceKeyCell* performance_cells(uint8_t* count) {
    return performance_cells_for_layout(kLayoutKalimba, count);
}

inline uint8_t uppercase_ascii(uint8_t key) {
    if (key >= 'a' && key <= 'z') {
        return static_cast<uint8_t>(key - ('a' - 'A'));
    }
    return key;
}

inline bool performance_cell_for_key(uint8_t layout, uint8_t key, PerformanceKeyCell* out) {
    uint8_t count = 0;
    const PerformanceKeyCell* cells = performance_cells_for_layout(layout, &count);
    const uint8_t canonical = uppercase_ascii(key);
    for (uint8_t i = 0; i < count; ++i) {
        if (cells[i].canonical_key == canonical) {
            if (out != nullptr) {
                *out = cells[i];
            }
            return true;
        }
    }
    return false;
}

inline bool performance_cell_for_key(uint8_t key, PerformanceKeyCell* out) {
    return performance_cell_for_key(kLayoutKalimba, key, out);
}

inline bool canonical_performance_key(uint8_t layout, uint8_t key, uint8_t* canonical_key) {
    PerformanceKeyCell cell{};
    if (!performance_cell_for_key(layout, key, &cell)) {
        return false;
    }
    if (canonical_key != nullptr) {
        *canonical_key = cell.canonical_key;
    }
    return true;
}

inline bool canonical_performance_key(uint8_t key, uint8_t* canonical_key) {
    return canonical_performance_key(kLayoutKalimba, key, canonical_key);
}

inline bool performance_note_for_key(uint8_t layout, uint8_t key, uint8_t* base_note) {
    if (base_note == nullptr) {
        return false;
    }

    PerformanceKeyCell cell{};
    if (!performance_cell_for_key(layout, key, &cell) || !cell.enabled) {
        return false;
    }
    *base_note = cell.base_note;
    return true;
}

inline bool performance_note_for_key(uint8_t key, uint8_t* base_note) {
    return performance_note_for_key(kLayoutKalimba, key, base_note);
}

inline bool note_name_for_key(
    uint8_t layout, uint8_t key, int8_t octave_transpose, char* out, size_t out_size) {
    if (out == nullptr || out_size == 0) {
        return false;
    }

    PerformanceKeyCell cell{};
    if (!performance_cell_for_key(layout, key, &cell) || !cell.enabled) {
        snprintf(out, out_size, "--");
        return false;
    }

    static constexpr const char* kNames[] = {
        "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B",
    };
    const int midi_note = static_cast<int>(cell.base_note) + static_cast<int>(octave_transpose);
    if (midi_note < 0 || midi_note > 127) {
        snprintf(out, out_size, "--");
        return false;
    }

    const int octave = (midi_note / 12) - 1;
    snprintf(out, out_size, "%s%d", kNames[midi_note % 12], octave);
    return true;
}

inline bool note_name_for_key(uint8_t key, int8_t octave_transpose, char* out, size_t out_size) {
    return note_name_for_key(kLayoutKalimba, key, octave_transpose, out, out_size);
}

inline bool preset_index_for_key(uint8_t key, uint8_t* preset) {
    if (preset == nullptr) {
        return false;
    }

    switch (key) {
        case 'a': case 'A': *preset = 0; return true;
        case 's': case 'S': *preset = 1; return true;
        case 'd': case 'D': *preset = 2; return true;
        case 'f': case 'F': *preset = 3; return true;
        default: return false;
    }
}

inline const char* name(uint8_t key) {
    switch (key) {
        case None: return "NONE";
        case JoyUp: return "JOY_UP";
        case JoyDown: return "JOY_DOWN";
        case JoyLeft: return "JOY_LEFT";
        case JoyRight: return "JOY_RIGHT";
        case JoyCenter: return "JOY_CENTER";
        case ButtonLeft1: return "BTN_L1";
        case ButtonRight1: return "BTN_R1";
        case Backspace: return "BACK";
        case Tab: return "TAB";
        case Enter: return "ENTER";
        case ButtonLeft2: return "BTN_L2";
        case ButtonRight2: return "BTN_R2";
        case Space: return "SPACE";
        case Alt: return "ALT";
        case ShiftLeft: return "SHIFT_L";
        case ShiftRight: return "SHIFT_R";
        case Sym: return "SYM";
        case Ctrl: return "CTRL";
        case F1: return "F1";
        case F2: return "F2";
        case F3: return "F3";
        case F4: return "F4";
        case F5: return "F5";
        case F6: return "F6";
        case F7: return "F7";
        case F8: return "F8";
        case F9: return "F9";
        case F10: return "F10";
        case Escape: return "ESC";
        case Left: return "LEFT";
        case Up: return "UP";
        case Down: return "DOWN";
        case Right: return "RIGHT";
        case CapsLock: return "CAPS";
        case Break: return "BREAK";
        case Insert: return "INS";
        case Home: return "HOME";
        case Delete: return "DEL";
        case End: return "END";
        case PageUp: return "PAGE_UP";
        case PageDown: return "PAGE_DOWN";
        case Power: return "POWER";
        default: break;
    }

    if (key >= 0x20 && key <= 0x7e) {
        return "ASCII";
    }
    return "UNKNOWN";
}

}  // namespace picoment::keys
