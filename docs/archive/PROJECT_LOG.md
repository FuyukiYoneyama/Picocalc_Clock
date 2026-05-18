# Picocalc_Clock Project Log

This is the project-wide work log for `Picocalc_Clock`.
Every firmware handed to the user for real-hardware testing must be traceable from this file.

## Rules

- Record the purpose before a hardware test.
- Record the exact firmware build identity after building.
- Match every user-provided UART log with the firmware build identity printed in that log.
- Do not rely on `version 0.3` alone; it is not unique enough for debugging.
- Feature-specific notes may live in separate files, but this file must link to them.

## Linked Logs

- LCD bring-up details: `LCD_BRINGUP_LOG.md`

## Current Firmware Identification

The firmware now prints this line at startup:

```text
BUILD ID git=<hash> dirty=<0-or-1> time="<build time>" purpose="<purpose>"
```

Current build purpose string:

```text
0.7.1-large-no-seconds
```

Meaning:

- Display RTC date, weekday, time, battery, and next alarm information.
- Provide on-device Set Time and Set Alarm screens.
- Provide an on-device general settings screen.
- Render the `HH:MM` clock larger when seconds are hidden.
- Save five daily alarm settings and display settings to AT24C32 EEPROM and
  resume them on power-on.
- Ring a PWM alarm tone, stop with `Space`, and auto-stop after 60 seconds.
- Print build identity at startup so real-hardware logs can be matched to
  source.

## Hardware Test Entries

### 2026-05-16: Add Build ID and Project Log

Purpose:

- Make UART logs traceable to the exact firmware build.
- Stop debugging from ambiguous `version 0.3` logs.
- Move from LCD-only memory to a project-wide work log.

Code changes:

- Added generated build-info header.
- Startup log now prints git hash, dirty flag, build time, and build purpose.
- Added `PROJECT_LOG.md`.
- Added `<local-rtc-workspace>/AGENTS.md` project rules.

Expected startup lines:

```text
Picocalc_Clock version 0.3 build release
BUILD ID git=<hash> dirty=<0-or-1> time="<build time>" purpose="lcd-fonttest-visual-no-readback"
```

Expected LCD behavior:

- Startup automatically runs the visual LCD test.
- No command input is required.
- No readback is performed.
- The screen should show the Spleen font sample if the visual path works.

Do not repeat:

- Do not use readback mismatch as the first visual failure signal.
- Do not re-add startup readback before plain visual display works.

### 2026-05-16: Hardware Log `pico20260516_070949.log`

Firmware identity:

```text
BUILD ID git=c440292 dirty=0 time="2026-05-16 07:07:20 +0900" purpose="lcd-fonttest-visual-no-readback"
```

UART result:

```text
lcd-test: draw_font_sample_screen start
lcd-test: result=VISUAL_ONLY path=fonttest draw_font_sample_screen no_readback
AUTO CHECK COMPLETE no_user_command_required idle
```

User-provided image:

```text
<local-log-folder>/IMG_8460.JPG
```

Observed visual result:

- The LCD shows sandstorm/noise.
- The expected font sample screen is not visible.

Conclusion:

- Removing startup readback was not enough.
- The failure is now confirmed on the no-readback visual path.
- Continue by eliminating remaining differences from `<local-synth-workspace>/Picocalc_fonttest`.

Next step:

- Compare and align remaining startup/build differences:
  - CMake source/include strategy.
  - Direct use of `Picocalc_ment` display source versus local copied display source.
  - `main()` order and extra RTC/I2C boot checks before/after LCD display.
  - UART baud/build defines only after LCD path is identical.

### 2026-05-16: Align Startup Order with Fonttest

Reason:

- `pico20260516_070949.log` matched firmware `c440292 dirty=0`.
- `IMG_8460.JPG` showed sandstorm/noise, not the expected font sample.
- The firmware had no startup readback, so readback was not the cause of this run.

Observed remaining difference from `Picocalc_fonttest`:

- `Picocalc_fonttest` sequence:

```cpp
stdio_init_all();
sleep_ms(200);
picoment::display::init();
picoment::keyboard::init();
picoment::display::draw_font_sample_screen(kBuildId);
```

- `Picocalc_Clock` still performed RTC/I2C boot checks before drawing the LCD test.
- `Picocalc_Clock` also wrote keyboard backlight from `ui_init()`.

Change:

- Move RTC/I2C boot check after the LCD visual draw.
- Remove Clock-specific backlight write from `ui_init()`.
- Update build purpose to:

```text
lcd-fonttest-order-no-bootcheck-before-draw
```

Expected result:

- UART log should show the new build purpose.
- LCD visual draw happens before RTC / EEPROM / keyboard boot checks.
- No readback.

### 2026-05-16: Hardware Log `pico20260516_072451.log`

Firmware identity:

```text
BUILD ID git=3129f30 dirty=0 time="2026-05-16 07:21:39 +0900" purpose="lcd-fonttest-order-no-bootcheck-before-draw"
```

UART result:

```text
lcd-test: result=VISUAL_ONLY path=fonttest draw_font_sample_screen no_readback
AUTO CHECK COMPLETE no_user_command_required idle
```

User-provided images:

```text
<local-log-folder>/IMG_8464.JPG
<local-log-folder>/IMG_8465.JPG
```

Observed visual result:

- `IMG_8464.JPG` shows the standalone `Picocalc_fonttest` screen with readable font sample text.
- `IMG_8465.JPG` shows `Picocalc_Clock` with sandstorm/noise and no readable font sample text.

Confirmed file comparison against standalone `Picocalc_fonttest`:

- `platform/picocalc_display.cpp`, `platform/picocalc_display.h`, and `platform/lcd_spi_min.pio` differ only by comments.
- Font headers are identical.
- Keyboard files differ only by comments.
- `main.cpp` and `CMakeLists.txt` still differ materially.

Next change:

- Replace the LCD bring-up `main.cpp` with the standalone fonttest-style direct startup path.
- Remove RTC/UI wrapper sources from this LCD-only build.
- Add the same `-O2` compile option used by standalone `Picocalc_fonttest`.
- Keep UART at the user-selected 115200 baud.

### 2026-05-16: Hardware Log `pico20260516_074320.log`

Firmware identity:

```text
BUILD ID git=ec267b6 dirty=0 time="2026-05-16 07:40:45 +0900" purpose="lcd-fonttest-standalone-main-direct"
```

UART result:

```text
lcd-fonttest-direct: init display
lcd-fonttest-direct: init keyboard
lcd-fonttest-direct: draw_font_sample_screen
lcd-fonttest-direct: idle no_readback no_bootcheck no_commands
```

User-provided image:

```text
<local-log-folder>/IMG_8466.JPG
```

Observed visual result:

- The LCD shows the readable Spleen native font sample screen.
- The previous sandstorm/noise symptom is gone in this build.

Conclusion:

- The direct standalone fonttest-style startup path is confirmed working.
- The next step is to keep this LCD path and add back RTC / EEPROM BOOT CHECK after drawing, one layer at a time.

### 2026-05-16: Add BOOT CHECK After Working LCD Draw

Purpose:

- Preserve the confirmed working LCD startup path.
- Add back RTC, AT24C32 probe, and keyboard-controller probe only after `draw_font_sample_screen()` completes.
- Check whether post-draw I2C diagnostics disturb the already-working LCD display.

Expected build purpose:

```text
lcd-direct-plus-rtc-bootcheck-after-draw
```

Expected UART sequence:

```text
lcd-fonttest-direct: draw_font_sample_screen
lcd-fonttest-direct: draw complete
BOOT CHECK AFTER LCD DRAW
  rtc: PASS/FAIL ...
  eeprom_probe: PASS/FAIL ...
  keyboard_probe: PASS/FAIL ...
END BOOT CHECK AFTER LCD DRAW
lcd-fonttest-direct: idle no_readback no_commands
```

Expected LCD behavior:

- The font sample screen remains visible.
- No command input is required.

### 2026-05-16: Hardware Log `pico20260516_080333.log`

Firmware identity:

```text
BUILD ID git=df142bf dirty=0 time="2026-05-16 07:52:08 +0900" purpose="lcd-direct-plus-rtc-bootcheck-after-draw"
```

UART result:

```text
rtc: PASS addr=0x68 time=2026-05-16 08:03:36 osf=0
eeprom_probe: PASS addr=0x57
keyboard_probe: PASS addr=0x1F
```

Conclusion:

- Adding RTC / EEPROM / keyboard probes after the working LCD draw did not fail in the UART log.
- The next build must not advance in tiny single-function steps.
- Instead, it must use numbered sequence logs and put the version/build identity on the LCD image.

### 2026-05-16: Hardware Log `pico20260516_091248.log`

Firmware identity:

```text
BUILD ID git=72e3674 dirty=0 time="2026-05-16 08:24:02 +0900" purpose="0.3.6-init-test-integrated-after-each"
```

UART result:

- `SEQ 01` through `SEQ 12` all reached `result=PASS`.
- RTC, EEPROM, and keyboard probes printed PASS.

User-provided image:

```text
<local-log-folder>/IMG_8469.JPG
```

Observed visual result:

- The final screen shows `v0.3.6 72e3674 S12 FINAL`.
- The image does not prove that each earlier LCD redraw succeeded, because each phase reused the same font sample layout.

Conclusion:

- UART confirms that the code path reached all phases.
- The visual test design is insufficient for photo-based phase verification.
- The next build must use unique per-phase visual regions or colors that accumulate on the screen.

### 2026-05-16: Versioned Sequenced Diagnostics

Purpose:

- Start explicit LCD bring-up subversions at `0.3.5`.
- Make UART logs identify each diagnostic stage with `SEQ xx BEGIN/END`.
- Make LCD photos self-identifying by drawing `Clock v0.3.5 git=<hash> d<dirty>` on screen.
- Draw final visible status lines with RTC / EEPROM / KEY results.

Expected build purpose:

```text
0.3.5-lcd-sequenced-diagnostics
```

Expected UART stages:

```text
SEQ 01 BEGIN lcd-display-init
SEQ 01 END lcd-display-init result=PASS
SEQ 02 BEGIN keyboard-init
SEQ 02 END keyboard-init result=PASS
SEQ 03 BEGIN lcd-font-sample-draw
VISUAL ID Clock v0.3.5 git=<hash> d0 purpose="0.3.5-lcd-sequenced-diagnostics"
SEQ 03 END lcd-font-sample-draw result=PASS
SEQ 04 BEGIN post-draw-i2c-bootcheck
SEQ 04 END post-draw-i2c-bootcheck result=PASS/FAIL
SEQ 05 BEGIN lcd-visible-status-overlay
SEQ 05 END lcd-visible-status-overlay result=PASS
SEQ COMPLETE no_readback no_commands image_contains_version_and_git
```

### 2026-05-16: Init/Test/Integrated Diagnostic Loop

Purpose:

- Restore the diagnostic functions while keeping the working LCD path.
- Detect initialization conflicts between subsystems, not just single-function failures.
- Repeat this pattern in one run:

```text
init subsystem
local test for that subsystem
integrated LCD redraw/status test
```

Expected version:

```text
0.3.6
```

Expected build purpose:

```text
0.3.6-init-test-integrated-after-each
```

Expected UART sequence:

```text
SEQ 01 lcd-display-init
SEQ 02 integrated-lcd-after-display-init
SEQ 03 keyboard-init
SEQ 04 keyboard-local-test
SEQ 05 integrated-lcd-after-keyboard
SEQ 06 i2c-rtc-init
SEQ 07 rtc-local-test
SEQ 08 integrated-lcd-after-rtc
SEQ 09 eeprom-local-test
SEQ 10 integrated-lcd-after-eeprom
SEQ 11 final-all-local-tests
SEQ 12 final-integrated-lcd
```

Expected LCD behavior:

- The LCD image includes `v0.3.6`, git hash, and stage such as `S12 FINAL`.
- The final screen includes RTC / EEPROM / KEY PASS or FAIL.
- If one initializer conflicts with another subsystem, the first broken integrated redraw should identify the stage.

### 2026-05-16: Phase Tile Accumulation Visual Test

Purpose:

- Make the final photo prove which phase draws completed.
- Avoid redrawing the same image for every integrated LCD test.
- Give each phase a unique screen region, label, and background color.

Expected version:

```text
0.3.7
```

Expected build purpose:

```text
0.3.7-phase-tile-accumulation
```

Expected visible tiles:

```text
S02 LCD      blue
S05 KEY      green
S08 RTC      purple
S10 EEPROM   yellow/orange
S12 FINAL    cyan
PHOTO ID     gray
```

Expected conclusion rule:

- If all six tiles are visible, every phase-specific LCD write completed.
- If a tile is missing or stale, the first missing tile identifies the phase where LCD writes stopped working.

### 2026-05-16: Hardware Log `pico20260516_092957.log`

Firmware identity:

```text
BUILD ID git=acfadbe dirty=0 time="2026-05-16 09:27:35 +0900" purpose="0.3.7-phase-tile-accumulation"
```

UART result:

- `SEQ 01` through `SEQ 12` reached PASS.
- UART printed all six visual tile writes:
  - `S02 LCD`
  - `S05 KEY`
  - `S08 RTC`
  - `S10 EEPROM`
  - `S12 FINAL`
  - `PHOTO ID`

User-provided image:

```text
<local-log-folder>/IMG_8470.JPG
```

Observed visual result:

- The photo shows the version header `Clock v0.3.7 git=acfadbe`.
- The photo shows six distinct colored tile regions.
- Visible tile labels include `S02 LCD`, `S05 KEY`, `S08 RTC`, `S10 EEPROM`, `S12 FINAL`, and `PHOTO ID`.

Conclusion:

- The phase tile diagnostic proves that LCD writes continued after keyboard init, RTC/I2C init, RTC read, EEPROM probe, and the final combined probe.
- The current subsystem initialization order does not reproduce the earlier LCD failure.
- Continue to the clock MVP using the proven startup order:
  display init, keyboard init, I2C init, RTC/EEPROM probe, then clock display updates.

### 2026-05-16: Clock MVP RTC Display

Purpose:

- Move from LCD diagnostics to the first actual clock application screen.
- Preserve the proven startup order:
  display init, keyboard init, I2C init, RTC / EEPROM / keyboard probe, then display updates.
- Keep visible build identity on screen.

Expected version:

```text
0.4.0
```

Expected build purpose:

```text
0.4.0-clock-mvp-rtc-display
```

Expected LCD behavior:

- Header shows `Clock v0.4.0 git=<hash>`.
- Center shows RTC date and time.
- Seconds update once per second.
- Footer shows startup probe summary for RTC, EEPROM, and KEY.

Expected UART behavior:

- Startup prints BUILD ID.
- Startup prints `STARTUP PROBE ...`.
- Runtime prints `CLOCK TICK ...` at startup and then about once per minute.

### 2026-05-16: Hardware Log `pico20260516_094342.log`

Firmware identity:

```text
BUILD ID git=19888fa dirty=0 time="2026-05-16 09:40:47 +0900" purpose="0.4.0-clock-mvp-rtc-display"
```

UART result:

```text
STARTUP PROBE rtc=PASS eeprom=PASS keyboard=PASS rtc_status=0x08
CLOCK TICK rtc=PASS time=2026-05-16 09:43:46
```

User-provided image:

```text
<local-log-folder>/IMG_8471.JPG
```

Observed visual result:

- The LCD shows only the header and accent line.
- The clock body is not visible.

Confirmed code cause:

- `draw_text_large_band()` accepts `h <= 24`.
- `0.4.0` called it with `h=78` and `h=46`, so it returned without drawing the time body.
- `draw_text_band()` also accepts `h <= 18`; `0.4.0` used `h=20` and `h=24` in some places.

Conclusion:

- This result does not prove that RTC read destroys LCD drawing.
- `0.4.0` had invalid draw heights.
- `0.4.1` changes the clock body to use valid band heights and logs `CLOCK TICK` every second for faster verification.

### 2026-05-16: Clock MVP Text-Band Seconds

Purpose:

- Fix invalid draw heights from `0.4.0`.
- Show time with seconds as `HH:MM:SS`.
- Print one UART `CLOCK TICK` per second so verification does not require a one-minute wait.

Expected version:

```text
0.4.1
```

Expected build purpose:

```text
0.4.1-clock-mvp-text-band-seconds
```

### 2026-05-16: Source Review After Draw-Height Bug

Confirmed constraints from `platform/picocalc_display.cpp`:

- `draw_text_band()` returns without drawing if `h > 18`.
- `draw_text_large_band()` returns without drawing if `h > 24`.
- `draw_text_band()` uses Cozette width 6.
- `draw_text_large_band()` uses overlay width 8.

Reviewed `main.cpp` draw calls:

- `draw_text_band(..., h=18)` only.
- `draw_text_large_band(..., h=24)` only.
- All fixed rectangles are within the 320 x 320 screen.
- Current fixed text strings fit within their target widths:
  - Header fits in 300 px.
  - Probe line fits in 300 px.
  - Date line fits in 252 px.
  - `HH:MM:SS` fits in 252 px.
  - Status line fits in 252 px.

Additional issue found:

- On RTC read failure, the old code could pass a zero-initialized date into weekday calculation.
- `0.4.2` adds RTC date/time validation before drawing the normal clock.
- Invalid RTC data draws placeholder date/time and does not call weekday calculation with invalid month/day.

Expected version:

```text
0.4.2
```

Expected build purpose:

```text
0.4.2-clock-mvp-safe-rtc-display
```

### 2026-05-16: Hardware Log `pico20260516_100957.log`

Firmware identity:

```text
BUILD ID git=89c1989 dirty=0 time="2026-05-16 10:05:24 +0900" purpose="0.4.2-clock-mvp-safe-rtc-display"
```

UART result:

- Startup probe printed `rtc=PASS eeprom=PASS keyboard=PASS`.
- `CLOCK TICK rtc=PASS time=...` printed every second from `10:10:00` through at least `10:10:32`.

User-provided images:

```text
<local-log-folder>/IMG_8475.JPG
<local-log-folder>/IMG_8476.JPG
```

Observed visual result:

- The LCD shows `Clock v0.4.2 git=89c1989`.
- The LCD shows the RTC date `2026-05-16 Sat`.
- The LCD shows the time including seconds, for example `10:10:13` and `10:10:26`.
- The footer shows `RTC OK  EEPROM OK  KEY OK`.

Conclusion:

- The draw-height bug from `0.4.0` is fixed.
- The RTC read loop does not prevent the current clock screen from updating once per second.
- `0.4.2` satisfies the current clock MVP verification target.

### 2026-05-16: Reduce RTC Read Frequency

Reason:

- User reported that the screen updates every second, but UART sometimes shows `FAIL`.
- Confirmed code fact: `0.4.2` called `ds3231_read_time()` once per main loop.
- The main loop slept `50 ms`, so RTC read frequency was up to about 20 reads per second.

Rejected intermediate idea:

- Reading once per second is too sparse because it can drift against the RTC second boundary and makes skipped/delayed updates harder to distinguish.

Change:

- `0.4.4` reads RTC four times per second, every 250 ms.
- LCD update and UART `CLOCK TICK` happen only when the RTC second value changes.
- UART line includes `poll_hz=4` and the number of reads since the last displayed second.

Expected version:

```text
0.4.4
```

Expected build purpose:

```text
0.4.4-clock-mvp-rtc-read-4hz
```

### 2026-05-16: Smooth Clock Visual Update

Reason:

- User reported that 250 ms RTC polling still feels rough because the second change can appear to hop.
- User requested 100 ms or less, for example 73 ms.
- User requested white text, black background, no unnecessary frame, larger centered date/time, and partial redraw only for changed parts.

Change:

- `0.4.5` polls RTC every 73 ms.
- LCD redraw still happens only when RTC second changes.
- Date uses Spleen native `12x24`.
- Time uses Spleen native `32x64`.
- Time is displayed as `HH:MM:SS`.
- The screen is black with white text.
- The decorative clock panel/frame is removed.
- Time updates per changed character cell instead of redrawing the whole clock body.
- UART line includes `poll_ms=73` and `reads=N`.

Expected version:

```text
0.4.5
```

Expected build purpose:

```text
0.4.5-clock-mvp-73ms-delta-draw
```

### 2026-05-16: Clean Clock Display And Battery Header

Reason:

- User confirmed that `0.4.5` display is smooth and requested moving toward the release UI.
- User requested removing `RTC PASS poll 73msec` and `RTC OK EEPROM OK KEY OK` from LCD.
- User requested battery percentage on the right side of the first line.

Change:

- `0.4.6` keeps startup probe and clock status on UART only.
- LCD no longer draws the RTC poll/status line.
- LCD no longer draws the RTC/EEPROM/KEY probe line.
- Header keeps the build/version marker on the left and draws battery percentage on the right.
- Battery is read from the PicoCalc keyboard controller I2C battery register `0x0B`.
- Battery text uses lower 7 bits as percentage and bit 7 as charging marker, following the checked `Picocalc_NESco` and PicoCalc reference behavior.

Expected version:

```text
0.4.6
```

Expected build purpose:

```text
0.4.6-clock-clean-display-battery
```

### 2026-05-16: Source Tree Cleanup And UART Set Command

Reason:

- User requested cleaning up the scattered source layout.
- User requested moving documentation into a documentation folder.
- User requested stopping the once-per-second UART clock tick output.
- User requested UART `help` / `?` and `set` commands.

Change:

- `0.5.0` moves implementation files under `src/`.
- `0.5.0` moves planning, bring-up, project log, publication notes, license review, and third-party notices under `docs/`.
- `CMakeLists.txt` now builds from `src/main.cpp`, `src/rtc/`, and `src/platform/`.
- The clock no longer prints a UART line every second.
- Startup build ID, startup probe, and startup battery logs remain.
- `help` and `?` print available commands plus the current date, weekday, and time when RTC read succeeds.
- `set yyyy-mm-dd` updates only the date while preserving the current RTC time.
- `set HH:MM:SS` updates only the time while preserving the current RTC date.
- `set` automatically distinguishes date and time arguments by format.
- A successful set prints `SET OK` and the current date, weekday, and time.
- A failed set prints only `SET FAIL`.

Expected version:

```text
0.5.0
```

Expected build purpose:

```text
0.5.0-uart-set-command-clean-tree
```

### 2026-05-16: Battery Label

Reason:

- User reported that the header battery value was hard to understand when it only showed a number and `%`.

Change:

- `0.5.1` changes the LCD header battery text to `Bat. NN%`.
- Charging state remains visible as `Bat. NN%+`.

Expected version:

```text
0.5.1
```

Expected build purpose:

```text
0.5.1-battery-label
```

### 2026-05-16: UART Prompt And Echo

Reason:

- User reported that the formal UART command interface regressed from the earlier diagnostic command UI.
- User requested a prompt, startup help, input echo, prompt after empty Return, and better responsiveness.

Change:

- `0.5.2` adds the UART prompt `> `.
- Startup now prints the help text and current date/time before showing the prompt.
- Printable UART input is echoed.
- Backspace updates both the input buffer and terminal display.
- Return with no command prints a fresh prompt.
- Every command returns to the prompt.
- The main loop polls UART before RTC/display work on every iteration.

Expected version:

```text
0.5.2
```

Expected build purpose:

```text
0.5.2-uart-prompt-echo
```

### 2026-05-16: Battery Width Fix

Reason:

- User reported that the LCD header showed `Bat. 10` instead of the full battery percentage.

Confirmed cause:

- `draw_text_band()` advances by `picoment::font::kCozetteWidth`.
- `kCozetteWidth` is `6`.
- The previous battery text width calculation used `5`, so `Bat. 100%` was clipped.

Change:

- `0.5.3` calculates battery text width from `picoment::font::kCozetteWidth`.
- The version/build text band is limited to the left side so it does not overlap the battery band.
- The right battery band remains wide enough for `Bat. 100%+`.

Expected version:

```text
0.5.3
```

Expected build purpose:

```text
0.5.3-battery-width-fix
```

### 2026-05-17: Set Time UI Plan Review Fixes

Reason:

- User requested updates based on review findings before implementing the
  on-device time setting screen.

Change:

- `docs/SET_TIME_UI_PLAN.md` now states that the `F8` shortcut must either be
  handled as raw `picoment::keys::F8` or added explicitly to the application
  keymap.
- The first implementation now handles all screen-changing keys on
  `KEY_STATE_PRESSED` only and ignores `KEY_STATE_HOLD` / `KEY_STATE_RELEASED`.
- Keyboard event polling is now specified as independent from UART polling.
- SetTime exit now requires invalidating the clock delta draw cache and forcing
  a fresh RTC read before returning to normal clock drawing.
- Digit-entry validation now distinguishes temporary edit-buffer values from
  normalized committed date/time values.

### 2026-05-17: Set Time UI Plan Responsiveness Fixes

Reason:

- Review found that the time setting UI plan still allowed keyboard response to
  inherit the RTC `900ms` rest interval.
- Review also found ambiguity in SetTime RTC polling and digit-entry validation.

Change:

- `docs/SET_TIME_UI_PLAN.md` now requires keyboard/UI polling to cap sleep at
  `20ms` in the first implementation.
- SetTime mode now stops the normal RTC display polling cadence and reads RTC
  only on entry, `Enter` write/readback, and exit.
- Digit entry now uses prefix-valid rules: impossible prefixes are rejected, and
  possible prefixes are normalized to valid displayed values immediately.
- The out-of-range year example was replaced with valid/rejected examples.

### 2026-05-17: DS3231 Year Range Policy Check

Reason:

- User asked to confirm the year range against the RTC specification.

Confirmed source:

- Analog Devices DS3231 datasheet/product information states that the DS3231
  year register is `00` - `99`, the month register includes a century bit, and
  leap-year compensation is valid up to `2100`.
- Current `src/rtc/ds3231.c` accepts `2000` - `2099`, decodes register `0x06` as
  `2000 + year`, and writes `year - 2000` to register `0x06`.

Change:

- `docs/SET_TIME_UI_PLAN.md` now documents `2000` - `2099` as the first
  implementation policy, with DS3231 century-bit support out of scope.

### 2026-05-17: Set Time UI Lower-Year Editing Policy

Reason:

- User pointed out that DS3231 stores only a two-digit year, so the `20` prefix
  in `20YY` is decoration for the first implementation.

Change:

- `docs/SET_TIME_UI_PLAN.md` now states that the year is displayed as `20YY`,
  but only `YY` is editable and highlighted.
- The plan now states that an existing DS3231 century bit is ignored by the
  first implementation and cleared on save, matching the current driver policy.
- The plan now includes the Analog Devices DS3231 reference URL.

### 2026-05-17: Set Time UI Year Wording Cleanup

Reason:

- Review found remaining wording that could imply the fixed `20` prefix was
  editable.

Change:

- `docs/SET_TIME_UI_PLAN.md` now shows the date format as `20YY-MM-DD`.
- Digit input now says `first editable digit` instead of `first digit`.
- Year wrap examples now explicitly state `2099` Up wraps to `2000` and `2000`
  Down wraps to `2099`.

### 2026-05-17: Set Time UI Implementation

Reason:

- User ended planning and requested implementation of the on-device time setting
  screen.

Change:

- Firmware version is now `0.5.6`.
- `Shift + F3` / `F8` opens the Set Time UI from the clock display.
- The Set Time UI displays `20YY-MM-DD` and `HH:MM:SS`.
- Only the lower two year digits are editable/highlighted.
- Arrow keys move/edit fields, digit keys enter values, `Enter` writes to the
  DS3231, and `Esc` cancels.
- Set Time mode stops the normal RTC display polling cadence.
- Keyboard polling is independent of UART polling and caps sleep at `20ms`.
- Returning to the clock screen invalidates the delta draw cache and forces a
  fresh RTC read.

Build check:

- `cmake --build build` completed successfully before commit.

### 2026-05-17: Set Time UI Firmware Handoff

Purpose:

- Verify the first on-device Set Time UI implementation on real PicoCalc
  hardware.

Firmware:

- Version: `0.5.6`
- Build purpose: `0.5.6-set-time-ui`
- Expected UF2: `build/Picocalc_Clock.uf2`

Expected startup log:

```text
Picocalc_Clock version 0.5.6 build release
BUILD ID git=<current commit> dirty=0 ... purpose="0.5.6-set-time-ui"
```

Expected manual check:

- Clock screen still displays date, weekday, time, and battery.
- `Shift + F3` / `F8` opens the Set Time screen.
- Only the lower two year digits are highlighted/editable.
- Arrow keys and digit keys edit fields.
- `Esc` cancels and returns to the clock screen.
- `Enter` writes the displayed value to DS3231 and returns to the clock screen.
- Returning to the clock screen redraws the normal clock display.

### 2026-05-17: Documentation Update For Set Time UI

Reason:

- User reported the first Set Time UI implementation looked acceptable and asked
  to update README and documentation.

Change:

- `README.md` now documents the on-device time setting screen and controls.
- `docs/README.md` now describes `SET_TIME_UI_PLAN.md` as implemented behavior
  and design notes.
- `docs/SET_TIME_UI_PLAN.md` now says the feature is implemented in firmware
  `0.5.6`.

### 2026-05-18: Alarm UI Planning Document

Reason:

- User pointed out that alarm implementation must be documented before coding.
- User assigned `Shift + F1` / `F6` to alarms and `Shift + F2` / `F7` to
  general settings.
- User specified that the alarm feature must support five alarms.

Change:

- Added `docs/ALARM_UI_PLAN.md`.
- Documented five RAM-backed daily alarms, `F6` entry, `F7` reservation,
  five-row alarm editing, simultaneous alarm handling, minute-based fire
  suppression, required alarm sound, and one focused hardware verification pass.
- Added `docs/ALARM_UI_PLAN.md` to `docs/README.md`.

### 2026-05-18: Alarm UI Implementation

Purpose:

- Implement the first five-alarm PicoCalc UI according to
  `docs/ALARM_UI_PLAN.md`.

Code changes:

- Version moved to `0.6.0`.
- Build purpose moved to `0.6.0-alarm-ui`.
- Added five RAM-backed daily alarms with defaults:
  `A1 07:30`, `A2 08:00`, `A3 12:00`, `A4 18:00`, `A5 22:00`, all OFF.
- Added `Shift + F1` / `F6` alarm setting screen.
- Reserved `Shift + F2` / `F7` for later general settings.
- Added next-alarm display on the clock screen.
- Added alarm trigger detection using the existing RTC display sample.
- Added `AlarmRinging` mode, `Space` stop, and 60-second auto-stop.
- Added PWM alarm sound using the `Picocalc_ment` audio PWM path.
- Updated README and license review for copied audio PWM files.

Build check:

- `cmake --build build` completed successfully.

Expected manual check:

- Clock screen still displays date, weekday, time, and battery.
- Clock screen shows `Alm OFF` when all alarms are off.
- `Shift + F1` / `F6` opens the Set Alarm screen.
- All five alarm rows are visible.
- Arrow keys move through row, field, and digit selection.
- Digit keys edit hour and minute.
- `Esc` steps back through `Digit -> Field -> Row`, then discards edits.
- `Enter` saves the edited alarm list and returns to the clock screen.
- The clock screen shows the next enabled alarm.
- Setting an alarm for the next minute causes the alarm screen and sound.
- `Space` stops the alarm.
- If not stopped manually, the alarm auto-stops after 60 seconds.
- The same minute does not ring again after stopping.

### 2026-05-18: Alarm EEPROM Persistence

Purpose:

- Save the five alarm settings to the AT24C32 EEPROM.
- Resume saved alarm settings on power-on.

Code changes:

- Version moved to `0.6.1`.
- Build purpose moved to `0.6.1-alarm-eeprom`.
- Added a 64-byte CRC32-protected settings record for five alarms.
- Added AT24C32 slot A/B persistence at `0x0000` and `0x0040`.
- Added two 32-byte page writes per settings record, ACK polling, and readback
  verification.
- Startup now loads the newest valid EEPROM slot after the hardware probe.
- Alarm `Enter` now writes EEPROM only when the edited alarm list changed.
- EEPROM missing or invalid records fall back to alarm defaults.

Build check:

- `cmake --build build` completed successfully.

Expected manual check:

- On first boot with blank or invalid EEPROM records, firmware logs
  `SETTINGS eeprom load default ...` and uses the default OFF alarms.
- After editing alarms and pressing `Enter`, firmware logs
  `SETTINGS eeprom save ok slot=<A-or-B> seq=<n>`.
- After power-cycle or firmware restart, firmware logs
  `SETTINGS eeprom load ok slot=<A-or-B> seq=<n>` and resumes the edited alarm
  list.
- Pressing `Enter` without changing alarms logs
  `SETTINGS eeprom save skip reason=unchanged`.

### 2026-05-18: General Settings UI

Purpose:

- Add the `Shift + F2` / `F7` general settings screen.
- Allow the user to show or hide seconds on the main digital clock display.
- Show the digital/analog mode row while keeping analog mode not implemented.

Code changes:

- Version moved to `0.7.0`.
- Build purpose moved to `0.7.0-settings-ui`.
- Added `SetSettings` UI mode.
- Added app settings with default `Seconds=ON` and `Style=DIGITAL`.
- Extended the EEPROM record to version 3 while still accepting alarm-only
  version 2 records.
- The general settings screen saves EEPROM only when the edited settings
  actually changed.
- The clock display can now render either `HH:MM:SS` or `HH:MM`.

Build check:

- `cmake --build build` completed successfully.

Expected manual check:

- `Shift + F2` / `F7` opens the Settings screen.
- `Up` / `Down` moves between `Seconds` and `Style`.
- `Left` / `Right` / `Space` toggles only the `Seconds` row.
- `Style` remains `DIGITAL` and logs or displays that analog is later.
- `Enter` saves changed settings and returns to the clock display.
- `Esc` discards edits and returns to the clock display.
- With `Seconds=OFF`, the clock display shows `HH:MM`.
- After restart, the seconds display setting is resumed from EEPROM.

### 2026-05-18: Larger Clock Without Seconds

Purpose:

- Make the clock display larger when the user hides seconds.

Code changes:

- Version moved to `0.7.1`.
- Build purpose moved to `0.7.1-large-no-seconds`.
- Added a display helper for 1.5x native Spleen font rendering.
- When seconds are hidden, the main clock renders `HH:MM` as `48x96`
  characters centered on the screen.
- When seconds are shown, the main clock keeps the existing `HH:MM:SS`
  `32x64` rendering.

Build check:

- `cmake --build build` completed successfully.

Expected manual check:

- With `Seconds=ON`, the clock still shows `HH:MM:SS`.
- With `Seconds=OFF`, the clock shows larger centered `HH:MM`.
- Switching between ON and OFF clears the old time band cleanly.

## Previous Important Results

### Keyboard Test Passed

Source:

- User-provided UART logs showed Enter, Esc, arrows, digits, and Space being read.

Decision:

- Keyboard test is not part of the current LCD bring-up loop.

### LCD Startup Readback Failed

Source:

```text
lcd-test: clear_raw=PASS bytes=030305010303050103030501
lcd-test: clear_readback=PASS match=FAIL px=0303,0501,0303,0501 expected=0000,0000,0000,0000
lcd-test: marker_readback=PASS match=FAIL px=0101,0404,0505,0707 expected=F800,07E0,001F,FFFF
lcd-test: result=FAIL reason=readback_mismatch
```

Decision:

- Remove startup readback from visual bring-up.
- Keep readback investigation separate until the basic visual path works.
