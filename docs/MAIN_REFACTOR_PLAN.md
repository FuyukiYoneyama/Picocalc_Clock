# Main Refactor Plan

## Purpose

`src/main.cpp` has become the central file for clock rendering, alarm logic,
RTC helpers, EEPROM settings, Life runtime control, UART commands, backlight
behavior, screenshot orchestration, settings editors, and the main event loop.

The goal is not only to reduce the line count. The goal is to make
`Picocalc_Clock` easier to maintain and easier to keep aligned with
`Picocalc_ClockCalc` where the two applications share clock-side behavior.
Future clock features should be added as feature modules and app routing, not
as another independent subsystem inside `main.cpp`.

This plan intentionally does not change source behavior. It defines the target
shape, implementation order, verification gates, logging policy, and hardware
validation cost model.

## Current Facts

- `src/main.cpp` is 3596 lines.
- Existing modules already include:
  - `src/platform/` for display, keyboard, audio, UART logging, LCD, SD, and
    FatFs time helpers;
  - `src/diagnostics/screenshot_capture.*` for BMP capture;
  - `src/life_board.*` for Conway Life board evolution and stability tracking;
  - `src/alarm_sound.*` for alarm audio;
  - `src/rtc/ds3231.*` for DS3231 access;
  - `src/ui.*` for older UI entry points.
- `src/main.cpp` currently owns application policy and reusable feature logic
  at the same time.
- `Picocalc_ClockCalc` already has a similar clock-side split with
  `alarm/`, `app/`, `clock/`, `settings/`, `life/`, `diagnostics/`, and
  `platform/` modules.

## Review 1 Findings

The first draft was reviewed for implementation readiness. The direction was
right, but several choices were still implicit:

- it did not say which ClockCalc modules should be copied conceptually and
  which calculator-only modules must be omitted;
- the CMake update point for each new `.cpp` file was not explicit enough;
- the settings and alarm extraction phases were too large;
- the plan did not define what should remain in `main.cpp` after each major
  extraction;
- the final hardware verification was mentioned but not costed.

The plan below fixes those points by naming file destinations, build points,
and phase completion criteria.

## Review 2 Findings

The second review focused on hardware validation cost and logging. The weak
points were:

- a single final hardware check was too rigid unless hardware-risk triggers
  were also defined;
- log-driven pass/fail rules were not concrete enough;
- phases said "no hardware check" without saying what happens if behavior is
  accidentally changed;
- the final app-controller phase had too much risk unless a pre-controller
  readiness gate was added.

The plan below makes the default hardware policy one final smoke run, while
allowing narrow additional hardware checks only when a listed hardware-risk
trigger is present.

## Review 3 Findings

The third review checked whether an implementer could execute the plan without
making hidden design decisions. Remaining issues were:

- some helper destinations were still open between `clock/`, `platform/`, and
  `app/`;
- the screenshot prefix and Clock-specific behavior needed explicit protection;
- the plan needed exact log categories and expected low-volume log markers;
- the implementation sequence needed smaller commits than "extract clock UI"
  or "extract alarm".

The final plan below resolves those by fixing ownership rules, exact module
destinations, static checks, and smoke-run expectations.

## Final Review Result

This version is implementation-ready. Each phase names the destination files,
the order of moves, the build checkpoints, the static checks, and the condition
that would justify additional hardware validation. The default hardware plan is
one final log-driven smoke run, with narrow extra runs allowed only for the
hardware-risk triggers listed later.

## Alignment With ClockCalc

Use the ClockCalc refactor as the known-good shape, but do not blindly copy it.

Align these concepts and names where practical:

- `clock/clock_time.*`
- `clock/clock_render.*`
- `clock/analog_render.*`
- `clock/calendar_render.*`
- `clock/clock_help.*`
- `settings/settings_model.h`
- `settings/settings_store.*`
- `alarm/alarm_model.*`
- `alarm/alarm_runtime.*`
- `alarm/alarm_ui.*`
- `life/life_runtime.*`
- `life/life_render.*`
- `diagnostics/screenshot_service.*`
- `diagnostics/uart_commands.*`
- `platform/battery.*`
- `platform/backlight_control.*`
- `platform/startup_probe.*`
- `app/loop_sleep.*`
- `app/app_mode.h`
- `app/app_controller.*`

Do not import calculator-only modules:

- `calculator_state.*`
- `calculator_input.*`
- `calculator_render.*`
- `expression_engine.*`
- `key_actions.*` unless a Clock-only equivalent is explicitly needed later.

Protect Clock-specific behavior:

- screenshot files keep the `clk_####.BMP` prefix;
- the visible app label remains `Clock v...`;
- the startup log remains `Picocalc_Clock version ...`;
- Clock starts in clock mode, not calculator mode;
- Clock does not add Esc-to-clock or Brk-to-calculator routing.

## Design Principles

- Keep `main.cpp` responsible for hardware startup, high-level application
  construction, and the top-level loop.
- Move feature algorithms and UI drawing into named modules.
- Separate hardware access from application policy:
  - hardware drivers stay in `platform/` or `rtc/`;
  - feature behavior stays in `clock/`, `settings/`, `alarm/`, `life/`, or
    `diagnostics/`;
  - integration glue and mode transitions stay in `app/`.
- Prefer plain structs and small functions over framework-like abstractions.
- Move code first. Rename, reshape, or redesign only after the move builds.
- Do not create circular dependencies between features.
- Add low-volume diagnostic logs before asking for hardware validation.

## Dependency Rules

Use these dependency rules while extracting code:

```text
main.cpp
  -> app/
  -> clock/ alarm/ settings/ life/ diagnostics/
  -> platform/ rtc/

app/
  -> clock/ alarm/ settings/ life/ diagnostics/
  -> platform/ rtc/

clock/ alarm/ settings/ life/ diagnostics/
  -> platform/ rtc/ only when they need hardware services

platform/ and rtc/
  -> no app, clock, alarm, settings, life, or diagnostics policy dependencies
```

Rules:

- `platform/` must not include feature headers.
- `rtc/` must not include feature headers.
- `clock/` may depend on `alarm/` only through small data types or alarm label
  helpers, not through alarm UI.
- `settings_store` may know the serialized settings record, but it must not
  draw settings screens.
- `alarm_model` must not draw and must not start audio.
- `alarm_ui` may draw alarm screens but must not decide when an alarm should
  ring.
- `life_runtime` may decide Life start/stop policy, while `life_render` draws
  board pixels.
- `diagnostics/screenshot_service` orchestrates screenshot sounds and capture,
  while `diagnostics/screenshot_capture` keeps the low-level BMP writer.
- `main.cpp` may include feature headers, but after a phase completes it must
  not retain that phase's algorithms.

## Build System Rule

Every phase that adds a `.cpp` file must update `CMakeLists.txt` in the same
commit. The safe order for each extraction is:

1. Add the new header and source file.
2. Add the new `.cpp` file to `add_executable(...)`.
3. Build with the mostly empty module.
4. Include the new header from `main.cpp`.
5. Build.
6. Move one small function group.
7. Build.
8. Repeat only after the previous group builds.

Do not move several independent groups and then update CMake at the end.

## Target Module Layout

```text
src/
  app/
    app_controller.cpp
    app_controller.h
    app_mode.h
    loop_sleep.cpp
    loop_sleep.h
    set_time_editor.cpp
    set_time_editor.h
    settings_editor.cpp
    settings_editor.h
  alarm/
    alarm_model.cpp
    alarm_model.h
    alarm_runtime.cpp
    alarm_runtime.h
    alarm_ui.cpp
    alarm_ui.h
  clock/
    analog_render.cpp
    analog_render.h
    calendar_render.cpp
    calendar_render.h
    clock_help.cpp
    clock_help.h
    clock_render.cpp
    clock_render.h
    clock_time.cpp
    clock_time.h
  settings/
    settings_model.h
    settings_store.cpp
    settings_store.h
  life/
    life_render.cpp
    life_render.h
    life_runtime.cpp
    life_runtime.h
  diagnostics/
    screenshot_capture.*
    screenshot_service.cpp
    screenshot_service.h
    uart_commands.cpp
    uart_commands.h
  platform/
    backlight_control.cpp
    backlight_control.h
    battery.cpp
    battery.h
    startup_probe.cpp
    startup_probe.h
    picocalc_display.*
    picocalc_keyboard.*
    picocalc_audio_pwm.*
    sd/
  rtc/
    ds3231.*
  main.cpp
```

This is the target shape. Introduce only the files named by the current phase.

## Shared Types

Move shared types early enough to avoid duplicate definitions, but not so early
that they become a broad global context.

- `UiMode` -> `src/app/app_mode.h`
- `ProbeResult` -> `src/platform/startup_probe.h`
- `BatteryStatus` -> `src/platform/battery.h`
- `BacklightState` -> `src/platform/backlight_control.h`
- `AlarmSettings`, `AlarmMatch`, `AlarmFireRecord` ->
  `src/alarm/alarm_model.h`
- `AppSettings`, `SettingsRecord` -> `src/settings/settings_model.h`
- `LifeRuntime`, `LifeHourRecord` -> `src/life/life_runtime.h`
- `SetTimeModel`, `TimeField`, `SelectionMode` ->
  `src/app/set_time_editor.h`
- `AlarmEditModel`, `AlarmField`, `AlarmSelectionMode` ->
  `src/alarm/alarm_ui.h`
- `SettingsEditModel` -> `src/app/settings_editor.h`
- `AnalogHandState` -> `src/clock/analog_render.h`

Do not introduce a broad `AppContext` until Phase 9. Earlier phases should pass
only the state they actually need.

## Phase Plan

### Phase 0: Safety Baseline

Goal: make later refactors easy to verify.

Tasks:

- Record current `src/main.cpp` line count in `PROJECT_LOG.md`.
- Record current branch, git hash, and dirty state in `PROJECT_LOG.md`.
- Run `git diff --check`.
- Build once with `cmake --build build -j4`.
- Record the UF2 path and size if the build succeeds.
- Do not change behavior.

Verification:

- Build succeeds.
- Startup build ID remains present in source.
- No source files are changed in this phase unless implementation has started.

### Phase 1: Extract Pure Clock Time Helpers

Goal: remove independent date/time helpers first because they are low risk.

Move to `src/clock/clock_time.*`:

- leap year helper;
- days-in-month and days-before-month helpers;
- weekday calculation and weekday names;
- days since 2000-01-01;
- moon age calculation;
- RTC datetime validity check;
- non-drawing date/time format helpers:
  - `format_clock_lines`;
  - `format_moon_age_line`;
  - any pure helper needed by calendar rendering.

Keep DS3231 I/O in `rtc/ds3231.*`.

Detailed steps:

1. Create `clock_time.h` and `clock_time.cpp`.
2. Add `src/clock/clock_time.cpp` to `CMakeLists.txt`.
3. Build with the mostly empty module.
4. Move calendar math helpers.
5. Build.
6. Move weekday helpers.
7. Build.
8. Move datetime validity.
9. Build.
10. Move moon age.
11. Build.
12. Move non-drawing format helpers.
13. Build.

Verification:

- Build succeeds after each moved group.
- `main.cpp` no longer contains pure calendar math or moon age math.
- No hardware run is scheduled for this phase unless a hardware-risk trigger is
  introduced.

### Phase 2: Extract Startup Probe, Battery, And Backlight State

Goal: isolate hardware-adjacent support code before moving UI logic.

Move to `src/platform/startup_probe.*`:

- keyboard-controller probe;
- AT24C32 probe;
- startup probe summary struct;
- `run_startup_probes`.

Keep I2C bus initialization in `main.cpp` until Phase 9 unless another module
needs shared bus setup.

Move to `src/platform/battery.*`:

- `BatteryStatus`;
- keyboard-controller battery register read.

Move to `src/platform/backlight_control.*`:

- `BacklightState`;
- restore-level tracking;
- checked backlight write helper;
- user off/on transition helpers;
- alarm forced-on and restore helpers.

Keep `handle_backlight_key_event` in `main.cpp` until Phase 9. It contains mode
policy and key routing.

Detailed steps:

1. Extract startup probe structs and functions.
2. Build.
3. Extract `BatteryStatus` and battery read.
4. Build.
5. Extract `BacklightState` and state transition helpers.
6. Build.

Verification:

- Build succeeds.
- Static checks:
  - `platform/startup_probe.*` does not include app or feature headers;
  - `platform/battery.*` does not include clock rendering headers;
  - `platform/backlight_control.*` does not include `app_mode.h`;
  - `handle_backlight_key_event` remains outside `platform/`.
- Additional hardware check is required only if Power, Space peek, alarm
  forced-backlight, or restore timing behavior changes.

### Phase 3: Extract Settings Model And Store

Goal: separate EEPROM persistence from screens.

Move to `src/settings/settings_model.h`:

- settings constants that describe stored data;
- `AppSettings`;
- `SettingsRecord`;
- clock style constants.

Move to `src/settings/settings_store.*`:

- CRC calculation;
- default alarms and default app settings;
- alarm and app settings validation;
- settings record creation and validation;
- settings record apply;
- AT24C32 read/write helpers;
- load and save settings from EEPROM.

Detailed steps:

1. Move structs and constants to `settings_model.h`.
2. Build.
3. Move default and validation helpers.
4. Build.
5. Move CRC and record conversion helpers.
6. Build.
7. Move AT24C32 read/write helpers.
8. Build.
9. Move `load_settings_from_eeprom` and `save_settings_to_eeprom`.
10. Build.

Verification:

- Build succeeds after each moved group.
- Static checks:
  - `settings_store.*` has no display calls;
  - `settings_store.*` has no key handling;
  - EEPROM slot addresses, record size, magic, format version, and flags are
    unchanged unless a separate behavior change is explicitly planned.
- Additional hardware check is required only if EEPROM layout, defaults,
  validation, sequence selection, or AT24C32 timing changes.

### Phase 4: Extract Alarm Model, Runtime, And UI

Goal: keep alarm matching, ringing runtime, and editor UI separate.

Move to `src/alarm/alarm_model.*`:

- `AlarmSettings`;
- `AlarmMatch`;
- `AlarmFireRecord`;
- alarm matching;
- next alarm calculation;
- fired-minute tracking;
- alarm label formatting.

Move to `src/alarm/alarm_runtime.*`:

- alarm auto-stop timeout helper;
- alarm start/stop state helpers;
- non-UI alarm runtime policy that can be shared by app routing.

Move to `src/alarm/alarm_ui.*`:

- `AlarmEditModel`;
- alarm edit field helpers;
- alarm settings screen rendering;
- alarm settings key helpers;
- alarm ringing screen rendering.

Keep `alarm_sound.*` as the audio backend. Do not move audio playback into
`alarm_ui.*`.

Detailed steps:

1. Move model structs and alarm matching helpers.
2. Build.
3. Move next-alarm and label formatting helpers.
4. Build.
5. Move fired-minute tracking.
6. Build.
7. Move alarm runtime helpers if they can be extracted without changing policy.
8. Build.
9. Move alarm editor model and field helpers.
10. Build.
11. Move alarm editor drawing.
12. Build.
13. Move alarm editor key helpers.
14. Build.
15. Move alarm ringing screen drawing.
16. Build.

Verification:

- Build succeeds after each moved group.
- Static checks:
  - `alarm_model.*` has no display calls and no audio calls;
  - `alarm_runtime.*` has no display drawing;
  - `alarm_ui.*` does not decide when an alarm should fire;
  - mode return after alarm remains in `main.cpp` or Phase 9 app routing.
- Additional hardware check is required only if alarm matching, alarm timing,
  alarm stop, auto-stop, audio start/stop, or mode-return behavior changes.

### Phase 5: Extract Clock Rendering In Four Subphases

Goal: remove the largest display surface from `main.cpp` while preserving
layout.

#### Phase 5A: Common And Digital Clock Rendering

Move to `src/clock/clock_render.*`:

- app label formatting;
- common clock frame;
- digital date/time delta drawing;
- no-seconds colon blink drawing;
- battery text formatting and battery delta drawing;
- next-alarm summary drawing used by digital clock.

Detailed steps:

1. Move common/digital constants used only by these helpers.
2. Build.
3. Move app label and clock frame helpers.
4. Build.
5. Move digital date/time delta helpers.
6. Build.
7. Move colon blink helper.
8. Build.
9. Move battery text and battery delta helpers.
10. Build.
11. Move digital next-alarm summary helper.
12. Build.

#### Phase 5B: Analog Clock Rendering

Move to `src/clock/analog_render.*`:

- analog geometry constants;
- `AnalogHandState`;
- face drawing;
- hand drawing;
- AM/PM, date, moon age, alarm, and RTC failure labels;
- `draw_analog_clock`.

Detailed steps:

1. Move analog constants and `AnalogHandState`.
2. Build.
3. Move coordinate and hand-state helpers.
4. Build.
5. Move static face and detail restore helpers.
6. Build.
7. Move hand and hub drawing helpers.
8. Build.
9. Move analog labels.
10. Build.
11. Move `draw_analog_clock`.
12. Build.

#### Phase 5C: Calendar Clock Rendering

Move to `src/clock/calendar_render.*`:

- calendar geometry constants;
- header drawing;
- weekday header drawing with red `Sun`;
- month grid and today highlight;
- calendar time, moon age, and alarm summary;
- `draw_calendar_clock`.

Detailed steps:

1. Move calendar constants.
2. Build.
3. Move calendar header helper.
4. Build.
5. Move weekday and month grid helpers.
6. Build.
7. Move today highlight behavior.
8. Build.
9. Move time, moon age, and alarm summary helpers.
10. Build.
11. Move `draw_calendar_clock`.
12. Build.

#### Phase 5D: Clock Help

Move to `src/clock/clock_help.*`:

- clock help pages;
- license summary page;
- help page count constant if needed.

Keep F10 routing and current help page state in `main.cpp` until Phase 9.

Verification for Phase 5:

- Build succeeds after each moved group.
- Static checks:
  - `main.cpp` selects which renderer to call;
  - `main.cpp` no longer contains drawing algorithms for digital, analog,
    calendar, or help screens after Phase 5D;
  - screenshot prefix remains `clk_` in screenshot capture/service code.
- Additional hardware check is required only if renderer selection policy,
  screen layout, screenshot routing, display timing, or clock-style settings
  behavior changes.

### Phase 6: Extract Set-Time And Settings Editors

Goal: move editor-local state and key handling out of the main loop.

Move to `src/app/set_time_editor.*`:

- `SetTimeModel`;
- `TimeField`;
- `SelectionMode`;
- field min/max/get/set helpers;
- preferred-day handling;
- set-time status helper;
- set-time line formatting;
- field position helper;
- set-time screen rendering;
- digit replacement and movement helpers.

Keep actual DS3231 write in `main.cpp` or app routing until Phase 9. The editor
prepares values; hardware write policy remains integration code.

Move to `src/app/settings_editor.*`:

- `SettingsEditModel`;
- settings edit model creation;
- settings screen rendering;
- settings selection movement;
- settings toggle handling.

Keep EEPROM save in `main.cpp` or app routing until Phase 9.

Detailed steps:

1. Move set-time types.
2. Build.
3. Move pure set-time field helpers.
4. Build.
5. Move set-time rendering.
6. Build.
7. Move set-time key helpers.
8. Build.
9. Move settings edit model and rendering.
10. Build.
11. Move settings edit key helpers.
12. Build.

Verification:

- Build succeeds after each moved group.
- Static checks:
  - `set_time_editor.*` does not write RTC directly;
  - `settings_editor.*` does not save EEPROM directly;
  - Enter/Esc/Brk mode routing remains integration code until Phase 9.
- Additional hardware check is required only if RTC write behavior, settings
  save behavior, or editor key semantics change.

### Phase 7: Extract Life Runtime And Rendering

Goal: keep Life as a feature module, not a special case inside `main.cpp`.

Keep board evolution in `src/life_board.*` during this refactor. Do not rename
it in these phases.

Move to `src/life/life_runtime.*`:

- `LifeRuntime`;
- `LifeHourRecord`;
- initial mode enum and names;
- seed mixing;
- random initial-mode selection;
- board initialization policy;
- start/stop/step helpers;
- hourly record tracking.

Move to `src/life/life_render.*`:

- cell drawing;
- initial board drawing;
- board diff drawing.

Detailed steps:

1. Move Life runtime structs and enum.
2. Build.
3. Move seed and initial-mode helpers.
4. Build.
5. Move board initialization policy.
6. Build.
7. Move start/stop/step/hour-record helpers.
8. Build.
9. Move cell, initial-board, and diff drawing.
10. Build.

Verification:

- Build succeeds after each moved group.
- Static checks:
  - `life_runtime.*` does not draw pixels;
  - `life_render.*` does not decide hourly/manual start policy;
  - `[L]` key routing and mode transition remain in integration code until
    Phase 9.
- Additional hardware check is required only if Life start/stop policy, hourly
  trigger timing, random initial-mode selection, or Space-stop behavior changes.

### Phase 8: Extract Screenshot Service, UART Commands, And Loop Sleep

Goal: isolate cross-cutting utility behavior before app-controller routing.

Move to `src/diagnostics/screenshot_service.*`:

- screenshot progress tone;
- capture-with-sounds orchestration;
- alarm-priority handling for screenshot sounds.

Keep low-level BMP capture and the `clk_####.BMP` filename prefix in
`src/diagnostics/screenshot_capture.*`.

Move to `src/diagnostics/uart_commands.*`:

- date/time argument parsing;
- UART help text;
- UART prompt;
- set date/time command handler;
- serial line polling.

Keep `usb_vbus_present` in `platform/` or a small app helper. UART command
code may call it through a narrow function but must not own global sleep policy.

Move to `src/app/loop_sleep.*`:

- sleep-duration calculation for active alarm, Life mode, UART/VBUS state,
  clock idle key polling, and colon blink needs.

Detailed steps:

1. Move screenshot tone and service wrapper.
2. Build.
3. Move UART parse helpers.
4. Build.
5. Move UART command help, prompt, and set command.
6. Build.
7. Move UART polling.
8. Build.
9. Move loop sleep calculation.
10. Build.

Verification:

- Build succeeds after each moved group.
- Static checks:
  - screenshot prefix remains `clk_`;
  - screenshot timestamp still uses RTC with build-time fallback through the
    existing FatFS timestamp provider;
  - UART remains at the existing stdio/UART settings;
  - VBUS and sleep policy are not silently changed.
- Additional hardware check is required only if SD/FatFS writes, screenshot
  timestamp behavior, UART command semantics, VBUS detection, or sleep/polling
  policy changes.

### Phase 9: Introduce App Controller Boundary

Goal: make future modules easier to integrate.

Create `src/app/app_mode.h` and `src/app/app_controller.*` only after enough
feature code has moved out.

`AppController` responsibilities:

- own `UiMode`;
- route keyboard events to the active feature;
- coordinate transitions among clock, clock help, set-time, alarm settings,
  settings, Life, and alarm ringing;
- own redraw flags and mode-local state that remain after extraction;
- keep the main loop readable.

`main.cpp` remains responsible for:

- SDK/hardware startup;
- build ID print;
- I2C/display/audio/keyboard/SD initialization;
- loading settings;
- constructing app state;
- calling the app controller loop.

Minimum starting interface:

```cpp
class AppController {
public:
    void init();
    void tick(uint32_t now_ms);
    void handle_key(const picoment::keyboard::KeyEvent& event);
    UiMode mode() const;
};
```

Do not force this exact API if the extracted code shows a smaller interface is
better, but do not introduce a broad catch-all context before it is needed.

Detailed steps:

1. Move `UiMode` to `app_mode.h`.
2. Build.
3. Create an `AppController` shell with no behavior change.
4. Build.
5. Route clock mode key handling through it.
6. Build.
7. Route clock help through it.
8. Build.
9. Route set-time through it.
10. Build.
11. Route alarm settings through it.
12. Build.
13. Route settings through it.
14. Build.
15. Route Life through it.
16. Build.
17. Route alarm ringing through it.
18. Build.
19. Move redraw flags and mode-local state only after the corresponding route
    is already building.

Pre-controller readiness gate:

- `git diff --check` passes.
- All previous phase static checks still pass.
- New low-volume logs needed for final smoke verification have been added.
- A final `docs/SMOKE_TEST_PLAN.md` exists or `PROJECT_LOG.md` contains an
  equivalent final smoke section.
- The final smoke section states firmware path, expected build id source,
  expected log lines, timeouts, pass/fail rules, and any remaining visual
  checks.

Verification:

- Build succeeds after each routed mode.
- `main.cpp` target size is checked after Phase 9.
- If `main.cpp` remains above 1200 lines, identify remaining feature logic and
  add a follow-up phase before calling the refactor complete.

## Verification Cadence

Build verification is required after every moved function group. Hardware
verification is planned, but it is expensive and must be used only when it
answers something that build output, static checks, and logs cannot answer.

Hardware validation cost model:

- A hardware run costs a UF2 flash, reboot, setup, user attention, possible
  SD-card handling, serial capture, and result interpretation.
- This is a behavior-preserving refactor, so repeated manual hardware checks
  after every source move are not reasonable.
- The best hardware validation is one final run whose serial log contains
  enough information to decide pass/fail.
- Necessary hardware validation must not be skipped. If hardware-dependent
  behavior changes, add a narrow log-driven check at the point where it keeps
  the failure range smallest.

Default hardware checks:

- one final log-driven smoke run after Phase 9.

Additional hardware checks are allowed only when a hardware-risk trigger is
present. A trigger is present if a phase changes any of these behaviors, not
merely moves existing code:

- EEPROM record layout, default values, persistence, sequence choice, or
  AT24C32 access timing;
- RTC reads, DS3231 writes, FatFS timestamp generation, SD writes, or screenshot
  file creation;
- backlight, Power key, Space peek, alarm forced-backlight, or restore timing;
- alarm matching, alarm firing, alarm stop, auto-stop, or mode-return
  semantics;
- keyboard polling, VBUS detection, UART polling, or sleep/power policy;
- display initialization, display bus behavior, renderer selection, or clock
  style selection;
- Life hourly trigger, manual `L` start, random initial mode, stability stop,
  timeout stop, or Space stop;
- any hardware-dependent behavior that cannot be judged from build output,
  static checks, and existing logs.

When a trigger is present:

1. Do not run a broad regression pass.
2. Add or confirm low-volume logs for the specific risk.
3. Record the check in `PROJECT_LOG.md` before asking for hardware work.
4. Run the smallest hardware check that answers the question.
5. Record the log filename, build id, actual result, next step, and failure not
   to repeat.

## Log-Driven Smoke Design

The final smoke run must prefer log assertions over manual observation. Add
low-volume diagnostic log lines before the final UF2 is handed to the user if
an important behavior is not visible in existing logs.

Before the final hardware run, create `docs/SMOKE_TEST_PLAN.md` or an
equivalent `PROJECT_LOG.md` section with:

- firmware path and expected build id source;
- exact steps in execution order;
- whether the run uses diagnostic commands or manual keys;
- expected log line patterns for each step;
- timeout or count rules;
- pass/fail decision rules;
- remaining visual checks and why logs cannot decide them.

Minimum final smoke-run log checks:

- startup emits version, git hash, dirty state, build time, and build purpose
  exactly once within 3 seconds;
- startup probe emits RTC, EEPROM, keyboard, battery, and VBUS summary within
  3 seconds;
- settings load emits one summary line with clock style, seconds, Life ON/OFF,
  and alarm fields within 3 seconds;
- mode transitions emit a line for clock help, set-time, alarm settings,
  settings, Life, and alarm ringing;
- clock renderer selection emits one line for digital, analog, calendar, and
  temporary calendar peek during the smoke sequence;
- screenshot begin/end emits path, prefix `clk_`, timestamp source, and result;
- Power/backlight actions emit off/on/peek/alarm-forced/restore state lines;
- alarm test emits match/fire/stop lines and the stop reason;
- Life test emits start source, selected initial mode, generation count, and
  stop reason;
- UART/VBUS test emits whether VBUS is present and whether high-frequency UART
  polling is enabled.

Visual checks should be limited to cases where logs cannot prove the outcome:

- final clock screen layout after digital, analog, and calendar renderers;
- screenshot image appearance if screenshot file creation succeeds but display
  content is in doubt.

Do not ask for hardware confirmation after every build-only move. If a behavior
cannot be proven by build output, static checks, or existing logs, add a
specific log line first.

## Implementation Rules

- One phase may be several small commits.
- Each source move updates version and build purpose in the same work unit.
- Each added `.cpp` file updates `CMakeLists.txt` in the same commit.
- Prefer moving code first, then renaming or reshaping it later.
- Avoid formatting churn while moving code.
- Keep existing behavior names and log strings unless a planned log addition is
  needed for verification.
- If a patch fails, reread the target section and retry with smaller
  already-successful edit units.
- Do not perform multiple risky extractions before a build.
- Do not carry temporary diagnostic menus or tests into the final firmware
  unless they are explicitly part of the product.

## Suggested Commit Sequence

1. `Document Clock main refactor plan`
2. `Extract Clock time helpers`
3. `Extract Clock startup and power services`
4. `Extract Clock settings store`
5. `Extract Clock alarm modules`
6. `Extract Clock renderers`
7. `Extract Clock editors`
8. `Extract Clock Life module`
9. `Extract Clock diagnostics services`
10. `Introduce Clock app controller`
11. `Prepare Clock refactor smoke plan`
12. `Finalize Clock source organization`

Split any commit further if the diff becomes hard to review.

## Expected End State

After the refactor, `src/main.cpp` should contain mostly startup, service
construction, and the top-level loop. A reasonable target is under 800 to 1200
lines.

The important success condition is not the exact line count. The success
condition is that adding a new feature means adding or editing a feature module
and app-controller routing, not expanding `main.cpp` with another independent
subsystem.

## Implementation-Ready Checklist

The plan is implementation-ready when all items below are true:

- each phase has a clear file destination;
- each phase has build points before unrelated behavior is moved;
- each new `.cpp` file has an explicit CMake update requirement;
- Clock-specific behavior is protected from accidental ClockCalc copying;
- hardware-risk triggers are defined so necessary hardware checks are not
  skipped;
- broad hardware validation is reserved for a documented log-driven smoke run;
- expected smoke-run log lines, timeouts, and pass/fail rules are documented
  before the final UF2 is handed to the user;
- `main.cpp` has a defined target role after each major extraction;
- app-controller introduction is delayed until feature code has already been
  reduced enough to make the controller understandable;
- future features have a clear integration rule: add a feature module and app
  routing, not a new subsystem inside `main.cpp`.
