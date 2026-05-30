# Settings UI Plan

This document records the implemented general settings screen.

Status: implemented in firmware `0.8.4`. Hardware verification notes, when
available, are recorded in `docs/archive/PROJECT_LOG.md`.

Build purpose string:

```text
0.8.4-calendar-mode
```

## Entry Key

- `Shift + F2`, reported by the PicoCalc keyboard firmware as `F7`: general
  settings screen.

Only `KEY_STATE_PRESSED` triggers the screen transition. `KEY_STATE_HOLD` and
`KEY_STATE_RELEASED` are ignored.

## Settings

Implemented:

- `Seconds`: `ON` or `OFF`.
- `Style`: `DIGITAL`, `ANALOG`, or `CALENDAR`.
- `Life`: `ON` or `OFF`.

## Controls

| Key | Action |
| --- | --- |
| `Up` / `Down` | Move between setting rows |
| `Left` / `Right` / `Space` | Toggle the selected setting |
| `Enter` | Commit changed settings and return to the clock display |
| `Esc` | Discard edits and return to the clock display |

`Enter` writes EEPROM only when settings actually changed.

## Persistence

Since firmware `0.7.0`, the AT24C32 settings record is version 3 and stores
both alarm settings and app settings. Version 2 alarm-only records are still
accepted so existing alarm settings can be migrated. When a version 2 record is
loaded, the app settings use their defaults:

```text
Seconds: ON
Style: DIGITAL
Life: OFF
```

The next save writes a version 3 record containing both alarm settings and app
settings.

Current version 3 record layout:

```cpp
struct SettingsRecord {
    uint32_t magic;       // 0x4b4c4350, "PCLK"
    uint16_t version;     // 3
    uint16_t size;        // 64
    uint32_t sequence;
    uint8_t alarm_enabled[5];
    uint8_t alarm_hour[5];
    uint8_t alarm_minute[5];
    uint8_t app_flags;    // bit 0: show seconds, bit 1: hourly Life
    uint8_t clock_style;  // 0: digital, 1: analog, 2: calendar
    uint8_t reserved[31];
    uint32_t crc32;
};
```

The CRC32 covers every byte before the `crc32` field. The firmware accepts
version 2 and version 3 records when loading, but writes version 3 records.
For version 3 records, `clock_style` values other than `0`, `1`, or `2` make
the record invalid.

## Clock Display

When `Seconds` is `ON`, the digital clock shows:

```text
HH:MM:SS
```

When `Seconds` is `OFF`, the digital clock shows:

```text
HH:MM
```

The `HH:MM` display uses a larger 1.5x rendering of the native `S32x64` font,
so hiding seconds makes the main clock easier to read instead of leaving unused
space. In this mode, the colon blinks every 1 second.

When `Style` is `ANALOG`, the clock shows a circular analog face. `Seconds ON`
shows a second hand; `Seconds OFF` hides the second hand and updates the hands
only when the minute changes. The analog face also shows an `AM` or `PM` label
and the current moon age below the date. The moon age is right-aligned near the
weekday so it reads as part of the date group rather than part of the clock
face.
The RTC is still sampled by the normal clock loop.

When `Style` is `CALENDAR`, the top line shows the current year/month on the
left. Moon age is shown at the lower right of the calendar. The month calendar
is shown below it with grid lines and today's date highlighted in light cyan.
A digital clock is shown below the calendar; it follows the `Seconds` setting
and shows either `HH:MM:SS` or `HH:MM`. The next alarm summary is shown below
the time.

When `Life` is `ON`, the clock starts Conway Life at every exact hour. The
hourly run ends when Life stabilizes, after 1 minute, or when `Space` is
pressed. Pressing `L` on the clock display starts Life manually regardless of
the setting; manual runs end when Life stabilizes or `Space` is pressed.
