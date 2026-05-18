# Settings UI Plan

This document records the implemented general settings screen.

Status: implemented in firmware `0.7.1`; hardware verification is still
required.

Build purpose string:

```text
0.7.1-large-no-seconds
```

## Entry Key

- `Shift + F2`, reported by the PicoCalc keyboard firmware as `F7`: general
  settings screen.

Only `KEY_STATE_PRESSED` triggers the screen transition. `KEY_STATE_HOLD` and
`KEY_STATE_RELEASED` are ignored.

## Settings

Implemented:

- `Seconds`: `ON` or `OFF`.

Shown but not editable yet:

- `Style`: fixed to `DIGITAL`. Analog display is planned but not implemented.

## Controls

| Key | Action |
| --- | --- |
| `Up` / `Down` | Move between setting rows |
| `Left` / `Right` / `Space` | Toggle `Seconds` on the seconds row |
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
```

The next save writes a version 3 record containing both alarm settings and app
settings.

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
space.

The RTC is still sampled by the normal clock loop. Hiding seconds affects only
the displayed text.
