# LCD Bring-up Log

This file records LCD bring-up attempts for `Picocalc_Clock`.
Do not repeat a failed experiment unless a new reason is recorded here.

Project-wide log:

- `PROJECT_LOG.md`

## Baseline Reference

Current display baseline:

- `<local-synth-workspace>/Picocalc_fonttest`
- `Picocalc_fonttest` uses `Picocalc_ment/src/platform/picocalc_display.cpp`.
- The normal visual path is:

```cpp
picoment::display::init();
picoment::keyboard::init();
picoment::display::draw_font_sample_screen(kBuildId);
```

Important rule:

- For visual bring-up, do not call LCD readback during startup.
- Do not switch the LCD bus to bitbang/SIO during startup visual testing.
- Do not add extra marker drawing unless there is a specific reason.

## Confirmed Facts

- `Picocalc_Clock` RTC probe has passed in UART logs.
- `Picocalc_Clock` EEPROM probe has passed in UART logs.
- `Picocalc_Clock` keyboard input test has passed in UART logs.
- LCD init logs reached `lcd_init: PASS`, but this only means init code ran.
- LCD visual output has been reported by the user as broken, including black screen, loader remnants, and sandstorm.
- `Picocalc_fonttest` has a screen capture implementation, but its normal startup display path does not call readback.

## Attempts

### 1. PIO RGB565 path copied from Picocalc_ment

Status: Kept.

What was done:

- Used PIO LCD path.
- Used RGB565 with `0x3A = 0x65`.
- Used the Picocalc_ment LCD init command sequence.

Result:

- LCD output was not correct in `Picocalc_Clock`.

Conclusion:

- The init command sequence alone is not the proven failure point.

### 2. Hardware SPI diagnostic path

Status: Removed from normal bring-up.

What was done:

- Added hardware SPI RGB888 / RGB565 diagnostic tests.

Result:

- It mixed another LCD path into the firmware and made diagnosis confusing.

Conclusion:

- Do not use this path for current bring-up.
- Normal bring-up should stay with the PIO RGB565 baseline.

### 3. Extra clear after display-on

Status: Reverted.

What was done:

- Added an extra `clear(0x0000)` and `set_window()` after display-on.

Result:

- This was not part of the `Picocalc_ment` / `Picocalc_NESco` reference path.

Conclusion:

- Do not re-add without a new reason.

### 4. Automatic keyboard test

Status: Removed.

What was done:

- Added automatic `key-test`.

Result:

- Keyboard was confirmed working, but the test made the bring-up UI confusing.

Conclusion:

- Keyboard test is complete.
- Do not include it in current LCD bring-up.

### 5. Startup LCD readback self-test

Status: Removed from visual bring-up.

What was done:

- Enabled `PICOMENT_SCREENSHOT_CAPTURE_BUILD=1`.
- Called `readback_probe_raw()` and `readback_rect_rgb565()` during startup LCD test.
- Compared clear and marker pixels against expected values.

Observed logs:

```text
lcd-test: clear_raw=PASS bytes=030305010303050103030501
lcd-test: clear_readback=PASS match=FAIL px=0303,0501,0303,0501 expected=0000,0000,0000,0000
lcd-test: marker_readback=PASS match=FAIL px=0101,0404,0505,0707 expected=F800,07E0,001F,FFFF
lcd-test: result=FAIL reason=readback_mismatch
```

Conclusion:

- Startup readback test failed.
- It also switches LCD bus pins between PIO and SIO/bitbang.
- Since `Picocalc_fonttest` does not do this in the startup display path, remove it from visual bring-up.
- Revisit readback only after the plain visual path works.

### 6. Match Picocalc_fonttest visual path

Status: Current approach.

What was done:

- Disabled startup readback.
- Removed extra clear before visual test.
- Removed corner markers.
- Removed `set_backlight()` before `display::init()`.
- Kept automatic startup test, but it now only calls `draw_font_sample_screen()`.

Current intended startup display sequence:

```cpp
picoment::display::init();
picoment::keyboard::init();
picoment::display::draw_font_sample_screen(PICOCALC_CLOCK_VERSION_STRING);
```

Current expected UART line:

```text
lcd-test: result=VISUAL_ONLY path=fonttest draw_font_sample_screen no_readback
```

### 7. Direct standalone fonttest-style main

Status: Current approach.

Why this exists:

- `pico20260516_072451.log` matched `3129f30 dirty=0`.
- The log showed the intended no-readback draw path ran.
- `<local-log-folder>/IMG_8465.JPG` still showed sandstorm/noise.
- `<local-log-folder>/IMG_8464.JPG` showed the standalone `Picocalc_fonttest` font sample screen.

Confirmed file comparison:

- Display source and PIO are not the current differentiator; their code differs only by comments.
- Font headers are identical.
- Remaining material differences are `main.cpp` and CMake/build setup.

Change:

- Use a direct `main.cpp` path:

```cpp
stdio_init_all();
sleep_ms(200);
picoment::display::init();
picoment::keyboard::init();
picoment::display::draw_font_sample_screen(PICOCALC_CLOCK_VERSION_STRING);
```

- Remove `ds3231.c` and `ui.cpp` from this LCD-only build.
- Add the same `-O2` compile option used by standalone `Picocalc_fonttest`.
- Keep no readback, no RTC boot check, no command UI.

Expected UART lines:

```text
BUILD ID git=<hash> dirty=0 time="<time>" purpose="lcd-fonttest-standalone-main-direct"
lcd-fonttest-direct: init display
lcd-fonttest-direct: init keyboard
lcd-fonttest-direct: draw_font_sample_screen
lcd-fonttest-direct: idle no_readback no_bootcheck no_commands
```

Result:

- `<local-log-folder>/pico20260516_074320.log` matched `ec267b6 dirty=0`.
- `<local-log-folder>/IMG_8466.JPG` showed a readable Spleen native font sample screen.

Conclusion:

- LCD visual output works when `Picocalc_Clock` uses the direct standalone fonttest-style startup path.
- Continue by adding back only one non-LCD layer at a time after the draw call.

### 8. Add RTC / EEPROM probes after working LCD draw

Status: Current approach.

Purpose:

- Keep the working direct LCD draw path unchanged.
- Add I2C diagnostics after the font sample is already visible.
- Detect whether post-draw RTC / EEPROM / keyboard probing breaks the LCD state.

Expected build purpose:

```text
lcd-direct-plus-rtc-bootcheck-after-draw
```

Expected result:

- UART prints `BOOT CHECK AFTER LCD DRAW`.
- LCD still shows the readable font sample screen.
- No readback and no command input.

Result:

- `<local-log-folder>/pico20260516_080333.log` matched `df142bf dirty=0`.
- RTC, EEPROM, and keyboard probe all printed PASS.

Conclusion:

- Post-draw I2C probing is not the current failure point in this build.
- Continue with a richer single-run diagnostic log instead of tiny one-feature increments.

### 9. Versioned sequenced diagnostics with visible build identity

Status: Current approach.

Purpose:

- Avoid ambiguous photos and logs.
- Put the test subversion and git hash on the LCD itself.
- Use UART `SEQ xx BEGIN/END` blocks so one hardware run can check multiple stages.

Version:

```text
0.3.5
```

Expected visible ID:

```text
Clock v0.3.5 git=<hash> d0
```

Expected final LCD status:

```text
v0.3.5 lcd-sequenced-diagnostics
RTC PASS EEPROM PASS KEY PASS
```

### 10. Init/test/integrated test after each subsystem

Status: Current approach.

Reason:

- A subsystem can pass its own local test and still break another subsystem.
- The diagnostic therefore redraws the LCD after each initializer and local test.

Version:

```text
0.3.6
```

Sequence:

```text
LCD init -> integrated LCD
Keyboard init -> keyboard probe -> integrated LCD
I2C/RTC init -> RTC read -> integrated LCD
EEPROM probe -> integrated LCD
All probes -> final integrated LCD
```

Expected visible stage labels:

```text
S02 LCD
S05 KEY
S08 RTC
S10 EEPROM
S12 FINAL
```

Result:

- `<local-log-folder>/pico20260516_091248.log` matched `72e3674 dirty=0`.
- UART showed `SEQ 01` through `SEQ 12` all reached PASS.
- `<local-log-folder>/IMG_8469.JPG` showed the final `S12 FINAL` screen.

Problem:

- The visual design reused the same font sample screen for each phase.
- A final photo therefore cannot prove that each previous phase redraw succeeded.

Conclusion:

- Replace repeated full-screen redraws with accumulating phase tiles.

### 11. Accumulating phase tiles

Status: Current approach.

Version:

```text
0.3.7
```

Rule:

- Each phase writes a different tile in a different color.
- The screen is cleared only once at the beginning.
- The final photo must show all six tiles.

Tiles:

```text
S02 LCD
S05 KEY
S08 RTC
S10 EEPROM
S12 FINAL
PHOTO ID
```

Result:

- `<local-log-folder>/pico20260516_092957.log` matched `acfadbe dirty=0`.
- UART printed all six `VISUAL TILE` writes.
- `<local-log-folder>/IMG_8470.JPG` showed all six tile regions.

Conclusion:

- LCD writes survive the current sequence of display init, keyboard init, I2C init, RTC read, EEPROM probe, and final combined probes.
- Stop iterating on LCD bring-up diagnostics for this path.
- Proceed to the actual clock display MVP while preserving the proven init order.

## Handoff to Clock MVP

LCD bring-up for the current init order is complete.

Next firmware version:

```text
0.4.0
```

Purpose:

- Display RTC date and time as the first usable clock screen.
- Keep visible version and git hash on the LCD.
- Keep startup probe summary on the LCD footer.

## Current State

Current commit:

```text
3129f30 Align LCD startup order with fonttest
```

Current test goal:

- Flash `build/Picocalc_Clock.uf2`.
- User captures UART log.
- User reports whether the font sample screen appears.
- No keyboard input.
- No command input.
- No startup readback.

## Do Not Repeat Without New Evidence

- Do not re-enable startup readback for visual bring-up.
- Do not re-add hardware SPI LCD diagnostics to the normal build.
- Do not re-add keyboard tests to the LCD bring-up flow.
- Do not add extra post-display-on clear unless tied to a specific reference.
- Do not judge visual success from `lcd_init: PASS`; it only means the init routine ran.
