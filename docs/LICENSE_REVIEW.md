# License Review

This document records the license status of code, font data, and reference
materials used while developing Picocalc_Clock.

## Project License

- Picocalc_Clock source code: MIT License
- License file: `LICENSE`

## Rules

- GPL-family source code is not copied into this MIT-licensed project.
- Reference-only projects are used for behavior and hardware understanding only.
- Copied files must have a confirmed compatible license.
- Bundled fonts and derived font data must be listed in
  `docs/THIRD_PARTY_NOTICES.md`.

## Reviewed Sources

| Source | File / Directory | Confirmed License | Use | Decision | Notes |
| --- | --- | --- | --- | --- | --- |
| Picocalc_ment | `src/platform/picocalc_display.cpp`, `src/platform/picocalc_display.h` | MIT | LCD implementation | Copied/adapted | File headers contain `SPDX-License-Identifier: MIT`. |
| Picocalc_ment | `src/platform/picocalc_keyboard.cpp`, `src/platform/picocalc_keyboard.h` | MIT | Keyboard implementation | Copied/adapted | File headers contain `SPDX-License-Identifier: MIT`. |
| Picocalc_ment | `src/platform/picocalc_key_table.h` | MIT | Key code constants | Copied/adapted | File header contains `SPDX-License-Identifier: MIT`. |
| Picocalc_ment | `src/platform/picocalc_uart_log.cpp`, `src/platform/picocalc_uart_log.h` | MIT | UART logging helper | Copied/adapted | File headers contain `SPDX-License-Identifier: MIT`. |
| Picocalc_ment | `src/platform/lcd_spi_min.pio` | MIT | LCD PIO program | Copied/adapted | File header contains `SPDX-License-Identifier: MIT`. |
| Picocalc_ment | `src/config/board_config.h`, `src/config/build_config.h`, `src/config/log_config.h` | MIT | Board and build configuration | Copied/adapted | File headers contain `SPDX-License-Identifier: MIT`. |
| Cozette | `src/font/cozette_font.h` | MIT | LCD text font data | Bundled | See `docs/THIRD_PARTY_NOTICES.md`. |
| Spleen | `src/font/overlay_font.h`, `src/font/spleen_native_fonts.h` | BSD 2-Clause | LCD text font data | Bundled | See `docs/THIRD_PARTY_NOTICES.md`. |
| Picocalc_NESco | project license and platform-side implementation | Mixed | Design reference | Reference only; not copied into this project | License boundary was checked before using it as a reference. |
| PicoCalc example code | keyboard and LCD examples | Not confirmed for copying | Hardware behavior reference | Reference only; not copied into this project | Used only to understand PicoCalc hardware behavior. |

## Pending Checks

- Additional fonts must be reviewed individually before bundling.
- External EEPROM example code must be reviewed individually before copying.
