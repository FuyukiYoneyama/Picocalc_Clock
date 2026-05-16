# Picocalc_Clock Implementation Plan

この文書は `Picocalc_Clock` を実装するための詳細計画である。
`README.md` を概要、本文書を作業手順と確認項目として扱う。

## 目標

PicoCalc 上で DS3231 RTC の現在時刻を画面表示し、時刻設定とアラームを操作できる時計アプリを作る。

初期 MVP は次の通り。

- Pico SDK の単独プロジェクトとしてビルドできる。
- C++ project として作り、`Picocalc_ment` の C++ API を素直に使える構成にする。
- `i2c1` / `GP6` / `GP7` で DS3231 にアクセスする。
- I2C speed は実機実験で決めるが、keyboard 側の要件を基本にする。
- DS3231 `0x68` が見つからない場合は画面にエラーを表示する。
- DS3231 から現在時刻を読み出す。
- PicoCalc 画面に年月日、曜日、時分秒を表示する。
- 1 秒ごとに表示を更新する。

## 入力資料

| Source | Purpose |
| ------ | ------- |
| `<local-rtc-workspace>/PicoCalc_RTC_Clock_Specification.docx` | 時計アプリ仕様 |
| `<local-rtc-workspace>/Picocalc_RTCtest` | RTC 接続確認済みコード |
| `<local-rtc-workspace>/Picocalc_RTCtest/STATUS.md` | RTC 実機確認履歴 |
| `<local-rtc-workspace>/Picocalc_RTCtest/ds3231.c` | 移植候補の DS3231 ドライバ |
| `<local-rtc-workspace>/Picocalc_RTCtest/ds3231.h` | 移植候補の DS3231 API |
| `<local-synth-workspace>/Picocalc_ment/src/font/cozette_font.h` | 時計画面フォント候補 |
| `<local-synth-workspace>/Picocalc_ment/src/font/overlay_font.h` | Help / About / License 画面フォント候補 |
| `<local-synth-workspace>/Picocalc_ment/src/font/spleen_native_fonts.h` | 診断・サイズ比較用フォント候補 |
| `<local-synth-workspace>/Picocalc_ment/THIRD_PARTY_NOTICES.md` | フォント notice 参照元 |
| `<local-synth-workspace>/Picocalc_ment/src/platform/picocalc_display.h` | LCD API 候補 |
| `<local-synth-workspace>/Picocalc_ment/src/platform/picocalc_keyboard.h` | keyboard API 候補 |
| `<local-synth-workspace>/Picocalc_ment/src/platform/picocalc_audio_pwm.h` | PWM audio API 候補 |
| `<local-synth-workspace>/Picocalc_ment/src/config/log_config.h` | log 抑制設計候補 |
| `<local-nes-workspace>/Picocalc_NESco/platform/rom_image.c` | Flash erase / program lockout 参考 |
| `<local-nes-workspace>/Picocalc_NESco/drivers/boko_flash_trace.c` | Flash trace 書き込み参考 |
| `<local-nes-workspace>/Picocalc_NESco/LICENSE` | NESco 側ライセンス境界 |

## 設計原則

- `Picocalc_RTCtest` は検証ツールとして残し、時計アプリの作業で破壊しない。
- `Picocalc_Clock` は C++ project とし、PicoCalc 固有 API を C wrapper で無理に包まない。
- `Picocalc_ment` の LCD / keyboard / audio 実装は、サブディレクトリ参照ではなく、必要ファイルを `Picocalc_Clock` 側へコピーして使う。
- `Picocalc_ment` 由来 platform API は `ui.cpp`、`keymap.h`、`alarm_sound.cpp` の内側に閉じ込める。
- RTC ドライバは表示処理やキー入力に依存させない。
- UI は PicoCalc 固有処理として `ui.cpp` / `ui.h` に閉じ込める。
- アラーム判定は `alarm.cpp` / `alarm.h` に閉じ込める。
- 設定保存は `settings.cpp` / `settings.h` に閉じ込め、保存先を後で変更できるようにする。
- フォントは `Picocalc_ment` のフォント実装を使い、第三者 notice を維持する。
- `Picocalc_Clock` 本体は MIT License とする。
- 実機確認に持ち込むソース変更では `version.h` を更新する。
- 未確認の内容は確認済み情報と区別して記録する。

## Project Layout

```text
Picocalc_Clock/
  CMakeLists.txt
  pico_sdk_import.cmake
  main.cpp
  version.h
  ds3231.c
  ds3231.h
  clock_app.cpp
  clock_app.h
  ui.cpp
  ui.h
  keymap.h
  alarm.cpp
  alarm.h
  alarm_sound.cpp
  alarm_sound.h
  settings.cpp
  settings.h
  platform/
    picocalc_display.cpp
    picocalc_display.h
    picocalc_keyboard.cpp
    picocalc_keyboard.h
    picocalc_key_table.h
    picocalc_audio_pwm.cpp
    picocalc_audio_pwm.h
    picocalc_uart_log.cpp
    picocalc_uart_log.h
    lcd_spi_min.pio
  config/
    board_config.h
    build_config.h
    log_config.h
  font/
    cozette_font.h
    overlay_font.h
    spleen_native_fonts.h
  README.md
  IMPLEMENTATION_PLAN.md
  LICENSE
  THIRD_PARTY_NOTICES.md
  LICENSE_REVIEW.md
```

## CMake 方針

`Picocalc_RTCtest` の成功済み Pico SDK 構成を第一候補として再利用する。
LCD / keyboard / audio は `Picocalc_ment` の C++ API を第一候補にするため、
project は C++ 対応を前提にする。DS3231 ドライバは C のまま移植してよい。
`Picocalc_ment` の source は build 時に sibling directory を参照せず、必要ファイルを
`Picocalc_Clock` 内へコピーしてから target source に加える。

予定設定:

- project: `Picocalc_Clock`
- C standard: `11`
- CXX standard: `17`
- libraries:
  - `pico_stdlib`
  - `hardware_i2c`
  - `pico_multicore`
  - `hardware_clocks`
  - `hardware_dma`
  - `hardware_gpio`
  - `hardware_irq`
  - `hardware_pio`
  - `hardware_pwm`
  - `hardware_spi`
- stdio:
  - 開発初期は UART 有効
  - USB stdio は PicoCalc 実機運用では使わない方針
- UART:
  - `uart0`
  - TX `GP0`
  - RX `GP1`
  - baudrate は `115200 bps` とする。

`Picocalc_ment` 由来の source 候補:

- `platform/picocalc_display.cpp`
- `platform/picocalc_keyboard.cpp`
- `platform/picocalc_key_table.h`
- `platform/picocalc_audio_pwm.cpp`
- `platform/picocalc_uart_log.cpp`
- `platform/lcd_spi_min.pio`

`add_executable()` / `target_sources()` は、Phase ごとに存在するファイルだけを登録する。
Phase 1 で最終形の source 一式を先に CMake へ入れない。まだコピーしていない
`platform/` や `font/` のファイルを参照すると、最小ビルドがそこで止まるため。

Phase 1 の最小 `add_executable()`:

```cmake
add_executable(Picocalc_Clock
    main.cpp
)
```

Phase 2 で RTC driver を追加する:

```cmake
target_sources(Picocalc_Clock PRIVATE
    ds3231.c
)
```

Phase 3 で LCD / keyboard / font / audio の取り込み後に追加する:

```cmake
target_sources(Picocalc_Clock PRIVATE
    ui.cpp
    platform/picocalc_display.cpp
    platform/picocalc_keyboard.cpp
    platform/picocalc_audio_pwm.cpp
    platform/picocalc_uart_log.cpp
)
```

以降の application source は、作成する Phase で追加する:

- Phase 4: `clock_app.cpp`
- Phase 6: `alarm.cpp`
- Phase 7: `alarm_sound.cpp`
- Phase 8: `settings.cpp`

CMake で明示すること:

- `project(Picocalc_Clock C CXX ASM)`
- `set(CMAKE_C_STANDARD 11)`
- `set(CMAKE_CXX_STANDARD 17)`
- `pico_generate_pio_header(Picocalc_Clock ${CMAKE_CURRENT_SOURCE_DIR}/platform/lcd_spi_min.pio)`
  - Phase 3 で `platform/lcd_spi_min.pio` をコピーしてから有効にする。
  - Phase 1 / Phase 2 ではまだ有効にしない。
- include directories:
  - `${CMAKE_CURRENT_SOURCE_DIR}`
  - `${CMAKE_CURRENT_SOURCE_DIR}/platform`
  - `${CMAKE_CURRENT_SOURCE_DIR}/config`
  - `${CMAKE_CURRENT_SOURCE_DIR}/font`
- compile definitions:
  - `PICO_DEFAULT_UART_BAUD_RATE=115200`
  - `PICOCLOCK_KEY_LCD_ENABLED=1`
  - `PICOCLOCK_BUILD_RELEASE` / `PICOCLOCK_BUILD_DEBUG` / `PICOCLOCK_BUILD_MEASURE` のいずれか 1 つ
  - `PICOCLOCK_LOG_ENABLED`
  - `PICOCLOCK_COMMAND_UART_ENABLED`
- stdio:
  - `pico_enable_stdio_usb(Picocalc_Clock 0)`
  - `pico_enable_stdio_uart(Picocalc_Clock 1)`
- extra outputs:
  - `pico_add_extra_outputs(Picocalc_Clock)`

`PICOMENT_*` macro の扱い:

- コピー直後に public な設定 macro は `PICOCLOCK_*` へ置換する。
- `PICOMENT_BUILD_RELEASE` / `PICOMENT_BUILD_DEBUG` / `PICOMENT_BUILD_MEASURE` は残さない。
- `PICOMENT_LOG_ENABLED` / `PICOMENT_METRICS_ENABLED` 相当は `PICOCLOCK_LOG_ENABLED` / `PICOCLOCK_METRICS_ENABLED` に置換する。
- `PM_LOG_BOOT` / `PM_LOG_MAIN` の macro 名はログ呼び出しとして残してよいが、内部では `PICOCLOCK_LOG_ENABLED` を見る。
- 互換 macro は使わない。コピー後のビルドエラーは `PICOCLOCK_*` へ寄せて直す。

## Hardware Constants

RTC 接続 pin と I2C port は `Picocalc_RTCtest` の確認済み設定を使う。
I2C speed は keyboard 側の要件を優先し、`400000 Hz` を第一候補として実機検証する。

```c
#define CLOCK_RTC_I2C_PORT i2c1
#define CLOCK_RTC_I2C_SDA_PIN 6
#define CLOCK_RTC_I2C_SCL_PIN 7
#define CLOCK_I2C_SPEED_HZ 400000
```

DS3231:

- 7-bit address: `0x68`
- status register: `0x0F`
- OSF bit: `0x80`

`Picocalc_ment` で確認した PicoCalc board constants:

- LCD SPI pins: SCK `GP10`, MOSI `GP11`, MISO `GP12`, CS `GP13`, DC `GP14`, RST `GP15`, RAM CS `GP21`
- Keyboard I2C: `i2c1`, SDA `GP6`, SCL `GP7`, address `0x1F`
- PWM audio: left `GP26`, right `GP27`, wrap `255`
- UART baudrate: `921600`

注意:

- RTC と keyboard は同じ `i2c1` / `GP6` / `GP7` バスを使う。
- RTC 試験では `10000 Hz`、`Picocalc_ment` keyboard では `400000 Hz` が確認されている。
- 時計アプリでの共有 I2C speed は keyboard 側の `400000 Hz` を第一候補にし、DS3231 / AT24C32 と同じバスで実機確認する。
- `400000 Hz` で DS3231 または AT24C32 が不安定な場合だけ、速度を落として再評価する。

## DS3231 Driver Plan

`Picocalc_RTCtest/ds3231.h` と `Picocalc_RTCtest/ds3231.c` を移植の第一候補にする。

移植時に維持する API:

```c
bool ds3231_read_time(i2c_inst_t *i2c, ds3231_datetime_t *dt);
bool ds3231_write_time(i2c_inst_t *i2c, const ds3231_datetime_t *dt);
bool ds3231_read_status(i2c_inst_t *i2c, uint8_t *status);
bool ds3231_clear_osf(i2c_inst_t *i2c);
```

移植時に追加する改善:

- 月別日数チェック。
- うるう年チェック。
- 曜日計算 helper を追加し、表示曜日は年月日からアプリ側で計算する。
- DS3231 の `day_of_week` は表示の主情報として使わない。
- RTC へ時刻を書き込むときは、計算した曜日を DS3231 day register にも書く。
- DS3231 day register の対応は `1=Mon, 2=Tue, ... 7=Sun` とする。
- RTC から読み出した day register が計算曜日と一致しない場合、表示は計算曜日を優先し、debug log で差分を確認できるようにする。

`status` 表示で OSF を自動 clear しない方針は維持する。
OSF clear は明示操作だけで行う。

## UI Plan

最初は画面表示だけを実装する。

確認済み API 候補:

- `picoment::display::init()`
- `picoment::display::clear()`
- `picoment::display::fill_rect()`
- `picoment::display::draw_text_band()`
- `picoment::display::draw_text_large_band()`
- `picoment::display::draw_status_line()`
- `picoment::keyboard::init()`
- `picoment::keyboard::read_event()`
- `picoment::keyboard::KeyEvent`
- `picoment::keyboard::KeyState`

フォント:

- 時計画面は `Picocalc_ment/src/font/cozette_font.h` を第一候補にする。
- Help / About / License のような長文画面は `overlay_font.h` または `spleen_native_fonts.h` を候補にする。
- 可能な範囲でフリーフォントも追加調査し、ライセンス、サイズ、描画負荷を確認して採用候補に入れる。
- フォントをコピーまたは派生データとして取り込む場合は、`THIRD_PARTY_NOTICES.md` に出典とライセンスを記録する。

通常画面:

```text
PicoCalc Clock
YYYY-MM-DD Ddd
HH:MM:SS
Alarm HH:MM ON
```

RTC 未検出画面:

```text
RTC not found
Check I2C wiring
SDA: GP6
SCL: GP7
```

メニュー:

```text
1. Set Time
2. Set Alarm
3. Alarm ON/OFF
4. About
```

キー操作:

| Key | Behavior |
| --- | -------- |
| `Enter` | 決定 |
| `Esc` | 戻る |
| `Up` / `Down` | 項目選択 |
| `Left` / `Right` | 値変更 |
| 数字キー | 時刻入力 |
| `Space` | アラーム ON/OFF、鳴動中はアラーム停止 |

キーコード対応:

| App Key | `picoment::keys` value | Raw value |
| ------- | ---------------------- | --------- |
| `Enter` | `Enter` | `0x0a` |
| `Esc` | `Escape` | `0xb1` |
| `Up` | `Up` | `0xb5` |
| `Down` | `Down` | `0xb6` |
| `Left` | `Left` | `0xb4` |
| `Right` | `Right` | `0xb7` |
| `Space` | `Space` | `0x20` |
| `0` - `9` | ASCII digit | `0x30` - `0x39` |

Phase 3 では `keymap.h` を作り、`picoment::keyboard::KeyEvent.key` から
`ClockKey` enum へ変換する。アプリ本体は raw key code を直接扱わない。

## Alarm Plan

初期アラームは「毎日同じ時刻に鳴る」だけにする。

設定構造体案:

```c
typedef struct {
    bool enabled;
    uint8_t hour;
    uint8_t minute;
} alarm_settings_t;
```

判定:

- `enabled == true`
- RTC の `hour` と `minute` が一致
- 同じ分で連続発火しないよう、最後に鳴らした日付と分を保持する

停止:

- 初期実装では `Space` で止める。
- 将来、実機操作感に応じて `Enter` / `Esc` / 任意キー停止を追加してもよい。

音:

- アラーム音は必須機能とする。
- 画面点滅または `ALARM` 表示は補助表示として扱い、音の代替完了条件にはしない。
- `alarm_sound.cpp` / `alarm_sound.h` を追加し、PWM audio へのサンプル供給をアプリ本体から分離する。

確認済み PWM audio API 候補:

- `picoment::audio_pwm::init_stream()`
- `picoment::audio_pwm::start_stream()`
- `picoment::audio_pwm::write_sample()`
- `picoment::audio_pwm::writable_samples()`
- `picoment::audio_pwm::stats()`

`Picocalc_ment` には screenshot capture build 用に `play_ui_tone()` があるが、
`PICOMENT_SCREENSHOT_CAPTURE_BUILD` 条件付きの API なので、そのまま通常アラーム音 API として扱わない。

最小アラーム音仕様:

- 初期音色は square wave とする。
- 周波数は `880 Hz` を第一候補にする。
- 鳴動パターンは `200 ms ON / 200 ms OFF` の繰り返しにする。
- 振幅は `uint8_t amplitude = 48` を第一候補にし、実機で耳障りな場合に下げる。
- sample rate は `48000 Hz` とする。
- square wave は 32-bit phase accumulator で生成する。
- phase increment は `(frequency_hz << 32) / 48000` 相当で求める。
- phase の最上位 bit が `0` なら `+amplitude`、`1` なら `-amplitude` とし、16-bit PCM sample へ変換する。
- 16-bit PCM sample は `sample = +/- amplitude * 256` を第一候補にし、left / right 同値で `write_sample(sample, sample)` へ渡す。
- OFF 区間は `write_sample(0, 0)` を供給する。
- `picoment::audio_pwm::init_stream()` と `start_stream()` は起動時に 1 回だけ呼ぶ。
- メインループでは `alarm_sound_service(now_ms)` を周期的に呼び、`writable_samples()` を見ながら不足分だけ `write_sample(left, right)` で供給する。
- 鳴動中も keyboard polling を止めず、`Space` 入力を優先して停止する。
- underrun は `stats().underrun_count` で確認し、debug build でのみログに出す。

`alarm_sound.h` の最小 API:

```cpp
void alarm_sound_init();
void alarm_sound_start(uint32_t now_ms);
void alarm_sound_stop();
void alarm_sound_service(uint32_t now_ms);
bool alarm_sound_active();
```

## Settings Plan

段階的に実装する。

1. 起動中 RAM のみ。
2. Pico 側 Flash 保存。
3. AT24C32 EEPROM 保存。

初期設定:

```c
alarm_settings_t alarm = {
    .enabled = false,
    .hour = 7,
    .minute = 30,
};
```

設定保存で扱う候補:

- アラーム ON/OFF
- アラーム時刻
- 12h / 24h 表示設定
- 最終表示モード

保存データ形式:

Flash と AT24C32 EEPROM は同じ record 形式を使う。
record は little-endian とし、padding に未初期化値を残さない。

```c
#define PICOCLOCK_SETTINGS_MAGIC 0x4b4c4350u  // "PCLK"
#define PICOCLOCK_SETTINGS_VERSION 1u
#define PICOCLOCK_SETTINGS_RECORD_SIZE 64u

typedef struct {
    uint32_t magic;
    uint16_t version;
    uint16_t size;
    uint32_t sequence;
    uint8_t alarm_enabled;
    uint8_t alarm_hour;
    uint8_t alarm_minute;
    uint8_t time_format_24h;
    uint8_t last_display_mode;
    uint8_t reserved[43];
    uint32_t crc32;
} settings_record_t;

static_assert(sizeof(settings_record_t) == PICOCLOCK_SETTINGS_RECORD_SIZE,
              "settings_record_t must remain 64 bytes");
```

valid 判定:

- `magic == PICOCLOCK_SETTINGS_MAGIC`
- `version == PICOCLOCK_SETTINGS_VERSION`
- `size == PICOCLOCK_SETTINGS_RECORD_SIZE`
- `alarm_hour <= 23`
- `alarm_minute <= 59`
- `crc32` が一致する

CRC32:

- 対象範囲は `settings_record_t` の先頭から `crc32` 直前まで。
- `crc32` field 自体は CRC 対象に含めない。
- 保存前に `reserved` をすべて `0` で埋める。

slot 選択:

- slot A / B の両方を読んで valid record を探す。
- 両方 valid の場合は `sequence` が新しい方を採用する。
- `sequence` は保存成功ごとに `+1` する。
- どちらも invalid の場合は初期設定を使う。

初期の保存先:

- 中間段階では Pico 側 Flash を使う。
- 最終目標は AT24C32 EEPROM 保存とする。

Flash 保存の注意:

- 設定変更時だけ保存する。
- 同じ内容の場合は書き込まない。
- Flash 領域は flash 末尾の 2 sector を settings 用に予約する。
- 予約サイズは `PICOCLOCK_FLASH_SETTINGS_SIZE = 2 * FLASH_SECTOR_SIZE` とする。
- offset は `PICO_FLASH_SIZE_BYTES - PICOCLOCK_FLASH_SETTINGS_SIZE` を第一候補にする。
- 2 sector を slot A / slot B として使い、sequence number が新しい valid record を採用する。
- `CMakeLists.txt` または build 後 check で firmware image が settings offset を越えないことを確認する。
- UF2 更新では settings 用 sector を image に含めない。設定が消えた場合も EEPROM または初期設定へ復旧できるようにする。
- erase block と書き込み回数対策は Pico SDK の `FLASH_SECTOR_SIZE` / `FLASH_PAGE_SIZE` と既存プロジェクト例を確認して決める。
- `Picocalc_NESco/platform/rom_image.c` では `flash_range_erase()` / `flash_range_program()` を `multicore_lockout_start_blocking()` / `multicore_lockout_end_blocking()` と `save_and_disable_interrupts()` / `restore_interrupts()` で囲む実装例がある。
- `Picocalc_NESco/platform/core1_worker.c` では core1 側に `multicore_lockout_victim_init()` を入れる実装例がある。
- `Picocalc_NESco/drivers/boko_flash_trace.c` では `FLASH_SECTOR_SIZE`、`XIP_BASE`、`flash_range_erase()`、`flash_range_program()` の使用例がある。

AT24C32 EEPROM 保存の注意:

- データシート確認済み事項として、容量は `4096 x 8`、word address は 2 byte、page write は 32 byte 上限、write cycle は最大 `10 ms` を見込む。
- EEPROM I2C address は既存確認済みの `0x57` を第一候補にする。
- EEPROM address `0x0000` - `0x003f` を slot A、`0x0040` - `0x007f` を slot B とする。
- 各 slot は `settings_record_t` 64 byte で、32 byte page 2 回に分けて書く。
- page write は page 境界をまたがない。
- write 手順:
  1. 2 byte word address と最大 32 byte data を送る。
  2. write cycle 完了まで ack polling する。
  3. ack polling は最大 `10 ms` まで待つ。
  4. 書き込み後に同じ address から read back し、record 全体を verify する。
- ack polling は対象 device address へ 0 byte 相当または current address read を試し、NACK 中は短く待って再試行する。
- EEPROM 保存失敗時は Pico Flash 保存または起動中 RAM 設定を維持し、設定破損として扱わない。
- EEPROM load は Flash load より優先する。EEPROM に valid record がない場合だけ Flash slot を見る。

## Log / Build Profile Plan

`Picocalc_ment` の `build_config.h` / `log_config.h` では次の方針が確認できた。

- release build では log / metrics を `0` にする。
- debug / measure build では log / metrics を `1` にする。
- `PM_LOG_BOOT` / `PM_LOG_MAIN` は log 無効時に `((void)0)` へ落ちる。

`Picocalc_Clock` でも同じ構造を候補にする。

使用する build / log macro:

- `PICOCLOCK_BUILD_RELEASE`
- `PICOCLOCK_BUILD_DEBUG`
- `PICOCLOCK_BUILD_MEASURE`
- `PICOCLOCK_LOG_ENABLED`
- `PICOCLOCK_METRICS_ENABLED`
- `PICOCLOCK_COMMAND_UART_ENABLED`

build profile:

- `PICOCLOCK_BUILD_RELEASE` / `PICOCLOCK_BUILD_DEBUG` / `PICOCLOCK_BUILD_MEASURE` は必ず 1 つだけ有効にする。
- default は `PICOCLOCK_BUILD_RELEASE` とする。
- Debug / Measure へ切り替える場合も `version.h` は通常どおり更新する。

release 相当で残す UART 出力:

- 起動時の build ID 1 行。
- UART コマンドに対する応答。

release 相当で抑制する UART 出力:

- 周期 heartbeat。
- 毎秒の時計更新ログ。
- I2C read 成功ログ。
- アラーム監視ループの周期ログ。

実機確認用ログ:

- 実機確認は、垂れ流しログではなく、起動時の自己診断サマリと UART コマンド応答で行う。
- 起動時には build ID の後に、1 回だけ `BOOT CHECK` サマリを出す。
- `BOOT CHECK` では、その firmware に実装済みの項目だけを `PASS` / `FAIL` / `SKIP` で出す。
- 自動判定できる項目は `PASS` / `FAIL` / `SKIP` で出す。
- LCD の目視確認、font の見え方、keyboard のキー入力確認のような手動確認項目は `MANUAL` とし、対応する診断コマンド名を表示する。
- release 相当でも、ユーザーが明示的に実行した診断コマンドの応答は残してよい。
- 毎秒ログ、周期 polling ログ、I2C 成功ログの連続出力は行わない。

起動時 `BOOT CHECK` の候補:

```text
Picocalc_Clock version 0.x build ...
BOOT CHECK
  uart: PASS 115200
  i2c: PASS i2c1 sda=GP6 scl=GP7 speed=400000
  rtc: PASS addr=0x68 time=YYYY-MM-DD HH:MM:SS osf=0
  eeprom_probe: PASS addr=0x57
  lcd_init: PASS
  lcd_visual: MANUAL command=lcd-test
  keyboard_init: PASS addr=0x1F
  keyboard_input: MANUAL command=key-test
  font_visual: MANUAL command=lcd-test fonts=cozette,spleen
  audio: SKIP
  flash_settings: SKIP
  eeprom_settings: SKIP
END BOOT CHECK
```

UART 診断コマンド候補:

| Command | Purpose |
| ------- | ------- |
| `diag` | 実装済み機能の状態をまとめて表示する |
| `i2c-scan` | `0x1F` / `0x57` / `0x68` を含む I2C device probe 結果を表示する |
| `rtc` | DS3231 status、OSF、現在時刻、計算曜日を表示する |
| `lcd-test` | 固定文字列とフォント確認画面を出す |
| `key-test` | 押されたキーの raw code と `ClockKey` を表示する |
| `alarm-test` | 短いアラーム音を鳴らし、`Space` 停止を確認する |
| `settings-test` | RAM / Flash / EEPROM の load、save、verify 結果を表示する |

診断コマンドは、1 回の実機確認で複数フェーズの実装をまとめて確認できるようにする。
失敗時は `FAIL reason=...` のように、次に見るべき箇所が分かる短い理由を付ける。

## Fixed Decisions

- Project language: C++ project とする。DS3231 ドライバは C のまま移植してよい。
- `Picocalc_ment` の LCD / keyboard / audio は必要ファイルをコピーして使い、build 時に sibling directory へ依存しない。
- Shared I2C speed: 実験で決めるが、基本は keyboard 側の `400000 Hz` を第一候補にする。
- UART baudrate: `115200 bps` とする。
- Audio: アラーム音は必須とする。
- Alarm stop key: 初期実装は `Space` 固定とする。
- Weekday: 年月日からアプリ側で計算する。
- DS3231 day register: `1=Mon ... 7=Sun` で計算曜日を書き込む。
- Font import scope: `0.3` で同梱する初期フォントは Cozette、Spleen overlay、Spleen native の 3 系統までにする。
- Settings storage target: 最終目標は AT24C32 EEPROM 保存とする。
- Pico Flash storage: flash 末尾 2 sector を slot A / B として予約する。
- Settings record: 64 byte の `settings_record_t` を Flash / EEPROM 共通形式にする。
- EEPROM storage: AT24C32 `0x57` の `0x0000` を slot A、`0x0040` を slot B にする。
- Build macros: `PICOCLOCK_BUILD_*` / `PICOCLOCK_LOG_ENABLED` / `PICOCLOCK_COMMAND_UART_ENABLED` を使う。
- Release UART: 起動時 build ID と UART コマンド応答だけを残す。
- `1.0`: AT24C32 EEPROM から設定復元できることを完成条件に含める。
- License review: `Picocalc_NESco` から流用する場合、対象ファイルごとの確認結果を `LICENSE_REVIEW.md` に記録する。

## Decisions To Make

- I2C speed は `400000 Hz` で RTC / keyboard / AT24C32 が同時に安定するか。安定しない場合の fallback speed をどうするか。
- アラーム音の周波数、振幅、鳴動パターンを実機でどう調整するか。
- 追加フリーフォントのライセンス条件をどこまで許容するか。

## Implementation Order

### Phase 0: 計画作成

- `Picocalc_Clock/README.md` を作成する。
- `Picocalc_Clock/IMPLEMENTATION_PLAN.md` を作成する。

完了条件:

- 新規フォルダが存在する。
- README と計画書が存在する。

### Phase 1: Pico SDK 最小プロジェクト

- `pico_sdk_import.cmake` を `Picocalc_RTCtest` からコピーする。
- `CMakeLists.txt` を作成する。
- `version.h` を `0.1` で作成する。
- `main.cpp` に起動ログと最小 UART 診断を実装する。
- UART stdio を有効にする。
- UART command parser の最小骨格を `main.cpp` に作る。
- `help` と `diag` コマンドを追加する。
- 起動時 `BOOT CHECK` の最小出力を追加する。
- `LICENSE` に MIT License を追加する。
- CMake に C / CXX / ASM、`115200 bps` UART、build profile macro、`pico_add_extra_outputs()` を入れる。
- Phase 1 の CMake source は `main.cpp` のみにする。
- `platform/`、`font/`、`ds3231.c`、`ui.cpp`、`alarm_sound.cpp` はまだ CMake に入れない。

完了条件:

- `cmake ..` が成功する。
- `make` が成功する。
- `Picocalc_Clock.uf2` が生成される。
- UART に `Picocalc_Clock version 0.1` が表示される。
- `help` コマンドに応答できる。
- `diag` コマンドで version、build profile、UART baudrate を表示できる。
- `BOOT CHECK` で未実装項目を `SKIP` として表示できる。
- `LICENSE` が存在し、MIT License が記載されている。

### Phase 2: RTC probe とドライバ移植

- `ds3231.h` / `ds3231.c` を移植する。
- `CMakeLists.txt` の `target_sources()` に `ds3231.c` を追加する。
- 日付妥当性チェックを改善する。
- `main.cpp` から DS3231 status / read を試す。
- `i2c-scan` コマンドを追加する。
- `rtc` コマンドを追加する。
- `diag` と `BOOT CHECK` に I2C、RTC、AT24C32 probe の結果を追加する。
- RTC 未検出時の状態をアプリ側で扱えるようにする。
- I2C speed `10000 Hz` と `400000 Hz` の両方で DS3231 read を試す。
- `400000 Hz` で AT24C32 address probe も試す。
- 時刻設定時、計算曜日を DS3231 day register に書き込む。

完了条件:

- DS3231 `0x68` から status を読める。
- DS3231 の日時を読める。
- `10000 Hz` と `400000 Hz` の両方で DS3231 read 結果を確認できる。
- `400000 Hz` で AT24C32 `0x57` の probe 結果を確認できる。
- `i2c-scan` で `0x1F` / `0x57` / `0x68` の probe 結果を表示できる。
- `rtc` で DS3231 status、OSF、現在時刻、計算曜日を表示できる。
- RTC 未検出時にエラー状態へ遷移できる。

### Phase 3: LCD / keyboard 調査

- PicoCalc 既存プロジェクトから LCD 表示サンプルを探す。
- PicoCalc 既存プロジェクトから keyboard 入力サンプルを探す。
- `Picocalc_ment` から LCD / keyboard / UART log / config / PIO の必要ファイルをコピーする。
- `CMakeLists.txt` に `ui.cpp` と `platform/` source を追加する。
- `clock_app.cpp`、`alarm.cpp`、`alarm_sound.cpp`、`settings.cpp` はまだ CMake に入れない。
- `platform/lcd_spi_min.pio` をコピーした後、`pico_generate_pio_header()` を有効にする。
- `Picocalc_ment` のフォント描画処理とフォント notice を確認する。
- 最小の文字表示 adapter を `ui.cpp` / `ui.h` に作る。
- `keymap.h` を作り、raw key code から `ClockKey` enum へ変換する。
- `lcd-test` コマンドを追加する。
- `key-test` コマンドを追加する。
- `diag` と `BOOT CHECK` に LCD init、keyboard init、manual check の案内を追加する。
- 必要なフォントファイルを `font/` に取り込む。
- `THIRD_PARTY_NOTICES.md` を更新する。
- コピーした `Picocalc_ment` ファイルとフォントを `LICENSE_REVIEW.md` に記録する。
- keyboard 初期化後、`400000 Hz` で keyboard event と DS3231 read が同じ実行中に成功するか確認する。

完了条件:

- 画面に固定文字列を表示できる。
- キー入力を 1 キー単位で取得できる。
- `Enter` / `Esc` / `Up` / `Down` / `Left` / `Right` / `Space` / `0` - `9` の key mapping を確認できる。
- keyboard polling 後も DS3231 read が成功する。
- `Picocalc_ment` 由来フォントで固定文字列を表示できる。
- `lcd-test` で固定文字列とフォント確認画面を表示できる。
- `key-test` で raw key code と `ClockKey` を表示できる。
- 取り込んだフォントの notice が残っている。

### Phase 4: 画面時計 MVP

- `clock_app.cpp` / `clock_app.h` を作る。
- `CMakeLists.txt` の `target_sources()` に `clock_app.cpp` を追加する。
- RTC 時刻を 1 秒ごとに読み出す。
- 通常画面に時刻を表示する。
- RTC 未検出画面を表示する。

完了条件:

- 日付と時刻が画面に表示される。
- 秒表示が 1 秒ごとに更新される。
- RTC 未検出時のエラー画面が出る。

### Phase 5: メニューと時刻設定

- メニュー画面を追加する。
- `Set Time` 画面を追加する。
- 年月日時分秒を編集できるようにする。
- `ds3231_write_time()` で RTC に書き込む。

完了条件:

- キー操作でメニューを開ける。
- 時刻設定後、RTC から読み戻した時刻が画面に出る。
- 不正日付は設定できない。

### Phase 6: アラーム設定

- `alarm.cpp` / `alarm.h` を作る。
- `CMakeLists.txt` の `target_sources()` に `alarm.cpp` を追加する。
- `Set Alarm` 画面を追加する。
- `Alarm ON/OFF` を追加する。
- 通常画面に `Alarm HH:MM ON/OFF` を表示する。

完了条件:

- アラーム時刻を変更できる。
- ON/OFF を切り替えられる。
- 設定内容が通常画面に反映される。

### Phase 7: アラーム通知

- アラーム一致判定を実装する。
- 鳴動中画面を実装する。
- `Space` キーで停止できるようにする。
- `alarm_sound.cpp` / `alarm_sound.h` を作る。
- `CMakeLists.txt` の `target_sources()` に `alarm_sound.cpp` を追加する。
- PWM audio でアラーム音を鳴らす。
- `alarm-test` コマンドを追加する。
- `diag` に alarm 設定、alarm state、audio init 状態を追加する。

完了条件:

- 指定時刻にアラーム状態へ入る。
- アラーム音が鳴る。
- `Space` キーで停止できる。
- `alarm-test` で短いアラーム音と `Space` 停止を確認できる。
- 同じ分で停止後に再発火しない。

### Phase 8: 設定保存

- `settings.cpp` / `settings.h` を作る。
- `CMakeLists.txt` の `target_sources()` に `settings.cpp` を追加する。
- まず RAM 初期値で構造を作る。
- `settings_record_t` を実装し、CRC32、valid 判定、sequence 比較を実装する。
- Pico Flash 末尾 2 sector の slot A / B 保存を実装する。
- `settings-test` コマンドに RAM / Flash の load、save、verify 結果を表示する処理を追加する。
- `diag` と `BOOT CHECK` に Flash settings の状態を追加する。
- AT24C32 保存へ移行できるよう、保存データ形式と storage backend を分離する。

完了条件:

- 起動時に初期設定を読み込める。
- 設定変更後に保存 API を呼び出せる。
- 保存後に再起動し、アラーム ON/OFF とアラーム時刻が復元される。
- firmware image が settings offset を越えていないことを build 後 check で確認できる。
- `settings-test` で RAM / Flash の load、save、verify 結果を確認できる。
- 保存失敗時の扱いを画面またはログで確認できる。

### Phase 9: AT24C32 EEPROM 保存

- AT24C32 EEPROM backend を追加する。
- EEPROM slot A `0x0000` / slot B `0x0040` を使う。
- 32 byte page 境界に合わせた書き込み処理を実装する。
- write cycle 完了待ちを実装する。
- write 後に read back verify を実装する。
- `settings-test` コマンドに EEPROM の load、save、verify、fallback 結果を追加する。
- `diag` と `BOOT CHECK` に EEPROM settings の状態を追加する。
- CRC32 不一致時は初期設定へ戻す。

完了条件:

- AT24C32 に設定を保存できる。
- 再起動後に AT24C32 から設定を復元できる。
- `settings-test` で EEPROM の load、save、verify、fallback 結果を確認できる。
- EEPROM 未検出時は Pico Flash または初期設定へ fallback できる。

## Hardware Verification Plan

実機確認は 1 回あたり 10 分以上のコストがかかるため、各 version ではなく
機能のまとまりごとに実施する。ビルド成功、静的に確認できる CMake 修正、
単体で確認できるロジックは実機確認の理由にしない。

目標回数は 4 回とする。

| Check | Version Range | Purpose | Expected |
| ----- | ------------- | ------- | -------- |
| 1 | `0.1` - `0.3` | Bring-up 確認 | `BOOT CHECK`、`diag`、`i2c-scan`、`rtc`、`lcd-test`、`key-test` で UART 起動、DS3231 read、AT24C32 probe、LCD 固定表示、key mapping、font 表示をまとめて確認できる |
| 2 | `0.4` - `0.5` | 時計表示と時刻設定確認 | `diag`、`rtc`、時刻設定操作で 1 秒更新表示と RTC read back を確認できる |
| 3 | `0.6` - `0.7` | アラーム機能確認 | `diag`、`alarm-test`、実時刻アラームで ON/OFF、時刻変更、アラーム音、`Space` 停止、同じ分での再発火防止を確認できる |
| 4 | `0.8` - `1.0` | 設定保存と初期完成確認 | `diag`、`settings-test`、再起動確認で Pico Flash 保存、AT24C32 EEPROM 保存、復元、fallback、仕様書の完成条件をまとめて確認できる |

実機確認を追加する条件:

- build は通るが、実機依存の pin / I2C / LCD / keyboard / audio / Flash / EEPROM の問題で次フェーズへ進めない場合。
- 既存の確認済み範囲を壊した可能性が高い変更を入れた場合。
- 保存処理や Flash erase / program のように、失敗時の影響が大きい変更を分離して確認したい場合。

Check 4 は原則 1 回に収める。ただし Pico Flash erase / program、AT24C32 write、
再起動後復元、fallback のどれかで失敗した場合は、原因切り分け後に保存系だけを
追加で 1 回確認してよい。

通常は上記 4 回に収める。途中 version の `0.2`、`0.3`、`0.6` などは
開発マイルストーンとして扱い、必ずしも毎回実機に持ち込まない。

## Build Procedure

予定コマンド:

```sh
cd <repo-root>
mkdir -p build
cd build
cmake ..
make
```

`PICO_SDK_PATH` が必要な場合は、`Picocalc_RTCtest` と同じ環境設定を使う。

## Acceptance Criteria

初期完成時:

- RTC から時刻取得できる。
- 画面表示できる。
- 秒単位で更新できる。
- キー操作できる。
- 時刻設定できる。
- アラーム設定できる。
- アラーム ON/OFF ができる。
- アラーム音が鳴る。
- `Space` キーでアラーム音を停止できる。
- Pico Flash に設定を保存し、再起動後に復元できる。
- AT24C32 EEPROM に設定を保存し、再起動後に復元できる。
- EEPROM 未検出時の fallback 動作を確認できる。
- `Picocalc_Clock` 本体の MIT License が明記されている。
- 取り込んだフォントの第三者 notice が残っている。

## Out Of Scope For Initial Version

- AT24C32 EEPROM の汎用読み書きツール化。
- DS3231 hardware alarm register の利用。
- SQW pin を使った割り込み。
- 複数アラーム。
- スヌーズ。
- NTP や外部時刻同期。
- 2100 年以降の century bit 対応。

## Review Notes

`Picocalc_RTCtest` のレビューで、`ds3231_write_time()` の日付チェックは `day <= 31` までであり、
月別日数やうるう年を見ていないことを確認した。

`Picocalc_Clock` へ移植するときは、最初のドライバ移植フェーズでこの点を修正する。

バージョン計画は次の順序に揃えた。

- `0.1`: Pico SDK 最小起動、UART 起動ログ。
- `0.2`: RTC probe、DS3231 status / read。
- `0.3`: LCD / key 最小確認、`Picocalc_ment` フォント表示確認。
- `0.4`: DS3231 時刻の 1 秒更新表示。
- `0.5`: メニュー骨格と時刻設定画面。
- `0.6`: アラーム設定と ON/OFF。
- `0.7`: PWM audio によるアラーム音と Space 停止。
- `0.8`: Pico Flash 設定保存。
- `0.9`: AT24C32 EEPROM 設定保存。
- `1.0`: 初期時計アプリ完成判定。
