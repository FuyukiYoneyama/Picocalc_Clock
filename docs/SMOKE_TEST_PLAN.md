# Picocalc_Clock Refactor Smoke Test Plan

This smoke test verifies the post-refactor firmware with one hardware run.
Serial log lines are the primary evidence. Visual checks are limited to the
clock faces whose rendering code was moved.

## Firmware Under Test

- UF2: `build/Picocalc_Clock.uf2`
- Version: `0.8.24`
- Build purpose: `0.8.24-uart-poll-on-charging`
- Build identity source: `build/generated/picocalc_clock_build_info.h`

Expected boot identity:

```text
Picocalc_Clock version 0.8.24 build release
BUILD ID git=<final clean commit> dirty=0 time="<clean build time>" purpose="0.8.24-uart-poll-on-charging"
```

## Pass/Fail Rules

- Pass if all required log lines appear in one serial capture and the visual
  checks do not show an obviously broken digital, analog, or calendar face.
- Do not repeat steps that already passed in the same capture.
- If a failure appears, keep the log file and resume only from the failed area.
- Alarm firing may be run separately only if a near-future alarm setup is
  needed; do not rerun unrelated display and screenshot checks for it.

## Required Log Checks

1. Boot and hardware probe:

```text
STARTUP PROBE rtc=PASS eeprom=PASS keyboard=PASS
STARTUP BATTERY PASS
STARTUP VBUS gpio=24 present=
UART poll=on
SETTINGS eeprom load
```

2. UART prompt and current time:

```text
Commands:
Current:
```

3. Clock help route:

```text
UI mode=clock-help
UI mode=clock
```

4. Settings routes:

```text
UI mode=set-alarm
ALARM edit cancel
UI mode=settings
SETTINGS cancel
UI mode=set-time
SETTIME cancel
```

5. Life route:

```text
UI mode=life source=manual
LIFE start source=manual mode=
LIFE stop reason=space
UI mode=clock
```

6. Screenshot route:

```text
SCREENSHOT screenshot begin path=0:/screenshots/clk_
SCREENSHOT screenshot done status=ok path=0:/screenshots/clk_
```

7. Backlight route:

```text
BACKLIGHT user=off
BACKLIGHT peek=on
BACKLIGHT peek=off
BACKLIGHT user=on
```

## Visual Checks

- Digital clock: date, time, moon age, battery, and next alarm are visible.
- Analog clock: date, moon age, hands, battery, and next alarm are visible.
- Calendar clock: month grid, highlighted today, time, moon age, and next alarm
  are visible.
- Calendar peek from clock mode with `C` shows the calendar while held and
  returns to the configured clock face when released.

## Suggested Key Sequence

1. Boot and capture the startup log.
2. `F10`, then `F10` or `Esc`.
3. `F6`, then `Esc`.
4. `F7`, then `Esc`.
5. `F8`, then `Esc`.
6. `L`, wait for `LIFE start`, then `Space`.
7. `Home`.
8. `Power`, hold/release `Space`, then `Power`.
9. Use `F7` only if needed to cycle through digital, analog, and calendar
   styles for the visual checks.
