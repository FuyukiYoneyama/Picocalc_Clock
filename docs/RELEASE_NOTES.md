# Release Notes

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
