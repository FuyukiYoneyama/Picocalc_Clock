# Release Notes

## v0.8.20

Picocalc_Clock `0.8.20` continues the source organization work.

Highlights:

- Extracted UART prompt handling, `help` / `?`, and `set` command parsing into
  `src/app/uart_commands.*`.
- The main loop still decides when UART polling is allowed.

Verification build:

```text
version 0.8.20
purpose="0.8.20-refactor-uart-commands"
```

## v0.8.19

Picocalc_Clock `0.8.19` continues the source organization work.

Highlights:

- Extracted clock help page drawing and page count into
  `src/clock/clock_help.*`.
- Help key routing remains in `src/main.cpp`.

Verification build:

```text
version 0.8.19
purpose="0.8.19-refactor-clock-help"
```

## v0.8.18

Picocalc_Clock `0.8.18` continues the source organization work.

Highlights:

- Extracted screenshot capture sound orchestration into
  `src/app/screenshot_service.*`.
- Home key routing and alarm-pending sound suppression policy remain in
  `src/main.cpp`.

Verification build:

```text
version 0.8.18
purpose="0.8.18-refactor-screenshot-service"
```

## v0.8.17

Picocalc_Clock `0.8.17` continues the source organization work.

Highlights:

- Extracted Life runtime state, random initial pattern selection, board drawing,
  stepping, stop logging, and hourly-run tracking into `src/life/life_runtime.*`.
- Clock mode entry and return policy remains in `src/main.cpp`.

Verification build:

```text
version 0.8.17
purpose="0.8.17-refactor-life-runtime"
```

## v0.8.16

Picocalc_Clock `0.8.16` continues the source organization work.

Highlights:

- Extracted settings editor state, drawing, and key helpers into
  `src/app/settings_editor.*`.
- EEPROM save policy remains in `src/main.cpp`.

Verification build:

```text
version 0.8.16
purpose="0.8.16-refactor-settings-editor"
```

## v0.8.15

Picocalc_Clock `0.8.15` continues the source organization work.

Highlights:

- Extracted set-time editor state, drawing, and key helpers into
  `src/app/set_time_editor.*`.
- DS3231 write policy remains in `src/main.cpp`.

Verification build:

```text
version 0.8.15
purpose="0.8.15-refactor-set-time-editor"
```

## v0.8.14

Picocalc_Clock `0.8.14` continues the source organization work.

Highlights:

- Extracted common clock frame, digital time delta drawing, no-seconds colon
  drawing, battery text, and digital next-alarm drawing into
  `src/clock/clock_render.*`.
- Clock style selection remains in `src/main.cpp`.

Verification build:

```text
version 0.8.14
purpose="0.8.14-refactor-clock-render"
```

## v0.8.13

Picocalc_Clock `0.8.13` continues the source organization work.

Highlights:

- Extracted alarm editor state, alarm settings screen drawing, alarm editor key
  helpers, and alarm ringing screen drawing into `src/alarm/alarm_ui.*`.
- Alarm save policy and mode return remain in `src/main.cpp`.

Verification build:

```text
version 0.8.13
purpose="0.8.13-refactor-alarm-ui"
```

## v0.8.12

Picocalc_Clock `0.8.12` continues the source organization work.

Highlights:

- Extracted alarm matching, fired-minute tracking, next-alarm calculation, and
  alarm label formatting into `src/alarm/alarm_model.*`.
- Alarm UI drawing and alarm sound orchestration remain unchanged in
  `src/main.cpp`.

Verification build:

```text
version 0.8.12
purpose="0.8.12-refactor-alarm-model"
```

## v0.8.11

Picocalc_Clock `0.8.11` continues the source organization work.

Highlights:

- Extracted EEPROM-backed settings model and storage helpers into
  `src/settings/`.
- Added `src/alarm/alarm_model.h` for shared alarm settings types used by the
  settings store.
- EEPROM record layout and log strings are intentionally preserved.

Verification build:

```text
version 0.8.11
purpose="0.8.11-refactor-settings-store"
```

## v0.8.10

Picocalc_Clock `0.8.10` continues the source organization work.

Highlights:

- Extracted startup probe helpers into `src/platform/startup_probe.*`.
- Extracted battery register reading into `src/platform/battery.*`.
- Extracted backlight state transition helpers into
  `src/platform/backlight_control.*`.
- Key routing and mode policy remain in `src/main.cpp`.

Verification build:

```text
version 0.8.10
purpose="0.8.10-refactor-platform-services"
```

## v0.8.9

Picocalc_Clock `0.8.9` starts the source organization work.

Highlights:

- Extracted pure clock date/time helpers into `src/clock/clock_time.*`.
- No RTC I/O, display drawing, settings persistence, alarm policy, or Life
  behavior is intentionally changed in this phase.

Verification build:

```text
version 0.8.9
purpose="0.8.9-refactor-clock-time"
```

## v0.8.8

Picocalc_Clock `0.8.8` adds the source organization plan for the next
main.cpp refactor.

Highlights:

- Added `docs/MAIN_REFACTOR_PLAN.md`.
- The plan aligns clock-side module boundaries with Picocalc_ClockCalc where
  appropriate while preserving Clock-specific behavior.
- The plan defines build checkpoints, hardware-risk triggers, and a final
  log-driven smoke verification policy.

Verification build:

```text
version 0.8.8
purpose="0.8.8-main-refactor-plan"
```

## v0.8.7

Picocalc_Clock `0.8.7` updates the integrated Conway Life start patterns.

Highlights:

- Life starts from one of four randomly selected initial modes: full random,
  center burst, quad burst, or mirrored quadrants.
- Manual and hourly Life starts use the same random mode selection.
- The UART log prints the selected mode as `mode=full`, `mode=center`,
  `mode=quad`, or `mode=mirrored`.

Verification build:

```text
version 0.8.7
purpose="0.8.7-life-random-initial-modes"
```

## v0.8.6

Picocalc_Clock `0.8.6` refines the calendar weekday header colors.

Highlights:

- Calendar mode draws only the `Sun` weekday header in red.
- Sunday date numbers remain white unless highlighted as today.

Verification build:

```text
version 0.8.6
purpose="0.8.6-calendar-sunday-red"
```

## v0.8.5

Picocalc_Clock `0.8.5` adds a temporary calendar peek shortcut.

Highlights:

- Holding `C` on the normal clock display temporarily shows calendar mode.
- Releasing `C` returns to the configured display style without changing the
  saved setting.
- F10 help includes the `C` hold shortcut.

Verification build:

```text
version 0.8.5
purpose="0.8.5-calendar-peek"
```

## v0.8.4

Picocalc_Clock `0.8.4` adds a calendar clock face.

Highlights:

- The display style setting now cycles through `DIGITAL`, `ANALOG`, and
  `CALENDAR`.
- Calendar mode shows the current year and month at top left and moon age at
  the lower right of the calendar.
- The month calendar has grid lines and highlights today's date in light cyan.
- The current digital time follows the seconds ON/OFF setting and is shown
  below the calendar, with the next alarm summary below the time.

Verification build:

```text
version 0.8.4
purpose="0.8.4-calendar-mode"
```

## v0.8.3

Picocalc_Clock `0.8.3` adds an integrated Conway Life display mode.

![Conway Life running on PicoCalc](images/clock_life_v083.png)

Highlights:

- The clock can run a 160 x 160 toroidal Conway Life board on the full
  320 x 320 LCD.
- A new `Life ON/OFF` setting controls automatic starts at every exact hour.
- Automatic hourly Life returns to the clock when the board stabilizes, after
  1 minute, or when `Space` is pressed.
- Pressing `L` starts Life manually regardless of the setting. Manual Life
  returns to the clock when the board stabilizes or `Space` is pressed.
- Clock screenshot files use the `0:/screenshots/clk_####.BMP` filename
  pattern.

![Life setting screen](images/settings_life_v083.png)

Verification build:

```text
version 0.8.3
purpose="0.8.3-life-mode"
```

## v0.8.2

Picocalc_Clock `0.8.2` adds moon age display to the clock face.

Highlights:

- Digital mode shows the current moon age below the time.
- Analog mode shows moon age directly under the date, right-aligned near the
  weekday so date, weekday, and moon age read as one "today" group.
- The digital alarm summary was moved lower to avoid overlap with the moon age
  line.
- The analog clock face keeps the moon age outside the dial so it does not
  compete with the hands.

Verification build:

```text
version 0.8.2
purpose="0.8.2-moon-age"
```

## v0.8.1

Picocalc_Clock `0.8.1` backports the ClockCalc clock-side interaction updates.

Highlights:

- Clock display footer now shows `Clock v...` and `F10:Help`.
- F10 opens an on-device help screen with keytop-style labels and license notes.
- Home saves screenshots as `0:/screenshots/clk_####.BMP`.
- Power toggles the LCD backlight, and Space temporarily lights the display
  while the backlight is off.

Verification build:

```text
version 0.8.1
purpose="0.8.1-clockcalc-backport"
```

## v0.8.0

Picocalc_Clock `0.8.0` adds the analog clock display.

Highlights:

- Analog clock display mode.
- `DIGITAL` / `ANALOG` style switching from the `Shift + F2` / `F7` settings
  screen.
- EEPROM-backed resume of the selected display style.
- `AM` / `PM` label inside the analog face.
- Rebalanced analog layout with more even spacing between the date, clock face,
  and alarm summary.
- Partial analog hand updates to avoid full-screen redraws during normal time
  updates.
- Existing digital mode, alarms, EEPROM settings, UART setup commands, and
  on-device time setting remain available.

Build identity for release assets should show:

```text
version 0.8.0
purpose="0.8.0-analog-clock"
dirty=0
```
