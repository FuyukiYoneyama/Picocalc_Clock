# Set Time UI

This document defines the implemented PicoCalc on-device time setting screen.
The UART command interface remains a development and maintenance aid; this
screen is the primary user-facing way to set the clock.

Status: implemented in firmware `0.5.6` and still present in the current
firmware.

## Goals

- Open a time setting screen from the normal clock display.
- Edit date and time with PicoCalc keys.
- Keep the edited date/time valid at all times.
- Write the final value to the DS3231 RTC with `Enter`.
- Return to the clock display with `Esc` without saving.

## Non-Goals

This section describes the scope of the original time-setting implementation
phase, not the complete current firmware.

- Do not add new UART commands for this feature unless they are required for
  debugging.
- Do not implement alarm settings in this phase.
- Do not implement EEPROM or Flash setting persistence in this phase.
- Do not build a general settings framework in this phase.

## Entry Key

The time setting screen is opened from the clock screen with:

- `Shift + F3`, which the PicoCalc keyboard firmware reports as `F8`.

Implementation should detect `picoment::keys::F8` directly. The STM32 keyboard
firmware already maps shifted function keys through the key table, so the Pico
application does not need to track Shift state for this shortcut.

Do not rely on the current generic `ClockKey` mapping for this shortcut unless
that mapping is explicitly extended. The first implementation may handle raw
`picoment::keys::F8` before converting other keys to the application-level key
enum. If the keymap is extended, use a named action such as `ClockKey::SetTime`
so the entry shortcut is not hidden behind `Other`.

Only `KEY_STATE_PRESSED` opens the time setting screen. Ignore `KEY_STATE_HOLD`
and `KEY_STATE_RELEASED` for this shortcut so the screen is not opened twice or
immediately affected by the key release event.

Other implemented shortcuts:

- `Shift + F1`, reported as `F6`, opens alarm settings.
- `Shift + F2`, reported as `F7`, opens general settings.

## Screen Fields

The editable value is shown as:

```text
20YY-MM-DD
HH:MM:SS
```

Fields:

- `Year`: shown as 4 digits, but only the lower 2 digits are editable
- `Month`: 2 digits
- `Day`: 2 digits
- `Hour`: 2 digits
- `Minute`: 2 digits
- `Second`: 2 digits

The first two year digits, `20`, are fixed decoration in this implementation.
The editable RTC value is the lower two digits, `YY`, because the DS3231 year
register stores only `00` - `99`.

## Selection Model

The UI has two selection modes.

### Field Selection

Field selection highlights the whole field:

- `YY` in `20YY`
- `MM`
- `DD`
- `HH`
- `MM`
- `SS`

Behavior:

- Left / Right moves to the previous or next field.
- Up / Down increments or decrements the whole selected field.
- A digit key starts digit input for the selected field.
- When digit input starts, the first editable digit of the selected field is
  replaced and the UI enters digit selection mode.
- Left on the first field stays on `Year`.
- Right on the last field stays on `Second`.

Initial state:

- The `Year` field is selected, but only the lower two year digits are
  highlighted. The fixed `20` prefix is not highlighted.

### Digit Selection

Digit selection highlights one digit inside the current field.

Behavior:

- A digit key replaces the highlighted digit and advances to the next digit.
- Up / Down increments or decrements only the highlighted digit.
- Left / Right moves to the previous or next digit.
- Moving past the edge of a field selects the neighboring field as a whole.
- After the final digit of a field is entered, the next field is selected as a
  whole.
- Left on the first editable digit of `Year` selects the lower two year digits
  as the whole `Year` field.
- Right on the last digit of `Second` selects the whole `Second` field.
- A digit entered on the last digit of `Second` leaves `Second` selected as a
  whole after normalization.

## Highlighting

The current selection must be shown with a changed background color. Do not use
an underline as the primary cursor indicator.

Recommended display states:

- Normal text: white on black.
- Selected field: dark text on a bright highlight background.
- Selected digit: dark text on a brighter or more focused highlight background.
- Error messages, if needed: yellow or another warning color.

## Numeric Input Rules

Field selection examples:

- If `Year` is shown as `2026` and selected as a field, only `26` is
  highlighted.
- If `Year` is selected as `2026` and the user presses `1`, the lower year tens
  digit is accepted, the display becomes `2016`, and the lower ones digit is
  selected.
- If `Month` is selected as `05` and the user presses `1`, the first digit is
  accepted, the value is immediately normalized to a valid month with that
  prefix, and the second digit is selected. The displayed month becomes `10`
  until the second digit is edited.

Digit selection examples:

- If the selected digit is the lower year tens digit in `2026` and the user
  presses Up, only that editable `2` digit changes.
- If Right is pressed on the last editable digit of `YY`, the `Month` field is
  selected as a whole.

## Dynamic Validation

The editor must keep the date/time valid while editing. `Enter` should normally
fail only if the RTC write or readback fails.

## DS3231 Year Range Policy

The DS3231 stores the year as a BCD `00` - `99` value in register `0x06`.
Register `0x05` stores the month and also has the century bit at bit 7. The
DS3231 datasheet says the century bit toggles when the year register overflows
from `99` to `00`, and the RTC's leap-year compensation is valid up to `2100`.

For the first on-device time setting implementation, `Picocalc_Clock` uses the
same policy as the current DS3231 driver:

- accept years `2000` - `2099`;
- display the year as `20YY`;
- treat `20` as fixed decoration in the UI;
- allow cursor movement and highlight only on the editable `YY` digits;
- write `year - 2000` to the DS3231 year register;
- keep the DS3231 century bit clear;
- do not expose `2100` or later in the UI.

If the DS3231 month register already has the century bit set, the first
implementation still interprets the year register as `2000` - `2099`, matching
the current driver. Saving from the time setting screen writes the month without
the century bit, which clears that bit.

Supporting another century would require an explicit century-bit policy and
driver changes. It is out of scope for this screen.

Allowed ranges:

- Year: `2000` to `2099`
- Month: `01` to `12`
- Day: `01` to the valid maximum day for the current year/month
- Hour: `00` to `23`
- Minute: `00` to `59`
- Second: `00` to `59`

The screen must never display an invalid date/time as the stable editor state.

Final normalization rules:

- Up / Down on a whole field wraps within that field's valid range.
  - Year wraps within `2000..2099`.
    - Up on `2099` wraps to `2000`.
    - Down on `2000` wraps to `2099`.
  - Month wraps within `01..12`.
  - Day wraps within `01..days_in_month(year, month)`.
  - Hour wraps within `00..23`.
  - Minute and Second wrap within `00..59`.
- Up / Down on a single digit edits that digit, then normalizes the whole field.
- Digit entry is prefix-valid:
  - after a digit is entered, the field must still be able to become at least one
    valid value while preserving the digits already typed in that field;
  - if the typed prefix cannot produce a valid value, reject the digit and keep
    the current selection;
  - if the typed prefix is possible but the current suffix would make the field
    invalid, normalize to the lowest valid value that preserves the typed prefix;
  - examples: Month first digit `1` normalizes to `10`, Month first digit `0`
    may normalize to `01`, and Month first digit `9` is rejected.
- Year digit entry edits only `YY`; all `00` - `99` lower-year values are valid
  and are displayed as `2000` - `2099`.
- Drawing uses the normalized committed state only. Do not draw hidden invalid
  edit-buffer values.

## Preferred Day Rule

The editor must preserve the user's intended day-of-month when changing year or
month.

Maintain both:

- `day`: the valid displayed day for the current year/month.
- `preferred_day`: the day the user intends to keep.

Rules:

- On screen entry, set `preferred_day = current RTC day`.
- When Year or Month changes:
  - `day = min(preferred_day, days_in_month(year, month))`.
- When the user edits Day directly:
  - `day = new_day`.
  - `preferred_day = new_day`.

For digit entry on the Day field, update `preferred_day` only when the Day value
is committed. A Day value is committed when:

- both Day digits have been entered;
- the user moves out of the Day field with Left or Right;
- the user presses Up or Down while a Day digit is selected.

This prevents a partially typed first digit from permanently changing the
month-end preference.

When Up or Down is pressed on a Day digit, apply the digit change first,
normalize the resulting Day value, and then store that normalized value as
`preferred_day`.

Examples:

```text
Start: 2026-05-31
preferred_day = 31

Month -> 02:
display day = 28
preferred_day remains 31

Month -> 01:
display day = 31
preferred_day remains 31
```

Leap year example:

```text
preferred_day = 29
2024-02 -> 2024-02-29
2026-02 -> 2026-02-28
2024-02 -> 2024-02-29
```

## Key Behavior

- `F8`: open the time setting screen from the clock screen.
- `Left`: previous field/digit.
- `Right`: next field/digit.
- `Up`: increment selected field or digit.
- `Down`: decrement selected field or digit.
- `0` - `9`: direct digit entry.
- `Enter`: write the edited value to the DS3231 RTC.
- `Esc`: cancel and return to the clock screen.

The first implementation handles all screen-changing key actions on
`KEY_STATE_PRESSED` only. Ignore both `KEY_STATE_RELEASED` and `KEY_STATE_HOLD`.
Long-press repeat for Up / Down / Left / Right may be added later, but it must
have an explicit repeat interval and must not be enabled accidentally by raw
STM32 hold events.

## Keyboard Event Dispatch

The main loop must read PicoCalc keyboard events independently of UART polling.
UART polling may be disabled for power saving, but the on-device UI still needs
keyboard input.

Implementation rules:

- Drain pending `picoment::keyboard::read_event()` events on each main-loop
  iteration, or at a fixed short interval that keeps the UI responsive.
- Keyboard polling must not inherit the RTC sleep interval. Even when UART
  polling is disabled and the RTC cadence is resting for `900ms`, keyboard
  polling keeps the main-loop sleep cap short.
- Use a keyboard/UI sleep cap of `10ms` to `30ms` while the on-device UI is
  enabled. The first implementation should use `20ms` as the target cap.
- Dispatch events by current UI mode:
  - `Clock`: handle `F8` on `KEY_STATE_PRESSED` and enter `SetTime`.
  - `SetTime`: handle arrows, digits, `Enter`, and `Esc` on
    `KEY_STATE_PRESSED`.
- Ignore `KEY_STATE_RELEASED` and `KEY_STATE_HOLD` in the first implementation.
- Keyboard event handling must run before any clock redraw decision for that
  loop, so entering `SetTime` cannot be immediately overwritten by the normal
  clock display.

## Clock Update Behavior While Editing

While the UI mode is `SetTime`:

- The normal clock display must not redraw over the setting screen.
- Stop the normal RTC display polling cadence.
- Do not run background RTC health polling in the first implementation.
- RTC reads in `SetTime` are limited to:
  - one read when entering the screen, used to initialize the edit model;
  - write/readback on `Enter`;
  - one fresh read when returning to the clock display after `Enter` or `Esc`.
- The edit model is initialized from the RTC when entering the screen and then
  remains under user control until `Enter` or `Esc`.
- Battery-aware UART polling may continue as before.
- On successful `Enter`, return to clock mode and force the clock screen to
  redraw from a fresh RTC read.
- On `Esc`, return to clock mode and force the clock screen to redraw from a
  fresh RTC read.

Because the normal clock screen uses delta drawing, returning from `SetTime`
must invalidate the clock draw cache. At minimum:

- redraw the clock frame/background;
- clear the previous date/time/battery strings used by delta drawing;
- reset RTC cadence state enough to force an immediate fresh RTC read;
- draw the clock from the value read back from the DS3231, not from the edited
  buffer.

## RTC Write Behavior

On `Enter`:

1. Convert the edited value to `ds3231_datetime_t`.
2. Calculate and set the DS3231 day-of-week field.
3. Call `ds3231_write_time()`.
4. Read the RTC back with `ds3231_read_time()`.
5. If write and readback succeed, return to the clock display.
6. If write or readback fails, stay on the setting screen and show `SET FAIL`.

The displayed clock must use the RTC value read from the DS3231, not a locally
incremented or predicted time value.

## Debug Logging

Logs are useful while developing this screen, but UART commands should not be
expanded unless necessary.

Recommended debug log events:

- `UI mode=set-time`
- `SETTIME field=...`
- `SETTIME digit=...`
- `SETTIME normalized ...`
- `SETTIME write start ...`
- `SETTIME write ok`
- `SETTIME write fail`
- `UI mode=clock`

These logs should be controlled by existing build/log configuration so they can
be reduced later.

## Implementation Steps

1. Add an application UI mode enum:
   - `Clock`
   - `SetTime`
2. Add a time edit model:
   - year, month, day, hour, minute, second
   - preferred_day
   - selected field
   - selection mode
   - selected digit index
   - for `Year`, digit index covers only the editable lower two digits
3. Add normalize helpers:
   - leap year
   - days in month
   - normalize date/time
   - apply preferred day
4. Add keyboard event polling/dispatch in the main loop:
   - read keyboard events independently of UART polling
   - dispatch events by `Clock` / `SetTime` mode
   - handle only `KEY_STATE_PRESSED` in the first implementation
   - cap UI/keyboard sleep at `20ms` so key response is not delayed by the RTC
     `900ms` rest interval
5. Add key handling for the clock screen:
   - `F8` with `KEY_STATE_PRESSED` enters `SetTime`.
   - either detect raw `picoment::keys::F8` before generic key mapping or extend
     the application keymap with a named set-time action
6. Add key handling for the setting screen:
   - movement
   - field increment/decrement
   - digit input
   - `Enter`
   - `Esc`
   - ignore release events for screen-changing actions
7. Add drawing for the setting screen:
   - large readable date/time
   - field/digit background highlight
   - short status message area
8. Add SetTime exit handling:
   - invalidate clock delta draw cache
   - force an immediate RTC read
   - redraw the normal clock frame before returning to delta updates
9. Keep RTC polling mode-specific:
   - `Clock`: use the existing RTC second-boundary cadence
   - `SetTime`: stop background RTC display polling and read only on entry,
     `Enter`, and exit
10. Build and verify with UART logs.

## Acceptance Criteria

- `F8` opens the time setting screen from the clock display.
- `F8` opens the setting screen only once per press.
- Keyboard events are read even when UART polling is disabled for power saving.
- Keyboard response is not delayed by the RTC `900ms` rest interval.
- The initial selected field is `Year`, with only the lower two year digits
  highlighted.
- Only the lower two year digits are editable and highlighted; the `20` prefix
  is fixed decoration.
- Field selection and digit selection are visually obvious by background color.
- Numeric entry works for all fields.
- Up / Down works on both whole fields and single digits.
- Invalid dates are never displayed as stable editor state.
- Digit entry rejects impossible prefixes and normalizes possible prefixes to a
  valid displayed value immediately.
- Month changes preserve `preferred_day` behavior for month-end dates.
- SetTime mode is not overwritten by normal clock redraws.
- SetTime mode does not run the normal RTC display polling cadence.
- `Enter` writes the displayed value to the DS3231 RTC and returns to the clock
  display on success.
- `Esc` cancels without writing and returns to the clock display.
- Returning to the clock display always redraws the clock screen even if the
  date/time text is unchanged from the previous cached value.
- No new UART command is required for normal use of this feature.

## References

- Analog Devices, DS3231 datasheet/product page:
  <https://www.analog.com/en/products/ds3231.html>
