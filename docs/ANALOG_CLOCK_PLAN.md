# Analog Clock Display Plan

This document defines the plan for adding an analog clock display mode to
Picocalc_Clock.

Status: implemented in firmware `0.8.0`; hardware verification is pending.

## Goals

- Add an analog clock display as a selectable clock style.
- Keep the current digital display unchanged.
- Use the existing `Shift + F2` / `F7` general settings screen to switch
  between `DIGITAL` and `ANALOG`.
- Store the selected style in the existing AT24C32 EEPROM settings record.
- Avoid adding new UART commands unless they are needed for debugging.
- Keep RTC polling behavior unchanged; analog drawing must use the RTC samples
  already read by the clock loop.

## Non-Goals

- Do not add multiple analog themes in the first implementation.
- Do not add smooth sub-second hand animation.
- Do not change the alarm UI, time setting UI, or EEPROM slot layout.
- Do not depend on LCD readback for verification.

## Pre-Implementation Facts

- Firmware `0.7.2` already has a version 3 EEPROM settings record.
- The record already contains `clock_style`.
- Firmware `0.7.2` accepts only `clock_style == 0`.
- The settings screen already shows `Style DIGITAL`, but the row is not
  editable.
- The public display API currently exposes `fill_rect()` and `draw_frame()`.
  Generic line and circle primitives are not public yet.

## Style Values

Use explicit style constants:

```cpp
enum class ClockStyle : uint8_t {
    Digital = 0,
    Analog = 1,
};
```

The EEPROM record layout remains version 3:

```cpp
uint8_t clock_style;  // 0: digital, 1: analog
```

No record size or slot address change is required. Firmware should continue to
accept version 2 alarm-only records, defaulting the style to `DIGITAL`.

Validation rule for version 3 records:

- `0`: accepted as digital.
- `1`: accepted as analog.
- Other values: mark the entire settings record invalid. This matches the
  current record validation style; do not silently coerce only `clock_style` to
  a default while accepting the rest of the record.

## Settings UI

The general settings screen becomes:

```text
Seconds  ON
Style    DIGITAL
```

or:

```text
Seconds  OFF
Style    ANALOG
```

Controls:

| Key | Action |
| --- | --- |
| `Up` / `Down` | Move between settings rows |
| `Left` / `Right` / `Space` on `Seconds` | Toggle seconds on/off |
| `Left` / `Right` / `Space` on `Style` | Toggle digital/analog |
| `Enter` | Commit changed settings and return to the clock display |
| `Esc` | Discard edits and return to the clock display |

Only `KEY_STATE_PRESSED` should be handled. Ignore `HOLD` and `RELEASED` for
the initial implementation.

When settings are committed, invalidate the clock drawing cache and force a
full clock screen redraw. This prevents the new analog or digital screen from
being skipped by the existing delta drawing logic.

## Seconds Setting In Analog Mode

The existing `Seconds` setting applies to both styles:

- `Seconds ON`: analog display draws hour, minute, and second hands.
- `Seconds OFF`: analog display draws only hour and minute hands.

This keeps the setting simple and avoids introducing an analog-specific option.

## Display Layout

The analog display should use the same overall screen responsibilities as the
digital display:

- Header: build/app label on the left and battery on the right.
- Main area: analog clock face.
- Lower area: alarm summary or alarm active state.

Initial geometry for the 320x320 LCD:

```text
Screen: 320 x 320
Face center: x=160, y=160
Face radius: 96
Face clear rect: x=52, y=62, w=216, h=200
Date text band: x=centered, y=38, h=24, Spleen S12x24
Alarm summary band: x=40, y=274, w=240, h=24, Spleen S12x24
```

Keep these as named constants. If implementation proves that overlap or
cropping occurs, adjust only the constants and record the reason in
`docs/archive/PROJECT_LOG.md`.

## Display Primitives

Add minimal public drawing primitives to `picoment::display`:

```cpp
void draw_line(int x0, int y0, int x1, int y1, uint16_t color);
void draw_circle(int cx, int cy, int radius, uint16_t color);
void fill_circle(int cx, int cy, int radius, uint16_t color);
```

Implementation rules:

- Use integer algorithms such as Bresenham line and midpoint circle.
- Keep clipping inside the primitive implementation.
- Use RGB565 colors only.
- Do not switch the LCD bus to readback/SIO mode.
- Clipping rule: drawing a point outside `0 <= x < 320` or `0 <= y < 320`
  must be skipped. Do not call `fill_rect()` with negative width, negative
  height, or coordinates that depend on unsigned underflow.
- `draw_line()` draws single-pixel-thick lines.
- `draw_line()` includes both endpoints.
- `draw_circle()` draws only the outline.
- `fill_circle()` fills the center disk and should be used only for small
  elements such as the hand hub in the first implementation.

First implementation uses single-pixel hands for all three hands. Do not add
thick-hand logic until the basic analog display has passed hardware
verification.

## Hand Calculation

Use a static 60-entry signed fixed-point lookup table. Do not use runtime float
math in the firmware.

Use this table style:

- 60 positions around the face.
- `0` points to 12 o'clock.
- Positive direction moves clockwise.
- Use signed fixed-point values scaled by `1024`.
- Store the table as `static constexpr int16_t kSin60[60]` and
  `static constexpr int16_t kCos60[60]` in `src/main.cpp`.

Coordinate conversion:

```cpp
x = cx + kSin60[index] * length / 1024;
y = cy - kCos60[index] * length / 1024;
```

This keeps `0` at 12 o'clock and avoids float math in the firmware.

Hand positions:

```text
second_index = second
minute_index = minute
hour_index   = (hour % 12) * 5 + (minute / 12)
```

Hand lengths:

```text
hour:   radius * 50 / 100
minute: radius * 72 / 100
second: radius * 82 / 100
```

Colors for the first implementation:

```text
face/ticks: 0x7bef
hour/minute: 0xffff
second: 0xf800
hub: 0xffff
background: black
```

## RTC Failure Behavior

Analog mode must handle RTC read failure without drawing stale hands as if the
time were valid.

When RTC is not available:

1. Draw the analog frame and static face.
2. Do not draw hour, minute, or second hands.
3. Draw a small status label such as `RTC --` in the lower area.
4. Keep battery display active if battery status can still be read.
5. Clear `previous_hand_state` so the next valid RTC sample redraws hands.

The existing RTC retry cadence remains unchanged.

## Drawing Strategy

The first implementation must avoid full-screen or full-face flashing during
normal hand updates. The analog renderer uses two drawing levels:

### Full Analog Redraw

Run only when entering analog mode or when the screen was overwritten by another
UI:

1. Clear the full screen.
2. Draw the header and battery band.
3. Draw the date band.
4. Draw the static clock face.
5. Draw 12 hour tick marks.
6. Draw the alarm summary band.
7. Clear `previous_hand_state`.
8. If RTC is valid, draw current hands and the center hub.
9. If RTC is not valid, draw the RTC failure state instead of hands.

### Hand-Only Update

Run for normal second/minute changes:

1. Erase previous hands by redrawing their exact line segments in black.
2. Restore only the static face details that may have been covered by the erased
   hands.
3. Draw new hour, minute, and optional second hands.
4. Draw the center hub last.

Do not clear the full screen, the full analog face, or the date/alarm/header
bands during a hand-only update.

The second hand must never cause the whole analog face to flash. If the
hand-only update has visual artifacts, fix the restore logic; do not fall back
to full-face redraw once per second.

Hand-only update order is fixed:

```text
erase previous hour hand in black
erase previous minute hand in black
erase previous second hand in black, only if it was visible
restore static face details
draw new hour hand
draw new minute hand
draw new second hand, only if visible
draw center hub
store new AnalogHandState as previous_hand_state
```

Always redraw all currently visible hands after erasing. Do not attempt to
update only the hand that changed; overlapping hands make that optimization
error-prone.

With `Seconds ON`, the analog hand state changes every second.

With `Seconds OFF`, the analog hand state changes only when the minute changes.
No blinking colon is drawn in analog mode.

The existing digital colon blink timer and sleep adjustment must be guarded by
both conditions:

```cpp
settings.clock_style == ClockStyle::Digital && !settings.show_seconds
```

Analog mode must not schedule or draw the digital colon blink.

Static face restoration after erasing hands:

- Redraw the outer circle.
- Redraw the 12 hour tick marks.
- Redraw the center hub after drawing the new hands.

This may redraw more static details than strictly necessary, but it keeps the
changed area small and avoids the visible flash of clearing the entire face.

## Main Loop Integration

The RTC polling cadence remains shared:

- Read RTC at the existing 47ms search cadence until the second changes.
- Rest for the existing post-change interval.
- Use the latest RTC value for either digital or analog drawing.

The analog feature must not increase RTC read frequency.

The existing `have_rtc_sample` and second-change detection remain the source of
truth. Analog rendering must not add a second RTC read path.

Clock drawing dispatch should be split by style:

```cpp
if (settings.clock_style == Digital) {
    draw_digital_clock(...);
} else {
    draw_analog_clock(...);
}
```

State that must force a full redraw:

- Style changed.
- Returning from settings UI.
- Returning from time setting UI.
- Returning from alarm UI.
- Alarm ringing screen stopped or timed out.
- LCD was cleared or reinitialized.

Cache invalidation requirements:

- `previous_style`: set to an invalid value whenever a full redraw is required.
- `previous_date`: clear when switching style or when the date band was
  overwritten by another UI.
- `previous_time`: clear when switching style or when returning to digital
  display.
- `previous_battery`: clear when the header was redrawn.
- `previous_alarm`: clear when the alarm summary area was redrawn.
- `previous_hand_state`: clear when entering analog mode, leaving another UI,
  changing the seconds setting, or changing from digital to analog.

Implement a single `force_clock_redraw()` helper that invalidates all caches
that can hide a required redraw.

Analog hand cache:

```cpp
struct AnalogHandState {
    bool valid;
    bool rtc_ok;
    bool show_second;
    uint8_t hour_index;
    uint8_t minute_index;
    uint8_t second_index;
};
```

When `show_second == false`, ignore `second_index` during equality comparison.
When `rtc_ok == false`, the hand state is invalid and the renderer draws the
RTC failure state instead of hands.

Helper functions to implement in `src/main.cpp`:

```cpp
AnalogHandState make_analog_hand_state(const ds3231_datetime_t& dt,
                                       bool rtc_ok,
                                       bool show_seconds);
bool analog_hand_state_equal(const AnalogHandState& a,
                             const AnalogHandState& b);
void draw_analog_static_face();
void restore_analog_static_face_details();
void draw_analog_hands(const AnalogHandState& state, bool erase);
void draw_analog_clock(const ds3231_datetime_t& dt,
                       bool rtc_ok,
                       const BatteryStatus& battery,
                       const AlarmSettings* alarms,
                       const AlarmFireRecord& last_fire,
                       bool force_full_redraw);
```

`draw_analog_hands(state, true)` draws the hand geometry in black using exactly
the same endpoints and thickness as `draw_analog_hands(state, false)`.

## File-Level Implementation Targets

First implementation should keep the change set small and avoid CMake churn.

Modify these files:

- `src/main.cpp`
  - Add `ClockStyle` constants or enum.
  - Update settings validation to accept `Digital` and `Analog`.
  - Update settings UI row text and toggle behavior.
  - Add analog drawing state such as `previous_style` and
    `previous_hand_state`.
  - Add `force_clock_redraw()`.
  - Add analog clock drawing helpers near the existing digital clock helpers.
  - Dispatch clock drawing by style in the main loop.
  - Guard colon blink drawing and sleep adjustment with
    `Digital && Seconds OFF`.
- `src/platform/picocalc_display.h`
  - Declare `draw_line()`, `draw_circle()`, and `fill_circle()`.
- `src/platform/picocalc_display.cpp`
  - Implement the new primitives using the existing LCD write path only.
- `README.md`
  - Move analog display from planned to implemented after hardware validation.
- `docs/SETTINGS_UI_PLAN.md`
  - Update `Style` from display-only to editable after implementation.
- `docs/archive/PROJECT_LOG.md`
  - Record the implementation and hardware handoff.

Do not add new source files in the first implementation unless `main.cpp`
becomes unreasonably hard to review. If a later refactor extracts
`analog_clock.cpp`, update `CMakeLists.txt` in that same refactor.

## Alarm Integration

Alarm matching, sound, stop, and auto-stop behavior do not change.

Analog mode still shows:

- next alarm summary when idle;
- alarm active status when ringing.

If the current digital alarm band coordinates overlap the analog face, add a
separate analog alarm summary position instead of moving the digital display.

## Versioning

This is a user-visible display feature. Recommended version:

```text
0.8.0-analog-clock
```

Update:

- `src/version.h`
- CMake build purpose string
- README feature list
- settings documentation
- project log

## Implementation Phases

### Phase 1: Settings Style Enablement

- Add `ClockStyle` constants.
- Accept `clock_style == 1` in settings validation.
- Make the `Style` row editable in the settings UI.
- Save and restore `DIGITAL` / `ANALOG` through the existing EEPROM v3 record.
- Force a full clock redraw after committing settings.

Build verification:

- Firmware builds.
- EEPROM record size remains 64 bytes.
- Existing version 2 migration still defaults to digital.

### Phase 2: Display Primitives

- Add public line and circle primitives.
- Keep all primitives write-only.
- Add clipping so out-of-range coordinates do not corrupt drawing.

Build verification:

- Firmware builds.
- Existing digital display code still compiles unchanged.

### Phase 3: Static Analog Face

- Add analog layout constants.
- Draw header, date, clock face, 12 hour ticks, and alarm summary.
- Do not draw placeholder hands. Hands are added only in Phase 4 from a real
  RTC sample.

Verification target covered by the final hardware check:

- Selecting `Style ANALOG` changes to an analog face.
- Battery and alarm summary remain readable.
- No LCD readback or bus mode switching is introduced.

### Phase 4: RTC-Driven Hands

- Draw hour, minute, and optional second hand from the latest RTC value.
- Implement hand-only updates for normal time changes.
- Use full analog redraw only when entering analog mode or returning from
  another UI.
- Keep digital mode behavior unchanged.
- Disable digital colon blink drawing while in analog mode.

Verification target covered by the final hardware check:

- With `Seconds ON`, the second hand advances once per second.
- With `Seconds OFF`, only hour/minute hands are shown.
- The second hand does not cause full-screen or full-face flashing.
- RTC read frequency is not increased.

### Phase 5: Integration Cleanup

- Verify transitions among clock, settings, time setting, alarm setting, and
  alarm ringing screens.
- Ensure every return path invalidates the clock drawing cache.
- Update README and settings documentation.
- Record the hardware test purpose and expected logs in
  `docs/archive/PROJECT_LOG.md` before handing off a UF2.

## Hardware Verification Plan

Use one focused hardware check after Phase 5. Do not request separate hardware
checks after Phase 1, Phase 2, Phase 3, or Phase 4 unless the firmware does not
build or the final check cannot isolate the failure.

The single planned hardware check:

1. Boot firmware and confirm build ID shows `0.8.0-analog-clock`.
2. Open settings with `F7`.
3. Toggle `Style` from `DIGITAL` to `ANALOG`.
4. Commit with `Enter`.
5. Confirm analog face appears.
6. Confirm battery label remains visible.
7. Confirm alarm summary remains visible.
8. Toggle `Seconds` ON/OFF and confirm second hand appears/disappears.
9. Reboot and confirm the selected style is restored from EEPROM.
10. Confirm alarm ringing still overrides the clock display and returns to the
    selected style after stop or timeout.

Expected UART evidence:

```text
BOOT ... version=0.8.0 ...
SETTINGS eeprom load ...
UI mode=settings
SETTINGS eeprom save ok ...
SETTINGS app seconds=... style=analog
```

If settings are already unchanged, `SETTINGS eeprom save skip reason=unchanged`
is also acceptable after verifying that the screen already shows the requested
style.

If flicker is visible, do not change multiple systems at once. First inspect
whether the hand-only update is accidentally clearing the face, header, date, or
alarm bands.

## Risks

- Hand erase/restore may leave small artifacts if it does not restore covered
  tick marks or the outer circle correctly.
- New drawing primitives may have off-by-one clipping bugs.
- Analog face geometry may overlap battery or alarm text.
- Settings validation currently rejects analog style and must be changed.
- Returning from other UI modes must force full redraw or the old delta cache
  may hide the style change.
