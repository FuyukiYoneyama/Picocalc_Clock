# Backlight Power Implementation Plan

This document converts `docs/BACKLIGHT_POWER_PLAN.md` into an implementation
plan.

Status: implemented.

This implementation depends on PicoCalc keyboard firmware that applies direct
`REG_ID_BKL = 0` writes as true zero brightness. The local Arduino keyboard
firmware source was updated to call `lcd_backlight_update_reg()` for direct
`REG_ID_BKL` writes, and the user reported that Picocalc_Clock can now set the
backlight brightness to zero on real hardware.

## Required Behavior

- `Power` short press toggles the app-level LCD backlight state.
- When the app-level state is off, holding `Space` temporarily lights the
  screen.
- Releasing `Space` returns the screen to off if the app-level state is still
  off and no alarm is ringing.
- Alarm ringing always lights the screen.
- When alarm ringing ends, the screen returns to off if the user had turned it
  off.
- `F6`, `F7`, and `F8` cancel the app-level off state and open their target
  screens with the backlight on.

## Confirmed Local Facts

- `src/platform/picocalc_key_table.h` defines `picoment::keys::Power` as
  `0x91`.
- `src/platform/picocalc_keyboard.h` defines key states:
  - `Pressed = 1`
  - `Hold = 2`
  - `Released = 3`
- `src/main.cpp` currently discards every event whose state is not `Pressed`
  before UI dispatch.
- PicoCalc keyboard firmware source has `REG_ID_BKL = 0x05` for LCD
  backlight.
- PicoCalc keyboard firmware source handles short PMU key press by sending
  `KEY_POWER` with `KEY_STATE_PRESSED`.
- PicoCalc keyboard firmware source routes writes to `REG_ID_BKL` through
  `lcd_backlight_update(0)`, and that helper clamps values below
  `LCD_BACKLIGHT_STEP` to `LCD_BACKLIGHT_STEP`.

## Implementation Risk

The current PicoCalc keyboard firmware source shows that a normal write of
`0` to `REG_ID_BKL` may become the minimum brightness, not true off. Because
this feature requires a real backlight-off state, implementation must first
verify the actual control path.

Implementation uses this outcome:

1. If the PicoCalc keyboard firmware accepts `REG_ID_BKL = 0` as true off on
   the target device, use that path.
2. If `REG_ID_BKL = 0` is clamped to minimum brightness, this firmware cannot
   implement true LCD backlight off through the keyboard register alone.

Do not silently degrade "backlight off" into "minimum brightness". That would
not match the requested behavior.

## New Platform API

Add LCD backlight accessors to the existing keyboard platform layer because the
LCD backlight register is exposed by the PicoCalc keyboard controller on the
same I2C address.

Files:

- `src/platform/picocalc_keyboard.h`
- `src/platform/picocalc_keyboard.cpp`

Add constants in `picocalc_keyboard.cpp`:

```cpp
constexpr uint8_t kRegLcdBacklight = 0x05;
constexpr uint8_t kWriteMask = 0x80;
```

Add public functions:

```cpp
bool read_lcd_backlight(uint8_t* value);
bool write_lcd_backlight(uint8_t value);
```

Implementation rules:

- `read_lcd_backlight()` reads register `0x05` using the same read helper style
  as `read_event()`.
- `write_lcd_backlight()` writes two bytes:

```text
0x05 | 0x80
value
```

- `write_lcd_backlight()` returns `true` only when the I2C write transfers both
  bytes.
- Do not add blocking sleeps to the write path unless hardware verification
  shows the keyboard controller requires it.

## Implementation Phases

Use four implementation phases. Phases 1 through 3 are code changes and should
be built together before the single hardware check. Phase 4 is documentation
cleanup after hardware behavior is known.

### Phase 1: Keyboard Backlight Register API

Files:

- `src/platform/picocalc_keyboard.h`
- `src/platform/picocalc_keyboard.cpp`

Tasks:

- Add `read_lcd_backlight()`.
- Add `write_lcd_backlight()`.
- Reuse the existing I2C address and bus initialization.
- Do not add new source files.
- Do not change `CMakeLists.txt`.

Build acceptance:

- The firmware builds.
- Existing keyboard event reads still compile and use the same public API.

### Phase 2: Backlight State Helpers

File:

- `src/main.cpp`

Tasks:

- Add `BacklightState`.
- Add `kDefaultRestoreBacklight`.
- Add helper functions listed in this plan.
- Initialize `BacklightState` after keyboard initialization and before the main
  loop.
- Preserve the current nonzero LCD backlight value as `restore_level` when it
  can be read.
- Keep the state RAM-only. Do not add EEPROM fields and do not change the
  settings record format.

Build acceptance:

- The firmware builds.
- No behavior changes are expected yet unless helpers are called.

### Phase 3: Main Loop Integration

File:

- `src/main.cpp`

Tasks:

- Move the `Pressed` filter so the backlight handler sees `Space Released`.
- Call `handle_backlight_key_event()` before normal UI dispatch.
- Add alarm start/stop hooks.
- Keep existing UI behavior unchanged except for backlight behavior.
- Keep logging event-based only.

Build acceptance:

- The firmware builds.
- Existing F6/F7/F8/Space behavior is still reachable through normal UI
  dispatch.

### Phase 4: Documentation After Hardware Check

Files:

- `README.md`
- `docs/BACKLIGHT_POWER_PLAN.md`
- `docs/BACKLIGHT_POWER_IMPLEMENTATION_PLAN.md`
- `docs/archive/PROJECT_LOG.md`

Tasks:

- Record the hardware check in `docs/archive/PROJECT_LOG.md`.
- If true off works, change status from planned to implemented.
- Add the short user-facing README usage section.
- If true off does not work, leave the feature documented as blocked and do not
  claim it as implemented.

No firmware behavior should be changed in Phase 4.

## Backlight State

Add this state near the other main-loop state in `src/main.cpp`:

```cpp
constexpr uint8_t kDefaultRestoreBacklight = 32;

struct BacklightState {
    bool user_off;
    bool space_peek_active;
    bool alarm_forced_on;
    uint8_t restore_level;
};
```

Initialization:

- Try to read current LCD backlight with `read_lcd_backlight()`.
- If read succeeds and value is nonzero, set `restore_level` to that value.
- Otherwise set `restore_level` to `kDefaultRestoreBacklight`.
- Start with:

```text
user_off = false
space_peek_active = false
alarm_forced_on = false
```

Whenever turning the backlight on, write `restore_level`.

Whenever turning the backlight off, first ensure `restore_level` is nonzero.
If the most recent read value is nonzero, preserve it before writing off.

If a backlight write fails, log it and do not change `user_off`,
`space_peek_active`, or `alarm_forced_on`. The application state must reflect
the last requested state that was successfully applied to hardware.

## Helper Functions

Implement local helpers in `src/main.cpp`:

```cpp
void backlight_turn_on(BacklightState* state);
void backlight_turn_off(BacklightState* state);
void backlight_cancel_user_off(BacklightState* state);
bool handle_backlight_key_event(const picoment::keyboard::KeyEvent& event,
                                UiMode ui_mode,
                                BacklightState* state);
void backlight_alarm_started(BacklightState* state);
void backlight_alarm_stopped(BacklightState* state);
```

Expected behavior:

- `backlight_turn_on()` writes `state->restore_level`.
- `backlight_turn_off()` writes `0`.
- `backlight_cancel_user_off()` clears all backlight override flags and writes
  `restore_level`.
- `handle_backlight_key_event()` handles:
  - `Power` `Pressed`
  - `Space` `Pressed`
  - `Space` `Released`
  - `F6` / `F7` / `F8` `Pressed`
- It returns `true` only when the event should not continue to normal UI
  dispatch.

Dispatch rule:

- `Power` events are consumed by backlight handling.
- `F6` / `F7` / `F8` `Pressed` events are not consumed; they cancel
  backlight-off and then continue to normal UI dispatch.
- `Space` `Pressed` events are not consumed; normal UI behavior still applies.
- `Space` `Released` events are handled for temporary-view cleanup, then normal
  UI dispatch ignores them because they are not `Pressed`.
- `Released` events other than `Space` are ignored by normal UI dispatch.
- Repeated `Space` `Pressed` events while `space_peek_active` is already true
  must not rewrite the backlight or repeat `BACKLIGHT peek=on` logs.

## Main Loop Changes

Current structure:

```text
read key event
if state != Pressed: continue
normal UI dispatch
```

Required structure:

```text
read key event
handle backlight key event first
if handler consumed the event: continue
if state != Pressed: continue
normal UI dispatch
```

This keeps existing screens mostly `Pressed`-only while still allowing
`Space Released` to turn the temporary backlight view off.

## Alarm Integration

Add `backlight_alarm_started()` at the existing alarm-fire point, immediately
before or after `draw_alarm_ringing_screen()`.

Add `backlight_alarm_stopped()` in both alarm stop paths:

- manual `Space` stop
- 60-second timeout

The stop helper must run after `ui_mode` leaves `AlarmRinging` or receive enough
context to know the alarm is no longer ringing.

If `user_off` is true and `alarm_forced_on` is true, the stop helper writes the
backlight off again.

Power during alarm ringing:

- Alarm ringing has physical-backlight priority.
- If `Power` is pressed while alarm is ringing, toggle only the desired
  `user_off` state and keep the physical backlight on.
- When the alarm stops:
  - if `user_off` is true, turn the physical backlight off;
  - if `user_off` is false, leave the physical backlight on.

This keeps the user-visible alarm behavior stable while still letting the user
choose the post-alarm backlight state.

## F6 / F7 / F8 Integration

The backlight handler cancels user-off before normal UI dispatch sees these
keys.

Normal behavior remains:

- `F6`: open alarm settings
- `F7`: open general settings
- `F8`: read RTC and open time setting if the read succeeds

If `F8` fails because RTC read fails, the backlight still remains on. The user
requested an interactive operation, so the app-level off state has already been
cancelled.

## Logging

Keep logs concise and event-based only:

```text
BACKLIGHT user=off
BACKLIGHT user=on
BACKLIGHT peek=on
BACKLIGHT peek=off
BACKLIGHT alarm=on
BACKLIGHT alarm=restore-off
BACKLIGHT interactive=on key=F7
BACKLIGHT write fail value=0
```

Do not log every main-loop iteration.

## Documentation Status

- `docs/BACKLIGHT_POWER_PLAN.md` records the implemented behavior.
- `README.md` includes the short user-facing `Power Saving` section.
- `README.md` includes the feature in the `Implemented` list.
- Release notes should be updated when the firmware version is advanced.

## Build Verification

Run:

```sh
cmake --build build
```

Acceptance:

- Build succeeds.
- Generated build info contains the current git hash.
- `PICOCALC_CLOCK_GIT_DIRTY` is expected to be `1` until the implementation is
  committed.

## Hardware Verification

Target: one focused hardware check.

The single hardware check is also the `REG_ID_BKL = 0` gate. Do not request a
separate preliminary hardware check unless the build cannot isolate this gate.

Precondition:

- Build a UF2 that prints a unique build purpose or git hash.
- Record expected logs in `docs/archive/PROJECT_LOG.md` before handing off the
  UF2.

Check sequence:

1. Boot firmware and confirm the clock display is visible.
2. Short-press `Power`.
   - Expected: backlight turns off.
   - Expected log: `BACKLIGHT user=off`.
3. Hold `Space`.
   - Expected: backlight turns on while held.
   - Expected log: `BACKLIGHT peek=on`.
4. Release `Space`.
   - Expected: backlight returns off.
   - Expected log: `BACKLIGHT peek=off`.
5. Short-press `Power`.
   - Expected: backlight turns on and stays on.
   - Expected log: `BACKLIGHT user=on`.
6. Short-press `Power` again, then press `F7`.
   - Expected: backlight turns on and settings screen opens.
   - Expected log: `BACKLIGHT interactive=on key=F7`.
7. Return to clock, turn backlight off, then trigger an alarm.
   - Expected: backlight turns on while the alarm rings.
   - Expected log: `BACKLIGHT alarm=on`.
8. Stop alarm with `Space`.
   - Expected: alarm stops and backlight returns off.
   - Expected log: `ALARM stopped by Space` and
     `BACKLIGHT alarm=restore-off`.
9. Repeat the alarm case without pressing `Space`.
   - Expected: after 60 seconds, alarm auto-stops and backlight returns off.
   - Expected log: `ALARM auto stop timeout=60s` and
     `BACKLIGHT alarm=restore-off`.

Additional hardware check is allowed only if step 2 shows that `REG_ID_BKL = 0`
does not produce true backlight off. In that case, do not continue implementing
the rest as if the display were off.
