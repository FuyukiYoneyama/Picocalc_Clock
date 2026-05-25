# Backlight Power Plan

This document defines the planned Picocalc_Clock behavior for LCD backlight
power saving.

Status: implemented.

This behavior requires PicoCalc keyboard firmware that applies `REG_ID_BKL = 0`
as true zero brightness. The local Arduino keyboard firmware source was updated
to route direct `REG_ID_BKL` writes through `lcd_backlight_update_reg()`, and
the user reported that Picocalc_Clock can turn the backlight brightness to zero
on real hardware.

## Goals

- Let the user turn the LCD backlight off without stopping the clock
  application.
- Let the user restore the backlight with the same key.
- Allow a quick temporary view while the backlight is off.
- Ensure alarms remain visible even if the user previously turned the
  backlight off.
- Treat entry into interactive setting screens as an explicit request to use
  the device, and restore the backlight.

## User Behavior

- Short-press `Power` once to turn the LCD backlight off.
- Short-press `Power` again to turn the LCD backlight on.
- While the backlight is off, hold `Space` to light the screen temporarily.
- Releasing `Space` returns the backlight to off.
- If an alarm starts while the backlight is off, the firmware turns the
  backlight on while the alarm is ringing.
- When the alarm is stopped manually or by the 60-second timeout, the firmware
  returns the backlight to off if the user backlight-off state is still active.
- Pressing `F6`, `F7`, or `F8` while the backlight is off cancels the
  backlight-off state and opens the requested screen with the backlight on.

## State Model

Use application-owned state separate from the PicoCalc keyboard firmware's
brightness controls:

```text
user_backlight_off: true / false
space_peek_active: true / false
alarm_forced_backlight_on: true / false
```

`user_backlight_off` records the user's app-level backlight-off request.
`space_peek_active` records the temporary hold-to-view state.
`alarm_forced_backlight_on` records that the alarm temporarily overrode the
user backlight-off state.

## Event Rules

### Power

Handle the `Power` key on `KEY_STATE_PRESSED`.

If `user_backlight_off` is `false`:

```text
turn LCD backlight off
user_backlight_off = true
space_peek_active = false
alarm_forced_backlight_on = false
```

If `user_backlight_off` is `true`:

```text
turn LCD backlight on
user_backlight_off = false
space_peek_active = false
alarm_forced_backlight_on = false
```

### Space Temporary View

This feature requires handling both `KEY_STATE_PRESSED` and
`KEY_STATE_RELEASED` for `Space`.

When `user_backlight_off` is `true` and `Space` is pressed:

```text
turn LCD backlight on
space_peek_active = true
```

When `space_peek_active` is `true` and `Space` is released:

```text
space_peek_active = false
if user_backlight_off is still true and no alarm is ringing:
    turn LCD backlight off
```

Normal `Space` behavior is still allowed. For example, during alarm ringing,
`Space` both lights the screen and stops the alarm.

### Alarm Ringing

When an alarm starts:

```text
if user_backlight_off is true:
    turn LCD backlight on
    alarm_forced_backlight_on = true
```

When an alarm stops by `Space` or by the 60-second timeout:

```text
if user_backlight_off is true and alarm_forced_backlight_on is true:
    turn LCD backlight off
alarm_forced_backlight_on = false
space_peek_active = false
```

If the user presses `Power` while the alarm is ringing, the normal `Power`
toggle applies. The alarm stop path should then respect the current
`user_backlight_off` state.

### F6 / F7 / F8

When `F6`, `F7`, or `F8` is pressed:

```text
turn LCD backlight on
user_backlight_off = false
space_peek_active = false
alarm_forced_backlight_on = false
continue opening the requested screen
```

This applies to:

- `F6`: alarm settings
- `F7`: general settings
- `F8`: time setting

## Implementation Notes

- Process backlight-related key events before the normal UI dispatch.
- Do not discard `KEY_STATE_RELEASED` globally before the backlight handler
  sees `Space` release.
- Keep the normal UI rule that most actions only use `KEY_STATE_PRESSED`.
- Use the same LCD-backlight control path already used for the existing
  Power-key backlight-off behavior.
- Do not document PicoCalc keyboard-firmware brightness shortcuts in the
  Picocalc_Clock README; they are PicoCalc platform behavior, not app-specific
  behavior.

## Verification

One focused hardware check should cover the feature:

1. Boot firmware and confirm the clock display is visible.
2. Short-press `Power`; the LCD backlight turns off and the clock keeps
   running.
3. Hold `Space`; the screen lights only while `Space` is held.
4. Short-press `Power`; the LCD backlight turns on and stays on.
5. Short-press `Power` again to turn the backlight off, then press `F7`; the
   backlight turns on and the settings screen opens.
6. Trigger or wait for an alarm while the backlight-off state is active; the
   backlight turns on while ringing.
7. Stop the alarm with `Space`; the backlight returns to off.
8. Repeat the alarm case without pressing `Space`; after the 60-second timeout,
   the backlight returns to off.
