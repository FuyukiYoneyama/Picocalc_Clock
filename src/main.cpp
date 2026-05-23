#include <cstdio>
#include <cstring>
#include <cstdlib>

#include "hardware/gpio.h"
#include "hardware/i2c.h"
#include "pico/stdlib.h"

#include "alarm_sound.h"
#include "ds3231.h"
#include "font/cozette_font.h"
#include "picocalc_clock_build_info.h"
#include "platform/picocalc_display.h"
#include "platform/picocalc_key_table.h"
#include "platform/picocalc_keyboard.h"
#include "version.h"

#define CLOCK_I2C_PORT i2c1
#define CLOCK_I2C_SDA_PIN 6
#define CLOCK_I2C_SCL_PIN 7
#define CLOCK_I2C_SPEED_HZ 400000
#define I2C_ADDR_KEYBOARD 0x1F
#define I2C_ADDR_AT24C32_EXPECTED 0x57
#define I2C_SCAN_TIMEOUT_US 10000
#define KBD_REG_BATTERY 0x0B

namespace {

constexpr uint16_t kBlack = 0x0000;
constexpr uint16_t kWhite = 0xffff;
constexpr uint16_t kDim = 0x7bef;
constexpr uint16_t kWarn = 0xfde0;
constexpr uint16_t kSecondHand = 0xf800;
constexpr uint16_t kHighlight = 0xff80;
constexpr uint16_t kHighlightDigit = 0x07ff;
constexpr uint16_t kHighlightText = 0x0000;
constexpr uint32_t kRtcSearchPollMs = 47;
constexpr uint32_t kRtcRestAfterTickMs = 900;
constexpr uint32_t kRtcFailRetryMs = 200;
constexpr uint32_t kMainLoopActiveSleepMs = 10;
constexpr uint32_t kMainLoopMaxSleepMs = 900;
constexpr uint32_t kUiSleepCapMs = 20;
constexpr uint32_t kAlarmLoopSleepMs = 2;
constexpr uint32_t kAlarmAutoStopMs = 60000;
constexpr uint32_t kColonBlinkMs = 1000;
constexpr uint8_t kAlarmCount = 5;
constexpr uint16_t kSettingsSlotA = 0x0000;
constexpr uint16_t kSettingsSlotB = 0x0040;
constexpr uint16_t kSettingsRecordSize = 64;
constexpr uint8_t kSettingsPageSize = 32;
constexpr uint32_t kSettingsMagic = 0x4b4c4350u;  // "PCLK"
constexpr uint16_t kSettingsVersionAlarmOnly = 2;
constexpr uint16_t kSettingsVersion = 3;
constexpr uint8_t kSettingsFlagShowSeconds = 0x01;
constexpr uint32_t kSettingsWriteCycleTimeoutMs = 20;
constexpr uint8_t kClockStyleDigital = 0;
constexpr uint8_t kClockStyleAnalog = 1;
constexpr int kDateBandX = 52;
constexpr int kDateY = 82;
constexpr int kDateBandW = 216;
constexpr int kDateH = 24;
constexpr int kTimeX = 32;
constexpr int kTimeY = 128;
constexpr int kTimeCharW = 32;
constexpr int kTimeH = 64;
constexpr int kTimeNoSecondsY = 112;
constexpr int kTimeNoSecondsCharW = 48;
constexpr int kTimeNoSecondsH = 96;
constexpr const char* kPrompt = "> ";
constexpr int kHeaderY = 12;
constexpr int kHeaderH = 18;
constexpr int kBatteryBandX = 214;
constexpr int kBatteryBandW = 96;
constexpr int kAlarmBandX = 78;
constexpr int kAlarmBandY = 218;
constexpr int kAlarmBandW = 164;
constexpr int kAlarmBandH = 24;
constexpr int kAnalogCenterX = 160;
constexpr int kAnalogCenterY = 160;
constexpr int kAnalogRadius = 96;
constexpr int kAnalogDateY = 38;
constexpr int kAnalogDateH = 24;
constexpr int kAnalogAlarmX = 40;
constexpr int kAnalogAlarmY = 274;
constexpr int kAnalogAlarmW = 240;
constexpr int kAnalogAlarmH = 24;
constexpr int kAnalogHubRadius = 4;
constexpr int kAnalogHourHandLength = kAnalogRadius * 50 / 100;
constexpr int kAnalogMinuteHandLength = kAnalogRadius * 72 / 100;
constexpr int kAnalogSecondHandLength = kAnalogRadius * 82 / 100;

static constexpr int16_t kSin60[60] = {
        0,   107,   213,   316,   416,   512,   602,   685,   761,   828,
      887,   935,   974,  1002,  1018,  1024,  1018,  1002,   974,   935,
      887,   828,   761,   685,   602,   512,   416,   316,   213,   107,
        0,  -107,  -213,  -316,  -416,  -512,  -602,  -685,  -761,  -828,
     -887,  -935,  -974, -1002, -1018, -1024, -1018, -1002,  -974,  -935,
     -887,  -828,  -761,  -685,  -602,  -512,  -416,  -316,  -213,  -107,
};

static constexpr int16_t kCos60[60] = {
     1024,  1018,  1002,   974,   935,   887,   828,   761,   685,   602,
      512,   416,   316,   213,   107,     0,  -107,  -213,  -316,  -416,
     -512,  -602,  -685,  -761,  -828,  -887,  -935,  -974, -1002, -1018,
    -1024, -1018, -1002,  -974,  -935,  -887,  -828,  -761,  -685,  -602,
     -512,  -416,  -316,  -213,  -107,     0,   107,   213,   316,   416,
      512,   602,   685,   761,   828,   887,   935,   974,  1002,  1018,
};

enum class UiMode {
    Clock,
    SetTime,
    SetAlarm,
    SetSettings,
    AlarmRinging,
};

enum class TimeField : uint8_t {
    Year = 0,
    Month,
    Day,
    Hour,
    Minute,
    Second,
};

enum class SelectionMode : uint8_t {
    Field,
    Digit,
};

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

struct ProbeResult {
    bool rtc_ok;
    bool eeprom_ok;
    bool keyboard_ok;
    uint8_t rtc_status;
};

struct BatteryStatus {
    bool ok;
    bool charging;
    uint8_t percent;
    uint8_t raw;
};

struct SetTimeModel {
    uint16_t year;
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
    uint8_t preferred_day;
    TimeField field;
    SelectionMode selection;
    uint8_t digit_index;
    char status[24];
};

struct AlarmSettings {
    bool enabled;
    uint8_t hour;
    uint8_t minute;
};

struct AppSettings {
    bool show_seconds;
    uint8_t clock_style;
};

struct SettingsRecord {
    uint32_t magic;
    uint16_t version;
    uint16_t size;
    uint32_t sequence;
    uint8_t alarm_enabled[kAlarmCount];
    uint8_t alarm_hour[kAlarmCount];
    uint8_t alarm_minute[kAlarmCount];
    uint8_t app_flags;
    uint8_t clock_style;
    uint8_t reserved[31];
    uint32_t crc32;
};

static_assert(sizeof(SettingsRecord) == kSettingsRecordSize,
              "SettingsRecord must fit one 64-byte EEPROM slot");

struct AlarmEditModel {
    AlarmSettings alarms[kAlarmCount];
    uint8_t selected_index;
    AlarmField field;
    AlarmSelectionMode selection;
    uint8_t digit_index;
    char status[32];
};

struct SettingsEditModel {
    AppSettings settings;
    uint8_t selected_index;
    char status[32];
};

struct AlarmFireRecord {
    uint16_t year;
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t minute;
    bool valid;
};

struct AlarmMatch {
    bool found;
    uint8_t first_index;
    uint8_t count;
    uint8_t hour;
    uint8_t minute;
};

struct AnalogHandState {
    bool valid;
    bool rtc_ok;
    bool show_second;
    uint8_t hour_index;
    uint8_t minute_index;
    uint8_t second_index;
};

bool time_reached(uint32_t now_ms, uint32_t target_ms) {
    return static_cast<int32_t>(now_ms - target_ms) >= 0;
}

uint32_t ms_until(uint32_t now_ms, uint32_t target_ms) {
    if (time_reached(now_ms, target_ms)) {
        return 0;
    }
    return target_ms - now_ms;
}

void print_build_id() {
    std::printf("\r\nPicocalc_Clock version %s build %s\r\n",
                PICOCALC_CLOCK_VERSION_STRING,
                PICOCALC_CLOCK_BUILD_PROFILE);
    std::printf("BUILD ID git=%s dirty=%u time=\"%s\" purpose=\"%s\"\r\n",
                PICOCALC_CLOCK_GIT_HASH,
                PICOCALC_CLOCK_GIT_DIRTY,
                PICOCALC_CLOCK_BUILD_TIME,
                PICOCALC_CLOCK_BUILD_PURPOSE);
}

void i2c_bus_init(uint32_t speed_hz) {
    i2c_init(CLOCK_I2C_PORT, speed_hz);
    gpio_set_function(CLOCK_I2C_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(CLOCK_I2C_SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(CLOCK_I2C_SDA_PIN);
    gpio_pull_up(CLOCK_I2C_SCL_PIN);
}

bool probe_keyboard_controller() {
    uint8_t reg = 0x01;
    uint8_t buf[2] = {0};
    int written = i2c_write_timeout_us(CLOCK_I2C_PORT, I2C_ADDR_KEYBOARD, &reg, 1,
                                       false, I2C_SCAN_TIMEOUT_US);
    if (written != 1) {
        return false;
    }
    int read = i2c_read_timeout_us(CLOCK_I2C_PORT, I2C_ADDR_KEYBOARD, buf, 2,
                                   false, I2C_SCAN_TIMEOUT_US);
    return read == 2;
}

bool probe_eeprom_24c32(uint8_t address) {
    uint8_t ptr[2] = {0x00, 0x00};
    int written = i2c_write_timeout_us(CLOCK_I2C_PORT, address, ptr, 2,
                                       false, I2C_SCAN_TIMEOUT_US);
    return written == 2;
}

uint32_t crc32_update(uint32_t crc, const uint8_t* data, size_t len) {
    crc = ~crc;
    for (size_t i = 0; i < len; ++i) {
        crc ^= data[i];
        for (int bit = 0; bit < 8; ++bit) {
            const uint32_t mask = 0u - (crc & 1u);
            crc = (crc >> 1) ^ (0xedb88320u & mask);
        }
    }
    return ~crc;
}

uint32_t settings_record_crc(const SettingsRecord& record) {
    return crc32_update(0u,
                        reinterpret_cast<const uint8_t*>(&record),
                        sizeof(SettingsRecord) - sizeof(record.crc32));
}

bool eeprom_read_bytes(uint16_t address, uint8_t* data, size_t len) {
    uint8_t ptr[2] = {
        static_cast<uint8_t>(address >> 8),
        static_cast<uint8_t>(address & 0xffu),
    };
    int written = i2c_write_timeout_us(CLOCK_I2C_PORT, I2C_ADDR_AT24C32_EXPECTED,
                                       ptr, 2, true, I2C_SCAN_TIMEOUT_US);
    if (written != 2) {
        return false;
    }
    int read = i2c_read_timeout_us(CLOCK_I2C_PORT, I2C_ADDR_AT24C32_EXPECTED,
                                   data, len, false, I2C_SCAN_TIMEOUT_US);
    return read == static_cast<int>(len);
}

bool eeprom_wait_ready() {
    const uint32_t start_ms = to_ms_since_boot(get_absolute_time());
    uint8_t ptr[2] = {0x00, 0x00};
    while (true) {
        int written = i2c_write_timeout_us(CLOCK_I2C_PORT, I2C_ADDR_AT24C32_EXPECTED,
                                           ptr, 2, false, I2C_SCAN_TIMEOUT_US);
        if (written == 2) {
            return true;
        }
        const uint32_t now_ms = to_ms_since_boot(get_absolute_time());
        if (time_reached(now_ms, start_ms + kSettingsWriteCycleTimeoutMs)) {
            return false;
        }
        sleep_ms(1);
    }
}

bool eeprom_write_page(uint16_t address, const uint8_t* data, size_t len) {
    if (len == 0 || len > kSettingsPageSize ||
        (address / kSettingsPageSize) !=
            ((address + static_cast<uint16_t>(len) - 1u) / kSettingsPageSize)) {
        return false;
    }

    uint8_t packet[2 + kSettingsPageSize] = {
        static_cast<uint8_t>(address >> 8),
        static_cast<uint8_t>(address & 0xffu),
    };
    std::memcpy(packet + 2, data, len);
    int written = i2c_write_timeout_us(CLOCK_I2C_PORT, I2C_ADDR_AT24C32_EXPECTED,
                                       packet, len + 2, false, I2C_SCAN_TIMEOUT_US);
    return written == static_cast<int>(len + 2) && eeprom_wait_ready();
}

bool eeprom_write_record(uint16_t slot_address, const SettingsRecord& record) {
    const uint8_t* bytes = reinterpret_cast<const uint8_t*>(&record);
    for (uint16_t offset = 0; offset < kSettingsRecordSize;
         offset += kSettingsPageSize) {
        if (!eeprom_write_page(slot_address + offset, bytes + offset,
                               kSettingsPageSize)) {
            return false;
        }
    }

    SettingsRecord verify = {};
    return eeprom_read_bytes(slot_address,
                             reinterpret_cast<uint8_t*>(&verify),
                             sizeof(verify)) &&
           std::memcmp(&verify, &record, sizeof(record)) == 0;
}

BatteryStatus read_battery_status() {
    BatteryStatus status = {};
    uint8_t reg = KBD_REG_BATTERY;
    uint8_t raw[2] = {0};
    int written = i2c_write_timeout_us(CLOCK_I2C_PORT, I2C_ADDR_KEYBOARD, &reg, 1,
                                       true, I2C_SCAN_TIMEOUT_US);
    if (written != 1) {
        return status;
    }
    int read = i2c_read_timeout_us(CLOCK_I2C_PORT, I2C_ADDR_KEYBOARD, raw, 2,
                                   false, I2C_SCAN_TIMEOUT_US);
    if (read != 2) {
        return status;
    }

    status.ok = true;
    status.raw = raw[1];
    status.charging = (raw[1] & 0x80u) != 0;
    status.percent = raw[1] & 0x7fu;
    if (status.percent > 100) {
        status.percent = 100;
    }
    return status;
}

ProbeResult run_startup_probes() {
    ProbeResult result = {};
    result.rtc_ok = ds3231_read_status(CLOCK_I2C_PORT, &result.rtc_status);
    result.eeprom_ok = probe_eeprom_24c32(I2C_ADDR_AT24C32_EXPECTED);
    result.keyboard_ok = probe_keyboard_controller();
    std::printf("STARTUP PROBE rtc=%s eeprom=%s keyboard=%s rtc_status=0x%02X\r\n",
                result.rtc_ok ? "PASS" : "FAIL",
                result.eeprom_ok ? "PASS" : "FAIL",
                result.keyboard_ok ? "PASS" : "FAIL",
                result.rtc_status);
    return result;
}

bool is_leap_year(int year) {
    return ((year % 4) == 0 && (year % 100) != 0) || ((year % 400) == 0);
}

uint8_t days_in_month(int year, int month) {
    static constexpr uint8_t kMonthDays[] = {
        31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
    };
    if (month < 1 || month > 12) {
        return 0;
    }
    if (month == 2 && is_leap_year(year)) {
        return 29;
    }
    return kMonthDays[month - 1];
}

int days_before_month(int year, int month) {
    static constexpr int kDays[] = {
        0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334
    };
    int days = kDays[month - 1];
    if (month > 2 && is_leap_year(year)) {
        ++days;
    }
    return days;
}

int weekday_from_date(int year, int month, int day) {
    const int y = year - 1;
    const int days_before_year = y * 365 + y / 4 - y / 100 + y / 400;
    const int ordinal = days_before_year + days_before_month(year, month) + day;
    return ordinal % 7;
}

const char* weekday_name(int weekday) {
    static constexpr const char* kNames[] = {
        "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
    };
    if (weekday < 0 || weekday > 6) {
        return "---";
    }
    return kNames[weekday];
}

void print_datetime(const ds3231_datetime_t& dt) {
    const int weekday = weekday_from_date(dt.year, dt.month, dt.day);
    std::printf("%04u-%02u-%02u %s %02u:%02u:%02u\r\n",
                dt.year, dt.month, dt.day, weekday_name(weekday),
                dt.hour, dt.minute, dt.second);
}

bool is_valid_datetime(const ds3231_datetime_t& dt) {
    if (dt.year < 2000 || dt.year > 2099 || dt.month < 1 || dt.month > 12 ||
        dt.day < 1 || dt.hour > 23 || dt.minute > 59 || dt.second > 59) {
        return false;
    }

    return dt.day <= days_in_month(dt.year, dt.month);
}

void draw_clock_frame() {
    char header[64];
    std::snprintf(header, sizeof(header), "Clock v%s git=%s",
                  PICOCALC_CLOCK_VERSION_STRING,
                  PICOCALC_CLOCK_GIT_HASH);

    picoment::display::clear(kBlack);
    picoment::display::draw_text_band(10, kHeaderY, 198, kHeaderH, header, kDim, kBlack);
}

void format_clock_lines(const ds3231_datetime_t& dt,
                        bool rtc_ok,
                        bool show_seconds,
                        char* date_line,
                        size_t date_len,
                        char* time_line,
                        size_t time_len) {

    if (rtc_ok) {
        const int weekday = weekday_from_date(dt.year, dt.month, dt.day);
        std::snprintf(date_line, date_len, "%04u-%02u-%02u %s",
                      dt.year, dt.month, dt.day, weekday_name(weekday));
        if (show_seconds) {
            std::snprintf(time_line, time_len, "%02u:%02u:%02u",
                          dt.hour, dt.minute, dt.second);
        } else {
            std::snprintf(time_line, time_len, "%02u:%02u",
                          dt.hour, dt.minute);
        }
    } else {
        std::snprintf(date_line, date_len, "---- -- -- ---");
        std::snprintf(time_line, time_len, show_seconds ? "--:--:--" : "--:--");
    }
}

void draw_clock_delta(const char* date_line,
                      const char* time_line,
                      char* previous_date,
                      char* previous_time,
                      bool rtc_ok) {
    if (std::strcmp(date_line, previous_date) != 0) {
        const int date_text_w = static_cast<int>(std::strlen(date_line)) * 12;
        const int date_text_x = (picoment::display::kScreenWidth - date_text_w) / 2;
        picoment::display::fill_rect(kDateBandX, kDateY, kDateBandW, kDateH, kBlack);
        picoment::display::draw_spleen_native_text_band(
            date_text_x, kDateY, date_text_w, kDateH, date_line,
            picoment::font::SpleenNativeSize::S12x24, kWhite, kBlack);
        std::snprintf(previous_date, 40, "%s", date_line);
    }

    const int len = static_cast<int>(std::strlen(time_line));
    const int old_len = static_cast<int>(std::strlen(previous_time));
    const bool large_time = len == 5;
    const int char_w = large_time ? kTimeNoSecondsCharW : kTimeCharW;
    const int time_h = large_time ? kTimeNoSecondsH : kTimeH;
    const int time_y = large_time ? kTimeNoSecondsY : kTimeY;
    const int time_x = (picoment::display::kScreenWidth - len * char_w) / 2;
    if (len != old_len) {
        picoment::display::fill_rect(0, kTimeNoSecondsY,
                                     picoment::display::kScreenWidth,
                                     kTimeNoSecondsH, kBlack);
        std::snprintf(previous_time, 9, "        ");
    }

    for (int i = 0; i < len; ++i) {
        if (time_line[i] != previous_time[i]) {
            char ch[2] = {time_line[i], '\0'};
            if (large_time) {
                picoment::display::draw_spleen_native_text_3x2_band(
                    time_x + i * char_w, time_y, char_w, time_h, ch,
                    picoment::font::SpleenNativeSize::S32x64,
                    rtc_ok ? kWhite : kWarn, kBlack);
            } else {
                picoment::display::draw_spleen_native_text_band(
                    time_x + i * char_w, time_y, char_w, time_h, ch,
                    picoment::font::SpleenNativeSize::S32x64,
                    rtc_ok ? kWhite : kWarn, kBlack);
            }
            previous_time[i] = time_line[i];
        }
    }
    previous_time[len] = '\0';
}

void draw_no_seconds_colon(bool visible,
                           bool rtc_ok,
                           char* previous_time) {
    if (std::strlen(previous_time) != 5) {
        return;
    }

    const char colon = visible ? ':' : ' ';
    if (previous_time[2] == colon) {
        return;
    }

    const int time_x =
        (picoment::display::kScreenWidth - 5 * kTimeNoSecondsCharW) / 2;
    char ch[2] = {colon, '\0'};
    picoment::display::draw_spleen_native_text_3x2_band(
        time_x + 2 * kTimeNoSecondsCharW,
        kTimeNoSecondsY,
        kTimeNoSecondsCharW,
        kTimeNoSecondsH,
        ch,
        picoment::font::SpleenNativeSize::S32x64,
        rtc_ok ? kWhite : kWarn,
        kBlack);
    previous_time[2] = colon;
}

void format_battery_text(const BatteryStatus& battery, char* text, size_t len) {
    if (!battery.ok) {
        std::snprintf(text, len, "Bat. --%%");
    } else if (battery.charging) {
        std::snprintf(text, len, "Bat. %u%%+", battery.percent);
    } else {
        std::snprintf(text, len, "Bat. %u%%", battery.percent);
    }
}

void draw_battery_delta(const BatteryStatus& battery, char* previous_battery) {
    char text[16];
    format_battery_text(battery, text, sizeof(text));
    if (std::strcmp(text, previous_battery) == 0) {
        return;
    }

    const int text_w = static_cast<int>(std::strlen(text)) * picoment::font::kCozetteWidth;
    int text_x = kBatteryBandX + kBatteryBandW - text_w;
    if (text_x < kBatteryBandX) {
        text_x = kBatteryBandX;
    }
    picoment::display::fill_rect(kBatteryBandX, kHeaderY, kBatteryBandW, kHeaderH, kBlack);
    picoment::display::draw_text_band(text_x, kHeaderY,
                                      kBatteryBandW - (text_x - kBatteryBandX),
                                      kHeaderH, text, kDim, kBlack);
    std::snprintf(previous_battery, 16, "%s", text);
}

void set_default_alarms(AlarmSettings* alarms) {
    static constexpr AlarmSettings kDefaults[kAlarmCount] = {
        {false, 7, 30},
        {false, 8, 0},
        {false, 12, 0},
        {false, 18, 0},
        {false, 22, 0},
    };
    for (uint8_t i = 0; i < kAlarmCount; ++i) {
        alarms[i] = kDefaults[i];
    }
}

AppSettings default_app_settings() {
    AppSettings settings = {};
    settings.show_seconds = true;
    settings.clock_style = kClockStyleDigital;
    return settings;
}

bool alarms_equal(const AlarmSettings* a, const AlarmSettings* b) {
    for (uint8_t i = 0; i < kAlarmCount; ++i) {
        if (a[i].enabled != b[i].enabled ||
            a[i].hour != b[i].hour ||
            a[i].minute != b[i].minute) {
            return false;
        }
    }
    return true;
}

bool app_settings_equal(const AppSettings& a, const AppSettings& b) {
    return a.show_seconds == b.show_seconds &&
           a.clock_style == b.clock_style;
}

bool alarm_settings_valid(const AlarmSettings* alarms) {
    for (uint8_t i = 0; i < kAlarmCount; ++i) {
        if (alarms[i].hour > 23 || alarms[i].minute > 59) {
            return false;
        }
    }
    return true;
}

bool app_settings_valid(const AppSettings& settings) {
    return settings.clock_style == kClockStyleDigital ||
           settings.clock_style == kClockStyleAnalog;
}

void make_settings_record(const AlarmSettings* alarms,
                          const AppSettings& settings,
                          uint32_t sequence,
                          SettingsRecord* record) {
    std::memset(record, 0, sizeof(*record));
    record->magic = kSettingsMagic;
    record->version = kSettingsVersion;
    record->size = kSettingsRecordSize;
    record->sequence = sequence;
    for (uint8_t i = 0; i < kAlarmCount; ++i) {
        record->alarm_enabled[i] = alarms[i].enabled ? 1u : 0u;
        record->alarm_hour[i] = alarms[i].hour;
        record->alarm_minute[i] = alarms[i].minute;
    }
    record->app_flags = settings.show_seconds ? kSettingsFlagShowSeconds : 0u;
    record->clock_style = settings.clock_style;
    record->crc32 = settings_record_crc(*record);
}

bool settings_record_valid(const SettingsRecord& record) {
    if (record.magic != kSettingsMagic ||
        (record.version != kSettingsVersion &&
         record.version != kSettingsVersionAlarmOnly) ||
        record.size != kSettingsRecordSize ||
        record.crc32 != settings_record_crc(record)) {
        return false;
    }
    for (uint8_t i = 0; i < kAlarmCount; ++i) {
        if (record.alarm_enabled[i] > 1 ||
            record.alarm_hour[i] > 23 ||
            record.alarm_minute[i] > 59) {
            return false;
        }
    }
    if (record.version >= kSettingsVersion &&
        record.clock_style != kClockStyleDigital &&
        record.clock_style != kClockStyleAnalog) {
        return false;
    }
    return true;
}

void apply_settings_record(const SettingsRecord& record,
                           AlarmSettings* alarms,
                           AppSettings* settings) {
    for (uint8_t i = 0; i < kAlarmCount; ++i) {
        alarms[i].enabled = record.alarm_enabled[i] != 0;
        alarms[i].hour = record.alarm_hour[i];
        alarms[i].minute = record.alarm_minute[i];
    }
    if (record.version >= kSettingsVersion) {
        settings->show_seconds = (record.app_flags & kSettingsFlagShowSeconds) != 0;
        settings->clock_style = record.clock_style;
    } else {
        *settings = default_app_settings();
    }
}

bool read_settings_slot(uint16_t slot_address, SettingsRecord* record) {
    return eeprom_read_bytes(slot_address,
                             reinterpret_cast<uint8_t*>(record),
                             sizeof(*record));
}

bool load_settings_from_eeprom(AlarmSettings* alarms,
                               AppSettings* settings,
                               uint32_t* sequence) {
    SettingsRecord slot_a = {};
    SettingsRecord slot_b = {};
    const bool read_a = read_settings_slot(kSettingsSlotA, &slot_a);
    const bool read_b = read_settings_slot(kSettingsSlotB, &slot_b);
    const bool valid_a = read_a && settings_record_valid(slot_a);
    const bool valid_b = read_b && settings_record_valid(slot_b);

    if (!valid_a && !valid_b) {
        std::printf("SETTINGS eeprom load default slotA=%s slotB=%s\r\n",
                    valid_a ? "valid" : "invalid",
                    valid_b ? "valid" : "invalid");
        return false;
    }

    const SettingsRecord* selected = &slot_a;
    char selected_slot = 'A';
    if (!valid_a || (valid_b && slot_b.sequence > slot_a.sequence)) {
        selected = &slot_b;
        selected_slot = 'B';
    }

    apply_settings_record(*selected, alarms, settings);
    if (!alarm_settings_valid(alarms)) {
        std::puts("SETTINGS eeprom load default reason=invalid_alarm");
        return false;
    }
    *sequence = selected->sequence;
    std::printf("SETTINGS eeprom load ok slot=%c seq=%lu\r\n",
                selected_slot,
                static_cast<unsigned long>(*sequence));
    return true;
}

bool save_settings_to_eeprom(const AlarmSettings* alarms,
                             const AppSettings& settings,
                             uint32_t* sequence) {
    if (!alarm_settings_valid(alarms)) {
        std::puts("SETTINGS eeprom save fail reason=invalid_alarm");
        return false;
    }
    if (!app_settings_valid(settings)) {
        std::puts("SETTINGS eeprom save fail reason=invalid_app_settings");
        return false;
    }

    const uint32_t next_sequence = *sequence + 1u;
    const uint16_t slot = (next_sequence & 1u) ? kSettingsSlotA : kSettingsSlotB;
    const char slot_name = (slot == kSettingsSlotA) ? 'A' : 'B';
    SettingsRecord record = {};
    make_settings_record(alarms, settings, next_sequence, &record);

    if (!eeprom_write_record(slot, record)) {
        std::printf("SETTINGS eeprom save fail slot=%c seq=%lu\r\n",
                    slot_name,
                    static_cast<unsigned long>(next_sequence));
        return false;
    }

    *sequence = next_sequence;
    std::printf("SETTINGS eeprom save ok slot=%c seq=%lu\r\n",
                slot_name,
                static_cast<unsigned long>(*sequence));
    return true;
}

bool same_alarm_minute(const AlarmFireRecord& record,
                       const ds3231_datetime_t& dt) {
    return record.valid &&
           record.year == dt.year &&
           record.month == dt.month &&
           record.day == dt.day &&
           record.hour == dt.hour &&
           record.minute == dt.minute;
}

void record_alarm_minute(AlarmFireRecord* record, const ds3231_datetime_t& dt) {
    record->year = dt.year;
    record->month = dt.month;
    record->day = dt.day;
    record->hour = dt.hour;
    record->minute = dt.minute;
    record->valid = true;
}

AlarmMatch find_alarm_match(const AlarmSettings* alarms,
                            const ds3231_datetime_t& dt) {
    AlarmMatch match = {};
    match.first_index = 0xffu;
    match.hour = dt.hour;
    match.minute = dt.minute;
    for (uint8_t i = 0; i < kAlarmCount; ++i) {
        if (!alarms[i].enabled ||
            alarms[i].hour != dt.hour ||
            alarms[i].minute != dt.minute) {
            continue;
        }
        if (!match.found) {
            match.found = true;
            match.first_index = i;
        }
        ++match.count;
    }
    return match;
}

int alarm_minutes_until(uint8_t now_hour,
                        uint8_t now_minute,
                        const AlarmSettings& alarm) {
    const int now_total = now_hour * 60 + now_minute;
    const int alarm_total = alarm.hour * 60 + alarm.minute;
    int delta = alarm_total - now_total;
    if (delta <= 0) {
        delta += 24 * 60;
    }
    return delta;
}

AlarmMatch find_next_alarm(const AlarmSettings* alarms,
                           const ds3231_datetime_t& dt,
                           const AlarmFireRecord& last_fire) {
    AlarmMatch next = {};
    next.first_index = 0xffu;
    int best_delta = 24 * 60 + 1;

    for (uint8_t i = 0; i < kAlarmCount; ++i) {
        if (!alarms[i].enabled) {
            continue;
        }
        int delta = alarm_minutes_until(dt.hour, dt.minute, alarms[i]);
        if (delta == 24 * 60 && same_alarm_minute(last_fire, dt)) {
            continue;
        }
        if (delta < best_delta) {
            best_delta = delta;
            next.found = true;
            next.first_index = i;
            next.count = 1;
            next.hour = alarms[i].hour;
            next.minute = alarms[i].minute;
        } else if (delta == best_delta &&
                   next.found &&
                   alarms[i].hour == next.hour &&
                   alarms[i].minute == next.minute) {
            ++next.count;
        }
    }

    return next;
}

void format_alarm_label(const AlarmMatch& match, char* text, size_t len) {
    if (!match.found) {
        std::snprintf(text, len, "Alm OFF");
        return;
    }
    std::snprintf(text, len, "Next A%u%s %02u:%02u",
                  match.first_index + 1u,
                  match.count > 1 ? "+" : "",
                  match.hour,
                  match.minute);
}

void draw_alarm_delta(const AlarmSettings* alarms,
                      const ds3231_datetime_t& dt,
                      const AlarmFireRecord& last_fire,
                      char* previous_alarm) {
    char text[24];
    const AlarmMatch next = find_next_alarm(alarms, dt, last_fire);
    format_alarm_label(next, text, sizeof(text));
    if (std::strcmp(text, previous_alarm) == 0) {
        return;
    }

    const int text_w = static_cast<int>(std::strlen(text)) * 12;
    const int text_x = (picoment::display::kScreenWidth - text_w) / 2;
    picoment::display::fill_rect(kAlarmBandX, kAlarmBandY,
                                 kAlarmBandW, kAlarmBandH, kBlack);
    picoment::display::draw_spleen_native_text_band(
        text_x, kAlarmBandY, text_w, kAlarmBandH, text,
        picoment::font::SpleenNativeSize::S12x24, kDim, kBlack);
    std::snprintf(previous_alarm, 24, "%s", text);
}

int analog_x(uint8_t index, int length) {
    return kAnalogCenterX + kSin60[index % 60] * length / 1024;
}

int analog_y(uint8_t index, int length) {
    return kAnalogCenterY - kCos60[index % 60] * length / 1024;
}

AnalogHandState make_analog_hand_state(const ds3231_datetime_t& dt,
                                       bool rtc_ok,
                                       bool show_seconds) {
    AnalogHandState state = {};
    state.valid = rtc_ok;
    state.rtc_ok = rtc_ok;
    state.show_second = show_seconds;
    if (!rtc_ok) {
        return state;
    }

    state.hour_index =
        static_cast<uint8_t>(((dt.hour % 12) * 5 + dt.minute / 12) % 60);
    state.minute_index = static_cast<uint8_t>(dt.minute % 60);
    state.second_index = static_cast<uint8_t>(dt.second % 60);
    return state;
}

bool analog_hand_state_equal(const AnalogHandState& a,
                             const AnalogHandState& b) {
    if (a.valid != b.valid ||
        a.rtc_ok != b.rtc_ok ||
        a.show_second != b.show_second ||
        a.hour_index != b.hour_index ||
        a.minute_index != b.minute_index) {
        return false;
    }
    if (!a.show_second) {
        return true;
    }
    return a.second_index == b.second_index;
}

void draw_analog_static_face() {
    picoment::display::draw_circle(kAnalogCenterX, kAnalogCenterY,
                                   kAnalogRadius, kDim);
    for (uint8_t i = 0; i < 12; ++i) {
        const uint8_t index = static_cast<uint8_t>(i * 5);
        const int x0 = analog_x(index, kAnalogRadius - 8);
        const int y0 = analog_y(index, kAnalogRadius - 8);
        const int x1 = analog_x(index, kAnalogRadius);
        const int y1 = analog_y(index, kAnalogRadius);
        picoment::display::draw_line(x0, y0, x1, y1, kDim);
    }
}

void restore_analog_static_face_details() {
    draw_analog_static_face();
}

void draw_analog_hand(uint8_t index, int length, uint16_t color, bool thick) {
    const int x1 = analog_x(index, length);
    const int y1 = analog_y(index, length);
    picoment::display::draw_line(kAnalogCenterX, kAnalogCenterY, x1, y1, color);
    if (!thick) {
        return;
    }

    const int dx = x1 - kAnalogCenterX;
    const int dy = y1 - kAnalogCenterY;
    int ox = 0;
    int oy = 0;
    if (dx * dx > dy * dy) {
        oy = 1;
    } else {
        ox = 1;
    }
    picoment::display::draw_line(kAnalogCenterX + ox, kAnalogCenterY + oy,
                                 x1 + ox, y1 + oy, color);
    picoment::display::draw_line(kAnalogCenterX - ox, kAnalogCenterY - oy,
                                 x1 - ox, y1 - oy, color);
}

void draw_analog_hands(const AnalogHandState& state, bool erase) {
    if (!state.valid || !state.rtc_ok) {
        return;
    }

    const uint16_t hour_minute_color = erase ? kBlack : kWhite;
    const uint16_t second_color = erase ? kBlack : kSecondHand;
    draw_analog_hand(state.hour_index, kAnalogHourHandLength, hour_minute_color, true);
    draw_analog_hand(state.minute_index, kAnalogMinuteHandLength, hour_minute_color, true);
    if (state.show_second) {
        draw_analog_hand(state.second_index, kAnalogSecondHandLength, second_color, false);
    }
}

void draw_analog_hub() {
    picoment::display::fill_circle(kAnalogCenterX, kAnalogCenterY,
                                   kAnalogHubRadius, kWhite);
}

void draw_analog_date_delta(const ds3231_datetime_t& dt,
                            bool rtc_ok,
                            char* previous_date) {
    char text[40];
    if (rtc_ok) {
        const int weekday = weekday_from_date(dt.year, dt.month, dt.day);
        std::snprintf(text, sizeof(text), "%04u-%02u-%02u %s",
                      dt.year, dt.month, dt.day, weekday_name(weekday));
    } else {
        std::snprintf(text, sizeof(text), "---- -- -- ---");
    }

    if (std::strcmp(text, previous_date) == 0) {
        return;
    }

    const int text_w = static_cast<int>(std::strlen(text)) * 12;
    const int text_x = (picoment::display::kScreenWidth - text_w) / 2;
    picoment::display::fill_rect(kDateBandX, kAnalogDateY,
                                 kDateBandW, kAnalogDateH, kBlack);
    picoment::display::draw_spleen_native_text_band(
        text_x, kAnalogDateY, text_w, kAnalogDateH, text,
        picoment::font::SpleenNativeSize::S12x24,
        rtc_ok ? kWhite : kWarn, kBlack);
    std::snprintf(previous_date, 40, "%s", text);
}

void draw_analog_alarm_delta(const AlarmSettings* alarms,
                             const ds3231_datetime_t& dt,
                             const AlarmFireRecord& last_fire,
                             char* previous_alarm) {
    char text[24];
    const AlarmMatch next = find_next_alarm(alarms, dt, last_fire);
    format_alarm_label(next, text, sizeof(text));
    if (std::strcmp(text, previous_alarm) == 0) {
        return;
    }

    const int text_w = static_cast<int>(std::strlen(text)) * 12;
    const int text_x = (picoment::display::kScreenWidth - text_w) / 2;
    picoment::display::fill_rect(kAnalogAlarmX, kAnalogAlarmY,
                                 kAnalogAlarmW, kAnalogAlarmH, kBlack);
    picoment::display::draw_spleen_native_text_band(
        text_x, kAnalogAlarmY, text_w, kAnalogAlarmH, text,
        picoment::font::SpleenNativeSize::S12x24, kDim, kBlack);
    std::snprintf(previous_alarm, 24, "%s", text);
}

void draw_analog_rtc_failure_label() {
    constexpr int kLabelW = 72;
    constexpr int kLabelX = (picoment::display::kScreenWidth - kLabelW) / 2;
    picoment::display::draw_spleen_native_text_band(
        kLabelX, kAnalogAlarmY, kLabelW, kAnalogAlarmH, "RTC --",
        picoment::font::SpleenNativeSize::S12x24, kWarn, kBlack);
}

void draw_analog_clock(const ds3231_datetime_t& dt,
                       bool rtc_ok,
                       const BatteryStatus& battery,
                       const AlarmSettings* alarms,
                       const AlarmFireRecord& last_fire,
                       bool show_seconds,
                       bool force_full_redraw,
                       char* previous_date,
                       char* previous_battery,
                       char* previous_alarm,
                       AnalogHandState* previous_hand_state) {
    const AnalogHandState new_state =
        make_analog_hand_state(dt, rtc_ok, show_seconds);

    if (force_full_redraw) {
        draw_clock_frame();
        previous_date[0] = '\0';
        previous_battery[0] = '\0';
        previous_alarm[0] = '\0';
        previous_hand_state->valid = false;
        draw_analog_static_face();
    }

    draw_analog_date_delta(dt, rtc_ok, previous_date);
    draw_battery_delta(battery, previous_battery);

    if (rtc_ok) {
        draw_analog_alarm_delta(alarms, dt, last_fire, previous_alarm);
        if (!analog_hand_state_equal(*previous_hand_state, new_state)) {
            if (previous_hand_state->valid) {
                draw_analog_hands(*previous_hand_state, true);
                restore_analog_static_face_details();
            }
            draw_analog_hands(new_state, false);
            draw_analog_hub();
            *previous_hand_state = new_state;
        }
    } else {
        if (previous_hand_state->valid) {
            draw_analog_hands(*previous_hand_state, true);
            restore_analog_static_face_details();
        }
        picoment::display::fill_rect(kAnalogAlarmX, kAnalogAlarmY,
                                     kAnalogAlarmW, kAnalogAlarmH, kBlack);
        draw_analog_rtc_failure_label();
        previous_alarm[0] = '\0';
        previous_hand_state->valid = false;
    }
}

TimeField next_field(TimeField field) {
    if (field == TimeField::Second) {
        return TimeField::Second;
    }
    return static_cast<TimeField>(static_cast<uint8_t>(field) + 1u);
}

TimeField previous_field(TimeField field) {
    if (field == TimeField::Year) {
        return TimeField::Year;
    }
    return static_cast<TimeField>(static_cast<uint8_t>(field) - 1u);
}

uint8_t field_digit_count(TimeField) {
    return 2;
}

int wrap_value(int value, int min_value, int max_value) {
    if (value > max_value) {
        return min_value;
    }
    if (value < min_value) {
        return max_value;
    }
    return value;
}

int field_min(const SetTimeModel&, TimeField field) {
    switch (field) {
    case TimeField::Year:
    case TimeField::Hour:
    case TimeField::Minute:
    case TimeField::Second:
        return 0;
    case TimeField::Month:
    case TimeField::Day:
        return 1;
    default:
        return 0;
    }
}

int field_max(const SetTimeModel& model, TimeField field) {
    switch (field) {
    case TimeField::Year:
        return 99;
    case TimeField::Month:
        return 12;
    case TimeField::Day:
        return days_in_month(model.year, model.month);
    case TimeField::Hour:
        return 23;
    case TimeField::Minute:
    case TimeField::Second:
        return 59;
    default:
        return 0;
    }
}

int get_field_value(const SetTimeModel& model, TimeField field) {
    switch (field) {
    case TimeField::Year:
        return model.year - 2000;
    case TimeField::Month:
        return model.month;
    case TimeField::Day:
        return model.day;
    case TimeField::Hour:
        return model.hour;
    case TimeField::Minute:
        return model.minute;
    case TimeField::Second:
        return model.second;
    default:
        return 0;
    }
}

void apply_preferred_day(SetTimeModel* model) {
    const uint8_t max_day = days_in_month(model->year, model->month);
    model->day = model->preferred_day > max_day ? max_day : model->preferred_day;
}

void set_field_value(SetTimeModel* model, TimeField field, int value) {
    switch (field) {
    case TimeField::Year:
        model->year = static_cast<uint16_t>(2000 + value);
        apply_preferred_day(model);
        break;
    case TimeField::Month:
        model->month = static_cast<uint8_t>(value);
        apply_preferred_day(model);
        break;
    case TimeField::Day:
        model->day = static_cast<uint8_t>(value);
        model->preferred_day = model->day;
        break;
    case TimeField::Hour:
        model->hour = static_cast<uint8_t>(value);
        break;
    case TimeField::Minute:
        model->minute = static_cast<uint8_t>(value);
        break;
    case TimeField::Second:
        model->second = static_cast<uint8_t>(value);
        break;
    }
}

void set_status(SetTimeModel* model, const char* text) {
    std::snprintf(model->status, sizeof(model->status), "%s", text);
}

SetTimeModel make_set_time_model(const ds3231_datetime_t& dt) {
    SetTimeModel model = {};
    model.year = dt.year;
    model.month = dt.month;
    model.day = dt.day;
    model.hour = dt.hour;
    model.minute = dt.minute;
    model.second = dt.second;
    model.preferred_day = dt.day;
    model.field = TimeField::Year;
    model.selection = SelectionMode::Field;
    model.digit_index = 0;
    set_status(&model, "Enter=save Esc=cancel");
    return model;
}

void format_set_time_lines(const SetTimeModel& model,
                           char* date_line,
                           size_t date_len,
                           char* time_line,
                           size_t time_len) {
    std::snprintf(date_line, date_len, "%04u-%02u-%02u",
                  model.year, model.month, model.day);
    std::snprintf(time_line, time_len, "%02u:%02u:%02u",
                  model.hour, model.minute, model.second);
}

void field_position(TimeField field, int* row, int* start, int* width) {
    switch (field) {
    case TimeField::Year:
        *row = 0;
        *start = 2;
        *width = 2;
        break;
    case TimeField::Month:
        *row = 0;
        *start = 5;
        *width = 2;
        break;
    case TimeField::Day:
        *row = 0;
        *start = 8;
        *width = 2;
        break;
    case TimeField::Hour:
        *row = 1;
        *start = 0;
        *width = 2;
        break;
    case TimeField::Minute:
        *row = 1;
        *start = 3;
        *width = 2;
        break;
    case TimeField::Second:
        *row = 1;
        *start = 6;
        *width = 2;
        break;
    }
}

void draw_set_time_screen(const SetTimeModel& model) {
    char date_line[16];
    char time_line[12];
    format_set_time_lines(model, date_line, sizeof(date_line),
                          time_line, sizeof(time_line));

    constexpr int kTitleY = 22;
    constexpr int kDateX = 80;
    constexpr int kDateYSet = 94;
    constexpr int kTimeXSet = 96;
    constexpr int kTimeYSet = 150;
    constexpr int kCharW = 16;
    constexpr int kCharH = 32;

    picoment::display::clear(kBlack);
    picoment::display::draw_spleen_native_text_band(
        62, kTitleY, 196, 24, "SET TIME",
        picoment::font::SpleenNativeSize::S12x24, kDim, kBlack);

    int selected_row = 0;
    int selected_start = 0;
    int selected_width = 0;
    field_position(model.field, &selected_row, &selected_start, &selected_width);
    if (model.selection == SelectionMode::Digit) {
        selected_start += model.digit_index;
        selected_width = 1;
    }

    const char* rows[2] = {date_line, time_line};
    const int row_x[2] = {kDateX, kTimeXSet};
    const int row_y[2] = {kDateYSet, kTimeYSet};
    for (int row = 0; row < 2; ++row) {
        const int len = static_cast<int>(std::strlen(rows[row]));
        for (int i = 0; i < len; ++i) {
            const bool selected =
                row == selected_row &&
                i >= selected_start &&
                i < selected_start + selected_width;
            char ch[2] = {rows[row][i], '\0'};
            picoment::display::draw_spleen_native_text_band(
                row_x[row] + i * kCharW, row_y[row],
                kCharW, kCharH, ch,
                picoment::font::SpleenNativeSize::S16x32,
                selected ? kHighlightText : kWhite,
                selected ? (model.selection == SelectionMode::Digit ? kHighlightDigit
                                                                     : kHighlight)
                         : kBlack);
        }
    }

    picoment::display::draw_text_band(
        32, 250, 256, 18, model.status, kDim, kBlack);
}

bool field_has_prefix_value(int value, int digit_count, int prefix_len, int prefix) {
    int divisor = 1;
    for (int i = 0; i < digit_count - prefix_len; ++i) {
        divisor *= 10;
    }
    return value / divisor == prefix;
}

bool find_lowest_valid_with_prefix(const SetTimeModel& model,
                                   TimeField field,
                                   int prefix_len,
                                   int prefix,
                                   int* value) {
    const int min_value = field_min(model, field);
    const int max_value = field_max(model, field);
    const int digit_count = field_digit_count(field);
    for (int candidate = min_value; candidate <= max_value; ++candidate) {
        if (field_has_prefix_value(candidate, digit_count, prefix_len, prefix)) {
            *value = candidate;
            return true;
        }
    }
    return false;
}

bool replace_digit_prefix_valid(SetTimeModel* model, uint8_t digit) {
    const TimeField field = model->field;
    const int index = model->digit_index;
    const int current = get_field_value(*model, field);
    const int tens = current / 10;
    const int ones = current % 10;
    int digits[2] = {tens, ones};
    digits[index] = digit;
    const int candidate = digits[0] * 10 + digits[1];
    const int prefix_len = index + 1;
    int prefix = 0;
    for (int i = 0; i < prefix_len; ++i) {
        prefix = prefix * 10 + digits[i];
    }

    const int min_value = field_min(*model, field);
    const int max_value = field_max(*model, field);
    int normalized = candidate;
    if (candidate < min_value || candidate > max_value) {
        if (!find_lowest_valid_with_prefix(*model, field, prefix_len, prefix,
                                           &normalized)) {
            set_status(model, "Invalid digit");
            return false;
        }
    }

    set_field_value(model, field, normalized);
    set_status(model, "Enter=save Esc=cancel");
    return true;
}

void select_field(SetTimeModel* model, TimeField field) {
    model->field = field;
    model->selection = SelectionMode::Field;
    model->digit_index = 0;
}

void advance_after_digit(SetTimeModel* model) {
    const uint8_t count = field_digit_count(model->field);
    if (model->digit_index + 1u < count) {
        model->selection = SelectionMode::Digit;
        ++model->digit_index;
        return;
    }
    if (model->field == TimeField::Second) {
        select_field(model, TimeField::Second);
    } else {
        select_field(model, next_field(model->field));
    }
}

void commit_day_if_needed(SetTimeModel* model) {
    if (model->field == TimeField::Day) {
        model->preferred_day = model->day;
    }
}

void handle_set_time_left(SetTimeModel* model) {
    commit_day_if_needed(model);
    if (model->selection == SelectionMode::Digit) {
        if (model->digit_index > 0) {
            --model->digit_index;
        } else {
            select_field(model, previous_field(model->field));
        }
    } else {
        select_field(model, previous_field(model->field));
    }
}

void handle_set_time_right(SetTimeModel* model) {
    commit_day_if_needed(model);
    if (model->selection == SelectionMode::Digit) {
        if (model->digit_index + 1u < field_digit_count(model->field)) {
            ++model->digit_index;
        } else {
            select_field(model, next_field(model->field));
        }
    } else {
        select_field(model, next_field(model->field));
    }
}

void handle_set_time_up_down(SetTimeModel* model, int delta) {
    if (model->selection == SelectionMode::Field) {
        const int min_value = field_min(*model, model->field);
        const int max_value = field_max(*model, model->field);
        const int value = wrap_value(get_field_value(*model, model->field) + delta,
                                     min_value, max_value);
        set_field_value(model, model->field, value);
        set_status(model, "Enter=save Esc=cancel");
        return;
    }

    const int current = get_field_value(*model, model->field);
    int digits[2] = {current / 10, current % 10};
    digits[model->digit_index] = wrap_value(digits[model->digit_index] + delta, 0, 9);
    const int candidate = digits[0] * 10 + digits[1];
    const int min_value = field_min(*model, model->field);
    const int max_value = field_max(*model, model->field);
    set_field_value(model, model->field,
                    candidate < min_value ? min_value :
                    candidate > max_value ? max_value : candidate);
    commit_day_if_needed(model);
    set_status(model, "Enter=save Esc=cancel");
}

void handle_set_time_digit(SetTimeModel* model, uint8_t digit) {
    if (model->selection == SelectionMode::Field) {
        model->selection = SelectionMode::Digit;
        model->digit_index = 0;
    }
    if (replace_digit_prefix_valid(model, digit)) {
        advance_after_digit(model);
    }
}

ds3231_datetime_t model_to_datetime(const SetTimeModel& model) {
    ds3231_datetime_t dt = {};
    dt.year = model.year;
    dt.month = model.month;
    dt.day = model.day;
    dt.hour = model.hour;
    dt.minute = model.minute;
    dt.second = model.second;
    dt.day_of_week = ds3231_calculate_day_of_week(dt.year, dt.month, dt.day);
    return dt;
}

void set_alarm_status(AlarmEditModel* model, const char* text) {
    std::snprintf(model->status, sizeof(model->status), "%s", text);
}

AlarmEditModel make_alarm_edit_model(const AlarmSettings* alarms) {
    AlarmEditModel model = {};
    for (uint8_t i = 0; i < kAlarmCount; ++i) {
        model.alarms[i] = alarms[i];
    }
    model.selected_index = 0;
    model.field = AlarmField::Hour;
    model.selection = AlarmSelectionMode::Row;
    model.digit_index = 0;
    set_alarm_status(&model, "Enter=save Esc=back");
    return model;
}

void draw_set_alarm_screen(const AlarmEditModel& model) {
    constexpr int kRowX = 44;
    constexpr int kRowY = 64;
    constexpr int kRowH = 26;
    constexpr int kCharW = 12;
    constexpr int kFieldHourStart = 3;
    constexpr int kFieldMinuteStart = 6;
    constexpr int kFieldEnabledStart = 9;

    for (uint8_t row = 0; row < kAlarmCount; ++row) {
        char line[24];
        std::snprintf(line, sizeof(line), "A%u %02u:%02u %s",
                      row + 1u,
                      model.alarms[row].hour,
                      model.alarms[row].minute,
                      model.alarms[row].enabled ? "ON" : "OFF");
        const int y = kRowY + row * kRowH;
        const bool selected_row =
            model.selected_index == row &&
            model.selection == AlarmSelectionMode::Row;
        if (selected_row) {
            picoment::display::fill_rect(32, y, 256, kRowH, kHighlight);
        } else {
            picoment::display::fill_rect(32, y, 256, kRowH, kBlack);
        }

        const int len = static_cast<int>(std::strlen(line));
        for (int i = 0; i < len; ++i) {
            bool selected_field = false;
            if (model.selected_index == row &&
                model.selection != AlarmSelectionMode::Row) {
                int start = kFieldHourStart;
                int width = 2;
                if (model.field == AlarmField::Minute) {
                    start = kFieldMinuteStart;
                } else if (model.field == AlarmField::Enabled) {
                    start = kFieldEnabledStart;
                    width = model.alarms[row].enabled ? 2 : 3;
                }
                if (model.selection == AlarmSelectionMode::Digit) {
                    start += model.digit_index;
                    width = 1;
                }
                selected_field = i >= start && i < start + width;
            }

            char ch[2] = {line[i], '\0'};
            picoment::display::draw_spleen_native_text_band(
                kRowX + i * kCharW, y, kCharW, 24, ch,
                picoment::font::SpleenNativeSize::S12x24,
                selected_row || selected_field ? kHighlightText : kWhite,
                selected_field ? (model.selection == AlarmSelectionMode::Digit
                                      ? kHighlightDigit
                                      : kHighlight)
                               : (selected_row ? kHighlight : kBlack));
        }
    }

    picoment::display::draw_text_band(
        32, 250, 256, 18, model.status, kDim, kBlack);
}

void draw_set_alarm_screen_full(const AlarmEditModel& model) {
    constexpr int kTitleY = 18;
    picoment::display::clear(kBlack);
    picoment::display::draw_spleen_native_text_band(
        68, kTitleY, 184, 24, "SET ALARM",
        picoment::font::SpleenNativeSize::S12x24, kDim, kBlack);
    draw_set_alarm_screen(model);
}

void draw_alarm_ringing_screen(const AlarmMatch& match) {
    char title[32];
    if (match.count > 1) {
        std::snprintf(title, sizeof(title), "ALARM A%u+",
                      match.first_index + 1u);
    } else {
        std::snprintf(title, sizeof(title), "ALARM A%u",
                      match.first_index + 1u);
    }
    char time_line[12];
    std::snprintf(time_line, sizeof(time_line), "%02u:%02u",
                  match.hour, match.minute);

    picoment::display::clear(kBlack);
    picoment::display::draw_spleen_native_text_band(
        62, 48, 196, 32, title,
        picoment::font::SpleenNativeSize::S16x32, kWarn, kBlack);
    picoment::display::draw_spleen_native_text_band(
        80, 124, 160, 64, time_line,
        picoment::font::SpleenNativeSize::S32x64, kWhite, kBlack);
    picoment::display::draw_text_band(
        104, 232, 112, 18, "Space: Stop", kDim, kBlack);
}

AlarmField previous_alarm_field(AlarmField field) {
    if (field == AlarmField::Hour) {
        return AlarmField::Hour;
    }
    return static_cast<AlarmField>(static_cast<uint8_t>(field) - 1u);
}

AlarmField next_alarm_field(AlarmField field) {
    if (field == AlarmField::Enabled) {
        return AlarmField::Enabled;
    }
    return static_cast<AlarmField>(static_cast<uint8_t>(field) + 1u);
}

int get_alarm_field_value(const AlarmEditModel& model, AlarmField field) {
    const AlarmSettings& alarm = model.alarms[model.selected_index];
    switch (field) {
    case AlarmField::Hour:
        return alarm.hour;
    case AlarmField::Minute:
        return alarm.minute;
    case AlarmField::Enabled:
        return alarm.enabled ? 1 : 0;
    default:
        return 0;
    }
}

void set_alarm_field_value(AlarmEditModel* model, AlarmField field, int value) {
    AlarmSettings& alarm = model->alarms[model->selected_index];
    switch (field) {
    case AlarmField::Hour:
        alarm.hour = static_cast<uint8_t>(value);
        break;
    case AlarmField::Minute:
        alarm.minute = static_cast<uint8_t>(value);
        break;
    case AlarmField::Enabled:
        alarm.enabled = value != 0;
        break;
    }
}

int alarm_field_max(AlarmField field) {
    switch (field) {
    case AlarmField::Hour:
        return 23;
    case AlarmField::Minute:
        return 59;
    case AlarmField::Enabled:
        return 1;
    default:
        return 0;
    }
}

bool find_alarm_lowest_valid_with_prefix(AlarmField field,
                                         int prefix_len,
                                         int prefix,
                                         int* value) {
    const int max_value = alarm_field_max(field);
    for (int candidate = 0; candidate <= max_value; ++candidate) {
        if (field_has_prefix_value(candidate, 2, prefix_len, prefix)) {
            *value = candidate;
            return true;
        }
    }
    return false;
}

bool replace_alarm_digit_prefix_valid(AlarmEditModel* model, uint8_t digit) {
    if (model->field == AlarmField::Enabled) {
        return false;
    }
    const int current = get_alarm_field_value(*model, model->field);
    int digits[2] = {current / 10, current % 10};
    digits[model->digit_index] = digit;
    const int candidate = digits[0] * 10 + digits[1];
    const int prefix_len = model->digit_index + 1;
    int prefix = 0;
    for (int i = 0; i < prefix_len; ++i) {
        prefix = prefix * 10 + digits[i];
    }

    int normalized = candidate;
    if (candidate > alarm_field_max(model->field)) {
        if (!find_alarm_lowest_valid_with_prefix(model->field,
                                                 prefix_len,
                                                 prefix,
                                                 &normalized)) {
            set_alarm_status(model, "Invalid digit");
            return false;
        }
    }

    set_alarm_field_value(model, model->field, normalized);
    set_alarm_status(model, "Enter=save Esc=back");
    return true;
}

void handle_alarm_left(AlarmEditModel* model) {
    if (model->selection == AlarmSelectionMode::Row) {
        model->selection = AlarmSelectionMode::Field;
        model->field = AlarmField::Hour;
        model->digit_index = 0;
    } else if (model->selection == AlarmSelectionMode::Field) {
        if (model->field == AlarmField::Hour) {
            model->selection = AlarmSelectionMode::Row;
        } else {
            model->field = previous_alarm_field(model->field);
        }
    } else if (model->digit_index > 0) {
        --model->digit_index;
    } else {
        model->selection = AlarmSelectionMode::Field;
    }
}

void handle_alarm_right(AlarmEditModel* model) {
    if (model->selection == AlarmSelectionMode::Row) {
        model->selection = AlarmSelectionMode::Field;
        model->field = AlarmField::Hour;
        model->digit_index = 0;
    } else if (model->selection == AlarmSelectionMode::Field) {
        if (model->field == AlarmField::Enabled) {
            model->selection = AlarmSelectionMode::Row;
        } else {
            model->field = next_alarm_field(model->field);
        }
    } else if (model->digit_index == 0) {
        ++model->digit_index;
    } else {
        model->selection = AlarmSelectionMode::Field;
    }
}

void handle_alarm_up_down(AlarmEditModel* model, int delta) {
    if (model->selection == AlarmSelectionMode::Row) {
        int row = static_cast<int>(model->selected_index) - delta;
        if (row < 0) {
            row = kAlarmCount - 1;
        } else if (row >= kAlarmCount) {
            row = 0;
        }
        model->selected_index = static_cast<uint8_t>(row);
        set_alarm_status(model, "Enter=save Esc=back");
        return;
    }

    if (model->selection == AlarmSelectionMode::Field) {
        const int max_value = alarm_field_max(model->field);
        const int value = wrap_value(get_alarm_field_value(*model, model->field) + delta,
                                     0, max_value);
        set_alarm_field_value(model, model->field, value);
        set_alarm_status(model, "Enter=save Esc=back");
        return;
    }

    int current = get_alarm_field_value(*model, model->field);
    int digits[2] = {current / 10, current % 10};
    digits[model->digit_index] = wrap_value(digits[model->digit_index] + delta, 0, 9);
    int candidate = digits[0] * 10 + digits[1];
    if (candidate > alarm_field_max(model->field)) {
        candidate = alarm_field_max(model->field);
    }
    set_alarm_field_value(model, model->field, candidate);
    set_alarm_status(model, "Enter=save Esc=back");
}

void handle_alarm_digit(AlarmEditModel* model, uint8_t digit) {
    if (model->selection == AlarmSelectionMode::Row ||
        model->field == AlarmField::Enabled) {
        return;
    }
    if (model->selection == AlarmSelectionMode::Field) {
        model->selection = AlarmSelectionMode::Digit;
        model->digit_index = 0;
    }
    if (!replace_alarm_digit_prefix_valid(model, digit)) {
        return;
    }
    if (model->digit_index == 0) {
        ++model->digit_index;
    } else {
        model->selection = AlarmSelectionMode::Field;
    }
}

void handle_alarm_escape(AlarmEditModel* model, UiMode* ui_mode, bool* redraw_clock) {
    if (model->selection == AlarmSelectionMode::Digit) {
        model->selection = AlarmSelectionMode::Field;
        return;
    }
    if (model->selection == AlarmSelectionMode::Field) {
        model->selection = AlarmSelectionMode::Row;
        return;
    }
    *ui_mode = UiMode::Clock;
    *redraw_clock = true;
    std::puts("ALARM edit cancel");
}

void set_settings_status(SettingsEditModel* model, const char* text) {
    std::snprintf(model->status, sizeof(model->status), "%s", text);
}

SettingsEditModel make_settings_edit_model(const AppSettings& settings) {
    SettingsEditModel model = {};
    model.settings = settings;
    model.selected_index = 0;
    set_settings_status(&model, "Enter=save Esc=cancel");
    return model;
}

void draw_settings_screen(const SettingsEditModel& model) {
    constexpr int kTitleY = 28;
    constexpr int kRowX = 44;
    constexpr int kRowY = 92;
    constexpr int kRowH = 34;
    constexpr int kRowW = 232;

    picoment::display::clear(kBlack);
    picoment::display::draw_spleen_native_text_band(
        62, kTitleY, 196, 24, "SETTINGS",
        picoment::font::SpleenNativeSize::S12x24, kDim, kBlack);

    const char* seconds_value = model.settings.show_seconds ? "ON" : "OFF";
    const char* style_value =
        model.settings.clock_style == kClockStyleAnalog ? "ANALOG" : "DIGITAL";
    char seconds_line[32];
    char style_line[32];
    std::snprintf(seconds_line, sizeof(seconds_line), "Seconds  %s", seconds_value);
    std::snprintf(style_line, sizeof(style_line), "Style    %s", style_value);
    const char* rows[2] = {
        seconds_line,
        style_line,
    };

    for (uint8_t row = 0; row < 2; ++row) {
        const int y = kRowY + row * kRowH;
        const bool selected = model.selected_index == row;
        picoment::display::fill_rect(32, y, kRowW, 26,
                                     selected ? kHighlight : kBlack);
        picoment::display::draw_spleen_native_text_band(
            kRowX, y, kRowW - 24, 24, rows[row],
            picoment::font::SpleenNativeSize::S12x24,
            selected ? kHighlightText : kWhite,
            selected ? kHighlight : kBlack);
    }

    picoment::display::draw_text_band(
        42, 174, 236, 18, "Style switches clock face", kDim, kBlack);
    picoment::display::draw_text_band(
        32, 250, 256, 18, model.status, kDim, kBlack);
}

void handle_settings_up_down(SettingsEditModel* model, int delta) {
    int row = static_cast<int>(model->selected_index) + delta;
    if (row < 0) {
        row = 1;
    } else if (row > 1) {
        row = 0;
    }
    model->selected_index = static_cast<uint8_t>(row);
    set_settings_status(model, "Left/Right toggles");
}

void handle_settings_toggle(SettingsEditModel* model) {
    if (model->selected_index == 0) {
        model->settings.show_seconds = !model->settings.show_seconds;
        set_settings_status(model, "Enter=save Esc=cancel");
    } else {
        model->settings.clock_style =
            model->settings.clock_style == kClockStyleAnalog
                ? kClockStyleDigital
                : kClockStyleAnalog;
        set_settings_status(model, "Enter=save Esc=cancel");
    }
}

bool uart_should_stay_awake(const BatteryStatus& battery) {
    return battery.ok && (battery.charging || battery.percent >= 100);
}

bool parse_date_arg(const char* arg, uint16_t* year, uint8_t* month, uint8_t* day) {
    if (std::strlen(arg) != 10 || arg[4] != '-' || arg[7] != '-') {
        return false;
    }
    int y = 0;
    int m = 0;
    int d = 0;
    char extra = '\0';
    if (std::sscanf(arg, "%d-%d-%d%c", &y, &m, &d, &extra) != 3) {
        return false;
    }
    if (y < 2000 || y > 2099 || m < 1 || m > 12 || d < 1 || d > 31) {
        return false;
    }
    ds3231_datetime_t candidate = {};
    candidate.year = static_cast<uint16_t>(y);
    candidate.month = static_cast<uint8_t>(m);
    candidate.day = static_cast<uint8_t>(d);
    candidate.hour = 0;
    candidate.minute = 0;
    candidate.second = 0;
    candidate.day_of_week =
        ds3231_calculate_day_of_week(candidate.year, candidate.month, candidate.day);
    if (!is_valid_datetime(candidate)) {
        return false;
    }
    *year = candidate.year;
    *month = candidate.month;
    *day = candidate.day;
    return true;
}

bool parse_time_arg(const char* arg, uint8_t* hour, uint8_t* minute, uint8_t* second) {
    if (std::strlen(arg) != 8 || arg[2] != ':' || arg[5] != ':') {
        return false;
    }
    int h = 0;
    int m = 0;
    int s = 0;
    char extra = '\0';
    if (std::sscanf(arg, "%d:%d:%d%c", &h, &m, &s, &extra) != 3) {
        return false;
    }
    if (h < 0 || h > 23 || m < 0 || m > 59 || s < 0 || s > 59) {
        return false;
    }
    *hour = static_cast<uint8_t>(h);
    *minute = static_cast<uint8_t>(m);
    *second = static_cast<uint8_t>(s);
    return true;
}

void print_help() {
    std::puts("Commands:");
    std::puts("  help");
    std::puts("  ?");
    std::puts("  set yyyy-mm-dd");
    std::puts("  set HH:MM:SS");

    ds3231_datetime_t dt = {};
    if (ds3231_read_time(CLOCK_I2C_PORT, &dt) && is_valid_datetime(dt)) {
        std::printf("Current: ");
        print_datetime(dt);
    } else {
        std::puts("Current: RTC read failed");
    }
}

void print_prompt() {
    std::printf("%s", kPrompt);
    std::fflush(stdout);
}

void handle_set_command(const char* arg) {
    while (*arg == ' ') {
        ++arg;
    }
    if (*arg == '\0') {
        std::puts("SET FAIL");
        return;
    }

    ds3231_datetime_t dt = {};
    if (!ds3231_read_time(CLOCK_I2C_PORT, &dt) || !is_valid_datetime(dt)) {
        std::puts("SET FAIL");
        return;
    }

    uint16_t year = 0;
    uint8_t month = 0;
    uint8_t day = 0;
    uint8_t hour = 0;
    uint8_t minute = 0;
    uint8_t second = 0;

    if (parse_date_arg(arg, &year, &month, &day)) {
        dt.year = year;
        dt.month = month;
        dt.day = day;
        dt.day_of_week = ds3231_calculate_day_of_week(year, month, day);
    } else if (parse_time_arg(arg, &hour, &minute, &second)) {
        dt.hour = hour;
        dt.minute = minute;
        dt.second = second;
        dt.day_of_week = ds3231_calculate_day_of_week(dt.year, dt.month, dt.day);
    } else {
        std::puts("SET FAIL");
        return;
    }

    ds3231_datetime_t after = {};
    if (!ds3231_write_time(CLOCK_I2C_PORT, &dt) ||
        !ds3231_read_time(CLOCK_I2C_PORT, &after) ||
        !is_valid_datetime(after)) {
        std::puts("SET FAIL");
        return;
    }

    std::puts("SET OK");
    print_datetime(after);
}

void handle_uart_command(char* line) {
    while (*line == ' ') {
        ++line;
    }
    if (*line == '\0') {
        return;
    }
    if (std::strcmp(line, "help") == 0 || std::strcmp(line, "?") == 0) {
        print_help();
        return;
    }
    if (std::strncmp(line, "set ", 4) == 0) {
        handle_set_command(line + 4);
        return;
    }
    std::puts("UNKNOWN COMMAND");
}

bool poll_uart_commands() {
    static char line[64];
    static size_t used = 0;
    static bool prompt_visible = false;
    bool had_input = false;

    if (!prompt_visible) {
        print_prompt();
        prompt_visible = true;
    }

    while (true) {
        int ch = getchar_timeout_us(0);
        if (ch == PICO_ERROR_TIMEOUT) {
            return had_input;
        }
        had_input = true;
        if (ch == '\r' || ch == '\n') {
            std::printf("\r\n");
            if (used > 0) {
                line[used] = '\0';
                handle_uart_command(line);
                used = 0;
            }
            print_prompt();
            prompt_visible = true;
            continue;
        }
        if (ch == '\b' || ch == 0x7f) {
            if (used > 0) {
                --used;
                std::printf("\b \b");
                std::fflush(stdout);
            }
            continue;
        }
        if (ch >= 0x20 && ch <= 0x7e && used + 1 < sizeof(line)) {
            const char echoed = static_cast<char>(ch);
            line[used++] = echoed;
            std::printf("%c", echoed);
            std::fflush(stdout);
        }
    }
}

uint32_t main_loop_sleep_ms(uint32_t next_rtc_read_ms,
                            bool uart_poll_enabled,
                            bool ui_poll_enabled) {
    const uint32_t now_ms = to_ms_since_boot(get_absolute_time());
    uint32_t wait_ms = ms_until(now_ms, next_rtc_read_ms);

    if (ui_poll_enabled && wait_ms > kUiSleepCapMs) {
        wait_ms = kUiSleepCapMs;
    }
    if (uart_poll_enabled && wait_ms > kMainLoopActiveSleepMs) {
        wait_ms = kMainLoopActiveSleepMs;
    }
    if (!uart_poll_enabled && wait_ms > kMainLoopMaxSleepMs) {
        wait_ms = kMainLoopMaxSleepMs;
    }
    if (wait_ms == 0) {
        wait_ms = 1;
    }
    return wait_ms;
}

}  // namespace

int main() {
    stdio_init_all();
    sleep_ms(200);

    print_build_id();
    std::puts("CLOCK MVP init display -> keyboard -> i2c probes -> rtc display");

    picoment::display::init();
    picoment::keyboard::init();
    i2c_bus_init(CLOCK_I2C_SPEED_HZ);
    ProbeResult probes = run_startup_probes();
    BatteryStatus startup_battery = read_battery_status();
    std::printf("STARTUP BATTERY %s raw=0x%02X percent=%u charging=%u\r\n",
                startup_battery.ok ? "PASS" : "FAIL",
                startup_battery.raw,
                startup_battery.percent,
                startup_battery.charging ? 1u : 0u);
    AlarmSettings alarms[kAlarmCount];
    set_default_alarms(alarms);
    AppSettings app_settings = default_app_settings();
    uint32_t settings_sequence = 0;
    if (probes.eeprom_ok) {
        (void)load_settings_from_eeprom(alarms, &app_settings, &settings_sequence);
    } else {
        std::puts("SETTINGS eeprom load skip reason=probe_fail");
    }
    print_help();
    draw_clock_frame();

    char previous_date[40] = "";
    char previous_time[9] = "        ";
    char previous_battery[16] = "";
    char previous_alarm[24] = "";
    uint8_t previous_style = 0xff;
    AnalogHandState previous_analog_hand = {};
    uint8_t last_second = 255;
    bool have_rtc_sample = false;
    ds3231_datetime_t latest_dt = {};
    bool latest_dt_valid = false;
    bool latest_rtc_ok = false;
    bool colon_visible = true;
    uint32_t next_rtc_read_ms = 0;
    uint32_t next_colon_blink_ms = 0;
    bool uart_poll_enabled = uart_should_stay_awake(startup_battery);
    UiMode ui_mode = UiMode::Clock;
    SetTimeModel set_time = {};
    AlarmEditModel alarm_edit = {};
    SettingsEditModel settings_edit = {};
    AlarmFireRecord last_alarm_fire = {};
    AlarmMatch ringing_alarm = {};
    ds3231_datetime_t ringing_dt = {};
    uint32_t alarm_started_ms = 0;

    auto force_clock_redraw = [&]() {
        draw_clock_frame();
        previous_date[0] = '\0';
        std::snprintf(previous_time, sizeof(previous_time), "        ");
        previous_battery[0] = '\0';
        previous_alarm[0] = '\0';
        previous_style = 0xff;
        previous_analog_hand.valid = false;
        have_rtc_sample = false;
        latest_dt_valid = false;
        latest_rtc_ok = false;
        last_second = 255;
        next_rtc_read_ms = 0;
        next_colon_blink_ms = 0;
        colon_visible = true;
    };

    while (true) {
        const uint32_t now_ms = to_ms_since_boot(get_absolute_time());

        alarm_sound_service(now_ms);
        if (ui_mode == UiMode::AlarmRinging &&
            time_reached(now_ms, alarm_started_ms + kAlarmAutoStopMs)) {
            alarm_sound_stop();
            record_alarm_minute(&last_alarm_fire, ringing_dt);
            std::puts("ALARM auto stop timeout=60s");
            ui_mode = UiMode::Clock;
            std::puts("UI mode=clock");
            force_clock_redraw();
        }

        if (uart_poll_enabled) {
            (void)poll_uart_commands();
        }

        picoment::keyboard::KeyEvent event = {};
        while (picoment::keyboard::read_event(&event)) {
            if (event.state != picoment::keyboard::KeyState::Pressed) {
                continue;
            }

            if (ui_mode == UiMode::Clock) {
                if (event.key == picoment::keys::F6) {
                    alarm_edit = make_alarm_edit_model(alarms);
                    ui_mode = UiMode::SetAlarm;
                    std::puts("UI mode=set-alarm");
                    draw_set_alarm_screen_full(alarm_edit);
                } else if (event.key == picoment::keys::F7) {
                    settings_edit = make_settings_edit_model(app_settings);
                    ui_mode = UiMode::SetSettings;
                    std::puts("UI mode=settings");
                    draw_settings_screen(settings_edit);
                } else if (event.key == picoment::keys::F8) {
                    ds3231_datetime_t dt = {};
                    if (ds3231_read_time(CLOCK_I2C_PORT, &dt) &&
                        is_valid_datetime(dt)) {
                        set_time = make_set_time_model(dt);
                        ui_mode = UiMode::SetTime;
                        std::puts("UI mode=set-time");
                        draw_set_time_screen(set_time);
                    } else {
                        std::puts("SETTIME enter fail: RTC read failed");
                    }
                }
                continue;
            }

            if (ui_mode == UiMode::SetTime) {
                bool redraw_set_time = true;
                switch (event.key) {
                case picoment::keys::Left:
                    handle_set_time_left(&set_time);
                    break;
                case picoment::keys::Right:
                    handle_set_time_right(&set_time);
                    break;
                case picoment::keys::Up:
                    handle_set_time_up_down(&set_time, 1);
                    break;
                case picoment::keys::Down:
                    handle_set_time_up_down(&set_time, -1);
                    break;
                case picoment::keys::Enter: {
                    ds3231_datetime_t dt = model_to_datetime(set_time);
                    ds3231_datetime_t after = {};
                    std::printf("SETTIME write start %04u-%02u-%02u %02u:%02u:%02u\r\n",
                                dt.year, dt.month, dt.day,
                                dt.hour, dt.minute, dt.second);
                    if (dt.day_of_week != 0 &&
                        ds3231_write_time(CLOCK_I2C_PORT, &dt) &&
                        ds3231_read_time(CLOCK_I2C_PORT, &after) &&
                        is_valid_datetime(after)) {
                        std::puts("SETTIME write ok");
                        ui_mode = UiMode::Clock;
                        std::puts("UI mode=clock");
                        force_clock_redraw();
                        redraw_set_time = false;
                    } else {
                        std::puts("SETTIME write fail");
                        set_status(&set_time, "SET FAIL");
                    }
                    break;
                }
                case picoment::keys::Escape:
                    ui_mode = UiMode::Clock;
                    std::puts("SETTIME cancel");
                    std::puts("UI mode=clock");
                    force_clock_redraw();
                    redraw_set_time = false;
                    break;
                default:
                    if (event.key >= '0' && event.key <= '9') {
                        handle_set_time_digit(&set_time,
                                              static_cast<uint8_t>(event.key - '0'));
                    } else {
                        redraw_set_time = false;
                    }
                    break;
                }

                if (ui_mode == UiMode::SetTime && redraw_set_time) {
                    draw_set_time_screen(set_time);
                }
                continue;
            }

            if (ui_mode == UiMode::SetAlarm) {
                bool redraw_alarm = true;
                bool redraw_clock = false;
                switch (event.key) {
                case picoment::keys::Left:
                    handle_alarm_left(&alarm_edit);
                    break;
                case picoment::keys::Right:
                    handle_alarm_right(&alarm_edit);
                    break;
                case picoment::keys::Up:
                    handle_alarm_up_down(&alarm_edit, 1);
                    break;
                case picoment::keys::Down:
                    handle_alarm_up_down(&alarm_edit, -1);
                    break;
                case picoment::keys::Enter: {
                    const bool changed = !alarms_equal(alarms, alarm_edit.alarms);
                    for (uint8_t i = 0; i < kAlarmCount; ++i) {
                        alarms[i] = alarm_edit.alarms[i];
                    }
                    if (changed) {
                        if (probes.eeprom_ok) {
                            (void)save_settings_to_eeprom(alarms,
                                                          app_settings,
                                                          &settings_sequence);
                        } else {
                            std::puts("SETTINGS eeprom save skip reason=probe_fail");
                        }
                    } else {
                        std::puts("SETTINGS eeprom save skip reason=unchanged");
                    }
                    if (latest_dt_valid &&
                        find_alarm_match(alarms, latest_dt).found) {
                        record_alarm_minute(&last_alarm_fire, latest_dt);
                        std::puts("ALARM suppress same minute");
                    }
                    std::printf("ALARM settings A1=%u %02u:%02u A2=%u %02u:%02u A3=%u %02u:%02u A4=%u %02u:%02u A5=%u %02u:%02u\r\n",
                                alarms[0].enabled ? 1u : 0u, alarms[0].hour, alarms[0].minute,
                                alarms[1].enabled ? 1u : 0u, alarms[1].hour, alarms[1].minute,
                                alarms[2].enabled ? 1u : 0u, alarms[2].hour, alarms[2].minute,
                                alarms[3].enabled ? 1u : 0u, alarms[3].hour, alarms[3].minute,
                                alarms[4].enabled ? 1u : 0u, alarms[4].hour, alarms[4].minute);
                    ui_mode = UiMode::Clock;
                    std::puts("UI mode=clock");
                    force_clock_redraw();
                    redraw_alarm = false;
                    break;
                }
                case picoment::keys::Escape:
                    handle_alarm_escape(&alarm_edit, &ui_mode, &redraw_clock);
                    if (redraw_clock) {
                        std::puts("UI mode=clock");
                        force_clock_redraw();
                        redraw_alarm = false;
                    }
                    break;
                default:
                    if (event.key >= '0' && event.key <= '9') {
                        handle_alarm_digit(&alarm_edit,
                                           static_cast<uint8_t>(event.key - '0'));
                    } else {
                        redraw_alarm = false;
                    }
                    break;
                }

                if (ui_mode == UiMode::SetAlarm && redraw_alarm) {
                    draw_set_alarm_screen(alarm_edit);
                }
                continue;
            }

            if (ui_mode == UiMode::SetSettings) {
                bool redraw_settings = true;
                switch (event.key) {
                case picoment::keys::Up:
                    handle_settings_up_down(&settings_edit, -1);
                    break;
                case picoment::keys::Down:
                    handle_settings_up_down(&settings_edit, 1);
                    break;
                case picoment::keys::Left:
                case picoment::keys::Right:
                case picoment::keys::Space:
                    handle_settings_toggle(&settings_edit);
                    break;
                case picoment::keys::Enter: {
                    const bool changed =
                        !app_settings_equal(app_settings, settings_edit.settings);
                    app_settings = settings_edit.settings;
                    if (changed) {
                        if (probes.eeprom_ok) {
                            (void)save_settings_to_eeprom(alarms,
                                                          app_settings,
                                                          &settings_sequence);
                        } else {
                            std::puts("SETTINGS eeprom save skip reason=probe_fail");
                        }
                    } else {
                        std::puts("SETTINGS eeprom save skip reason=unchanged");
                    }
                    std::printf("SETTINGS app seconds=%u style=%s\r\n",
                                app_settings.show_seconds ? 1u : 0u,
                                app_settings.clock_style == kClockStyleAnalog
                                    ? "analog"
                                    : "digital");
                    ui_mode = UiMode::Clock;
                    std::puts("UI mode=clock");
                    force_clock_redraw();
                    redraw_settings = false;
                    break;
                }
                case picoment::keys::Escape:
                    ui_mode = UiMode::Clock;
                    std::puts("SETTINGS cancel");
                    std::puts("UI mode=clock");
                    force_clock_redraw();
                    redraw_settings = false;
                    break;
                default:
                    redraw_settings = false;
                    break;
                }

                if (ui_mode == UiMode::SetSettings && redraw_settings) {
                    draw_settings_screen(settings_edit);
                }
                continue;
            }

            if (ui_mode == UiMode::AlarmRinging) {
                if (event.key == picoment::keys::Space) {
                    alarm_sound_stop();
                    record_alarm_minute(&last_alarm_fire, ringing_dt);
                    std::puts("ALARM stopped by Space");
                    ui_mode = UiMode::Clock;
                    std::puts("UI mode=clock");
                    force_clock_redraw();
                }
                continue;
            }
        }

        if (ui_mode == UiMode::Clock && time_reached(now_ms, next_rtc_read_ms)) {

            ds3231_datetime_t dt = {};
            const bool rtc_ok = ds3231_read_time(CLOCK_I2C_PORT, &dt) &&
                                is_valid_datetime(dt);
            if (rtc_ok && !have_rtc_sample) {
                latest_dt = dt;
                latest_dt_valid = true;
                latest_rtc_ok = true;
                last_second = dt.second;
                have_rtc_sample = true;
                next_rtc_read_ms = now_ms + kRtcSearchPollMs;
            } else if (rtc_ok && dt.second != last_second) {
                latest_dt = dt;
                latest_dt_valid = true;
                latest_rtc_ok = true;
                BatteryStatus battery = read_battery_status();
                uart_poll_enabled = uart_should_stay_awake(battery);
                if (app_settings.clock_style == kClockStyleAnalog) {
                    const bool force_full_redraw =
                        previous_style != app_settings.clock_style;
                    draw_analog_clock(dt, true, battery, alarms, last_alarm_fire,
                                      app_settings.show_seconds,
                                      force_full_redraw,
                                      previous_date, previous_battery,
                                      previous_alarm, &previous_analog_hand);
                    previous_style = app_settings.clock_style;
                } else {
                    if (previous_style != app_settings.clock_style) {
                        draw_clock_frame();
                        previous_date[0] = '\0';
                        std::snprintf(previous_time, sizeof(previous_time), "        ");
                        previous_battery[0] = '\0';
                        previous_alarm[0] = '\0';
                        previous_style = app_settings.clock_style;
                    }
                    char date_line[40];
                    char time_line[24];
                    format_clock_lines(dt, true, app_settings.show_seconds,
                                       date_line, sizeof(date_line),
                                       time_line, sizeof(time_line));
                    if (!app_settings.show_seconds && !colon_visible) {
                        time_line[2] = ' ';
                    }
                    draw_clock_delta(date_line, time_line,
                                     previous_date, previous_time, true);
                    draw_battery_delta(battery, previous_battery);
                    draw_alarm_delta(alarms, dt, last_alarm_fire, previous_alarm);
                }
                AlarmMatch alarm_match = find_alarm_match(alarms, dt);
                if (alarm_match.found && !same_alarm_minute(last_alarm_fire, dt)) {
                    ringing_alarm = alarm_match;
                    ringing_dt = dt;
                    alarm_started_ms = now_ms;
                    ui_mode = UiMode::AlarmRinging;
                    alarm_sound_start(now_ms);
                    draw_alarm_ringing_screen(ringing_alarm);
                    std::printf("ALARM fire date=%04u-%02u-%02u time=%02u:%02u alarms=A%u%s\r\n",
                                dt.year, dt.month, dt.day,
                                dt.hour, dt.minute,
                                alarm_match.first_index + 1u,
                                alarm_match.count > 1 ? "+" : "");
                }
                last_second = dt.second;
                next_rtc_read_ms = now_ms + kRtcRestAfterTickMs;
            } else if (rtc_ok) {
                latest_dt = dt;
                latest_dt_valid = true;
                latest_rtc_ok = true;
                next_rtc_read_ms = now_ms + kRtcSearchPollMs;
            } else if (!rtc_ok) {
                have_rtc_sample = false;
                latest_dt_valid = false;
                latest_rtc_ok = false;
                last_second = 255;
                BatteryStatus battery = read_battery_status();
                uart_poll_enabled = uart_should_stay_awake(battery);
                if (app_settings.clock_style == kClockStyleAnalog) {
                    const bool force_full_redraw =
                        previous_style != app_settings.clock_style;
                    draw_analog_clock(dt, false, battery, alarms, last_alarm_fire,
                                      app_settings.show_seconds,
                                      force_full_redraw,
                                      previous_date, previous_battery,
                                      previous_alarm, &previous_analog_hand);
                    previous_style = app_settings.clock_style;
                } else {
                    if (previous_style != app_settings.clock_style) {
                        draw_clock_frame();
                        previous_date[0] = '\0';
                        std::snprintf(previous_time, sizeof(previous_time), "        ");
                        previous_battery[0] = '\0';
                        previous_alarm[0] = '\0';
                        previous_style = app_settings.clock_style;
                    }
                    char date_line[40];
                    char time_line[24];
                    format_clock_lines(dt, false, app_settings.show_seconds,
                                       date_line, sizeof(date_line),
                                       time_line, sizeof(time_line));
                    if (!app_settings.show_seconds && !colon_visible) {
                        time_line[2] = ' ';
                    }
                    draw_clock_delta(date_line, time_line,
                                     previous_date, previous_time, false);
                    draw_battery_delta(battery, previous_battery);
                    picoment::display::fill_rect(kAlarmBandX, kAlarmBandY,
                                                 kAlarmBandW, kAlarmBandH, kBlack);
                    previous_alarm[0] = '\0';
                }
                next_rtc_read_ms = now_ms + kRtcFailRetryMs;
            }
        }

        if (ui_mode == UiMode::Clock &&
            app_settings.clock_style == kClockStyleDigital &&
            !app_settings.show_seconds &&
            time_reached(now_ms, next_colon_blink_ms)) {
            if (std::strlen(previous_time) == 5) {
                colon_visible = !colon_visible;
                draw_no_seconds_colon(colon_visible, latest_rtc_ok, previous_time);
            }
            next_colon_blink_ms = now_ms + kColonBlinkMs;
        }

        if (ui_mode == UiMode::AlarmRinging) {
            sleep_ms(kAlarmLoopSleepMs);
        } else if (ui_mode != UiMode::Clock) {
            sleep_ms(uart_poll_enabled ? kMainLoopActiveSleepMs : kUiSleepCapMs);
        } else {
            uint32_t sleep_ms_value =
                main_loop_sleep_ms(next_rtc_read_ms, uart_poll_enabled, true);
            if (app_settings.clock_style == kClockStyleDigital &&
                !app_settings.show_seconds) {
                const uint32_t blink_wait_ms = ms_until(now_ms, next_colon_blink_ms);
                if (blink_wait_ms < sleep_ms_value) {
                    sleep_ms_value = blink_wait_ms;
                }
                if (sleep_ms_value == 0) {
                    sleep_ms_value = 1;
                }
            }
            sleep_ms(sleep_ms_value);
        }
    }
}
