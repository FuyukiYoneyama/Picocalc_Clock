# Release Notes

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

