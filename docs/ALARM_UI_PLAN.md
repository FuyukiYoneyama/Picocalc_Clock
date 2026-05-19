# Alarm UI Plan

This document records the PicoCalc alarm feature. It began as the alarm
implementation plan and now describes the implemented behavior without relying
on the chat history.

Status: alarm UI implemented in firmware `0.6.0`; alarm EEPROM resume added in
firmware `0.6.1`. The current firmware keeps this alarm behavior and stores the
alarm data inside the shared settings record.

Build purpose string for the alarm EEPROM implementation:

```text
0.6.1-alarm-eeprom
```

## Goals

- Provide five daily alarms.
- Open the alarm setting screen from the clock display.
- Edit alarm time and ON/OFF state on the PicoCalc itself.
- Trigger an audible alarm at the configured time.
- Stop the alarm with `Space`.
- Avoid adding UART commands unless they are required for debugging.

## Non-Goals

This section describes the scope of the original alarm implementation phase,
not the complete current firmware.

- Do not implement complex repeat rules such as weekday-only alarms.
- Do not use the DS3231 hardware alarm registers in the first implementation.
- Do not add a general settings menu in this phase.

## Entry Keys

The function-key shortcuts are assigned as follows:

- `Shift + F1`, reported by the PicoCalc keyboard firmware as `F6`: alarm
  setting screen.
- `Shift + F2`, reported as `F7`: general settings screen.
- `Shift + F3`, reported as `F8`: time setting screen, already implemented.

Only `KEY_STATE_PRESSED` should trigger a screen transition. Ignore
`KEY_STATE_HOLD` and `KEY_STATE_RELEASED` in the first implementation.

## Alarm Count

The application supports five alarms:

```cpp
constexpr int kAlarmCount = 5;
```

Each alarm has:

```cpp
struct AlarmSettings {
    bool enabled;
    uint8_t hour;    // 0..23
    uint8_t minute;  // 0..59
};
```

Initial defaults:

```text
A1 OFF 07:30
A2 OFF 08:00
A3 OFF 12:00
A4 OFF 18:00
A5 OFF 22:00
```

Firmware `0.6.1` introduced AT24C32 EEPROM persistence for these settings at
I2C address `0x57`. Restarting the firmware loads the newest valid EEPROM
record. If the EEPROM is not detected or both records are invalid, the firmware
keeps the defaults above.

EEPROM layout:

```text
0x0000 - 0x003F  slot A
0x0040 - 0x007F  slot B
```

Each slot is a 64-byte record. The alarm-only version 2 record was:

```cpp
struct SettingsRecord {
    uint32_t magic;       // 0x4b4c4350, "PCLK"
    uint16_t version;     // 2
    uint16_t size;        // 64
    uint32_t sequence;
    uint8_t alarm_enabled[5];
    uint8_t alarm_hour[5];
    uint8_t alarm_minute[5];
    uint8_t reserved[33];
    uint32_t crc32;
};
```

Current firmware writes version 3 of the same 64-byte record. Version 3 keeps
the alarm fields and uses part of the former reserved area for display settings:

```cpp
struct SettingsRecord {
    uint32_t magic;       // 0x4b4c4350, "PCLK"
    uint16_t version;     // 3
    uint16_t size;        // 64
    uint32_t sequence;
    uint8_t alarm_enabled[5];
    uint8_t alarm_hour[5];
    uint8_t alarm_minute[5];
    uint8_t app_flags;    // bit 0: show seconds
    uint8_t clock_style;  // 0: digital
    uint8_t reserved[31];
    uint32_t crc32;
};
```

The CRC32 covers every byte before the `crc32` field. Version 2 and version 3
records are accepted when loading. Version 2 records migrate by using the
default app settings: seconds ON and digital style.

Writes alternate between slot A and slot B using the sequence number. The
firmware writes only when the edited alarm list actually changes. Each slot
write is split into two 32-byte AT24C32 page writes, followed by ACK polling
and readback verification.

## Alarm Setting Screen

The alarm setting screen shows all five alarms at once:

```text
SET ALARM

> A1 07:30 ON
  A2 08:00 OFF
  A3 12:00 OFF
  A4 18:00 OFF
  A5 22:00 OFF
```

The current row and field must be shown with a changed background color. Do not
use an underline as the primary cursor indicator.

Editable fields per row:

- `Hour`
- `Minute`
- `ON/OFF`

The alarm number itself is not edited as a field. It is selected by moving the
current row.

Use the same general visual language as the Set Time UI:

- black background;
- white normal text;
- changed background color for the selected row or field;
- no underline as the primary cursor indicator.

Recommended layout:

- Title: `SET ALARM`, `S12x24`, near the top.
- Alarm rows: `S12x24`, one row per alarm.
- Footer help: small Cozette text such as `Enter=save Esc=cancel`.

## Controls

| Key | Action |
| --- | --- |
| `Up` / `Down` | Move rows in row selection; change values in field/digit selection |
| `Left` / `Right` | Move between `Hour`, `Minute`, and `ON/OFF` |
| `0` - `9` | Enter digits for `Hour` or `Minute` |
| `Enter` | Commit the edited alarm list from any selection mode |
| `Esc` | Step back inside the editor, or discard edits from row selection |

## Selection Model

The alarm UI has three selection modes.

### Row Selection

Row selection highlights one whole alarm row.

Behavior:

- `Up` / `Down` moves between `A1` and `A5`.
- `Left` / `Right` enters field selection on the highlighted row.
- `Enter` accepts the edited alarm list and returns to the clock display.
- `Esc` discards edits and returns to the clock display.
- Digit keys are ignored.

Initial state:

- `A1` is selected as a row.

### Field Selection

Field selection highlights one field on the current row.

Fields:

- `Hour`
- `Minute`
- `ON/OFF`

Behavior:

- `Up` / `Down` on `Hour` wraps within `00..23`.
- `Up` / `Down` on `Minute` wraps within `00..59`.
- `Up` / `Down` on `ON/OFF` toggles the value.
- `Left` / `Right` moves between `Hour`, `Minute`, and `ON/OFF`.
- `Left` on `Hour` returns to row selection.
- `Right` on `ON/OFF` returns to row selection.
- Digit input is active only on `Hour` and `Minute`.
- Digit input on `Hour` or `Minute` starts digit selection.
- Digit input on `ON/OFF` is ignored.
- `Enter` accepts the edited alarm list and returns to the clock display.
- `Esc` returns to row selection. If already in row selection, `Esc` discards
  edits and returns to the clock display.

### Digit Selection

Digit selection highlights one digit inside `Hour` or `Minute`.

Behavior:

- A digit key replaces the highlighted digit and advances to the next digit.
- Up / Down increments or decrements only the highlighted digit, then normalizes
  the whole field.
- Left / Right moves to the previous or next digit.
- Moving past the edge of `Hour` or `Minute` returns to field selection on that
  field.
- After the final digit is entered, return to field selection on that field.
- `Enter` accepts the edited alarm list and returns to the clock display.
- `Esc` returns to field selection for the current field.

Validation:

- Digit input is active only on `Hour` and `Minute`.
- Digit input uses the same prefix-valid behavior as the Set Time UI.
- Digit input is ignored on `ON/OFF`.
- The screen must never display an invalid alarm time as the stable editor
  state.

## Edit Model

The displayed editor state must be separate from the committed alarm settings:

```cpp
enum class AlarmField : uint8_t {
    Hour,
    Minute,
    Enabled,
};

enum class AlarmSelectionMode : uint8_t {
    Row,
    Field,
    Digit,
};

struct AlarmEditModel {
    AlarmSettings alarms[kAlarmCount];
    uint8_t selected_index;      // 0..4
    AlarmField field;
    AlarmSelectionMode selection;
    uint8_t digit_index;         // 0..1 for Hour/Minute
    char status[32];
};
```

Entering `SetAlarm` copies the committed settings into `AlarmEditModel`.
`Enter` copies `AlarmEditModel.alarms` back to the committed settings from any
selection mode. `Esc` steps back by selection mode:

- `Digit` -> `Field`
- `Field` -> `Row`
- `Row` -> discard the edit model and return to the clock display

Discarding leaves the committed settings unchanged.

## Clock Screen Display

The clock screen should show the next enabled alarm without disturbing the
large date/time layout.

Recommended text:

```text
Next A1 07:30
```

If more than one enabled alarm shares the same next time, show a compact marker:

```text
Next A1+ 07:30
```

If all alarms are off:

```text
Alm OFF
```

The next alarm calculation is:

1. Consider all enabled alarms later than the current `HH:MM` today.
2. If any exist, choose the earliest time today.
3. Otherwise choose the earliest enabled alarm tomorrow.
4. If multiple alarms share the chosen time, the lowest alarm number is the
   representative and `+` indicates additional alarms at that same time.

For display, use the same minute suppression rule as firing. If the current
date/hour/minute has already fired, do not show that same minute as the next
alarm. Show the next later matching minute instead.

## Trigger Rules

Alarm checking must use the RTC sample already read for the clock display. Do
not add extra RTC reads just for alarm checking in the first implementation.

An alarm fires when:

- at least one alarm is enabled;
- the current RTC hour and minute match one or more enabled alarms;
- the current date/hour/minute has not already fired.

The fired record should be minute-based, not alarm-index-based:

```cpp
struct AlarmFireRecord {
    uint16_t year;
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t minute;
    bool valid;
};
```

This prevents repeated firing in the same minute and handles multiple alarms
with the same time as one event.

If the user changes alarm settings while the current minute matches an enabled
alarm, do not immediately fire. Wait until the clock enters a later matching
minute.

## Simultaneous Alarms

If multiple alarms are enabled at the same `HH:MM`, they should be treated as
one alarm event.

Example ringing display:

```text
ALARM A1+A2

07:30

Space: Stop
```

Stopping this event with `Space` consumes the whole matching group for that
date/hour/minute. Do not ring again for another alarm in the same minute.

## Ringing Mode

Add a UI mode for active alarms:

```cpp
enum class UiMode {
    Clock,
    SetTime,
    SetAlarm,
    AlarmRinging,
};
```

Keyboard events must be dispatched by UI mode:

- `Clock`: handle `F6`, `F7`, `F8`, normal alarm firing, and clock display.
- `SetTime`: keep the existing Set Time event handling.
- `SetAlarm`: handle alarm editor keys only.
- `AlarmRinging`: handle `Space` stop first and ignore unrelated keys.

Do not let the existing `SetTime` default branch handle `SetAlarm` or
`AlarmRinging` events.

While `AlarmRinging`:

- keep keyboard polling responsive;
- treat `Space` with `KEY_STATE_PRESSED` as the stop command;
- ignore `KEY_STATE_HOLD` and `KEY_STATE_RELEASED`;
- continue servicing the alarm sound generator;
- do not run the normal clock delta drawing path over the ringing screen.
- automatically stop the alarm after `60` seconds if the user does not press
  `Space`.

When stopped:

- stop the audio;
- record the fired date/hour/minute;
- return to the clock display;
- force a clock redraw by invalidating the delta draw cache.

Manual stop and automatic timeout use the same post-stop behavior. Both record
the fired date/hour/minute in `AlarmFireRecord`, and neither may ring again for
the same date/hour/minute.

## Alarm Sound

An audible alarm is required. Screen-only notification is not sufficient.

The implementation should add:

```cpp
void alarm_sound_init();
void alarm_sound_start(uint32_t now_ms);
void alarm_sound_stop();
void alarm_sound_service(uint32_t now_ms);
bool alarm_sound_active();
```

Implementation notes:

- Base the PWM audio path on the existing PicoCalc audio implementation used by
  `Picocalc_ment`.
- Use `picoment::audio_pwm::init_stream()`,
  `picoment::audio_pwm::start_stream()`,
  `picoment::audio_pwm::writable_samples()`, and
  `picoment::audio_pwm::write_sample()`.
- Initial tone: square wave.
- Initial frequency: `880 Hz`.
- Initial pattern: `200 ms ON / 200 ms OFF`.
- Initial amplitude: `48`.
- Initial sample rate: `48000 Hz`.
- Generate samples with a phase accumulator.
- In the OFF part of the pattern, write zero samples.
- While ringing, cap the main-loop sleep to a short interval such as `2 ms` and
  service audio often enough that the PWM stream does not underrun.
- Report underrun information to UART only when useful for debugging.

Source files to add or copy:

- `src/platform/picocalc_audio_pwm.cpp`
- `src/platform/picocalc_audio_pwm.h`
- `src/alarm_sound.cpp`
- `src/alarm_sound.h`

`picocalc_audio_pwm.*` should be copied from the current `Picocalc_ment`
implementation and kept under the existing third-party notice policy. Do not
copy unrelated synthesizer code.

`CMakeLists.txt` additions:

- Add `src/platform/picocalc_audio_pwm.cpp` to `add_executable()`.
- Add `src/alarm_sound.cpp` to `add_executable()`.
- Link the required Pico SDK libraries for PWM audio:
  - `hardware_pwm`
  - `hardware_dma`
  - `hardware_irq`
  - `hardware_clocks`

If the copied audio source requires another Pico SDK library, add it only after
confirming the build error and the referenced API.

## UART Policy

Do not add normal user-facing alarm commands in the first implementation.

UART may log:

```text
ALARM settings enabled=1 time=07:30
ALARM fire date=2026-05-18 time=07:30 alarms=A1+A2
ALARM stopped by Space
ALARM auto stop timeout=60s
ALARM suppress same minute
```

If a manual audio test is needed during bring-up, keep it temporary or clearly
debug-only.

## Implementation Plan

1. Add alarm data structures and RAM defaults.
2. Add `F6` entry handling from the clock screen.
3. Add the `SetAlarm` UI with five visible alarms.
4. Add `AlarmEditModel` and implement `Enter` commit / `Esc` discard.
5. Add edit handling for hour, minute, and ON/OFF.
6. Split keyboard dispatch by `Clock`, `SetTime`, `SetAlarm`, and
   `AlarmRinging`.
7. Add next-alarm calculation and compact clock-screen display.
8. Add alarm trigger detection using the normal RTC display sample.
9. Add `AlarmRinging` mode and `Space` stop handling.
10. Add `alarm_sound.cpp` / `alarm_sound.h` and PWM audio output.
11. Add AT24C32 EEPROM save/load for alarm settings.
12. Update README and documentation after implementation.

## Hardware Verification

Use one focused hardware check for the first complete implementation:

- `F6` opens the alarm setting screen.
- All five alarm rows are visible.
- Hour, minute, and ON/OFF can be edited.
- `Esc` cancels edits.
- `Enter` accepts edits.
- After `Enter`, changed alarm settings are saved to AT24C32 EEPROM.
- After power-cycle or firmware restart, the saved alarm settings are resumed.
- The clock screen shows the next enabled alarm or `Alm OFF`.
- Setting an alarm for the next minute causes the alarm to ring.
- Simultaneous alarms at the same minute ring as one event.
- `Space` stops the alarm.
- If not stopped manually, the alarm auto-stops after 60 seconds.
- The same minute does not ring again after stopping.
- The clock display returns cleanly after stopping.

If audio fails, separate the failure into:

- alarm trigger state reached;
- ringing screen drawn;
- audio initialized;
- audio samples serviced;
- `Space` stop processed.
