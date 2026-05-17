#include <cstdio>
#include <cstring>
#include <cstdlib>

#include "hardware/gpio.h"
#include "hardware/i2c.h"
#include "pico/stdlib.h"

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
constexpr uint16_t kHighlight = 0xff80;
constexpr uint16_t kHighlightDigit = 0x07ff;
constexpr uint16_t kHighlightText = 0x0000;
constexpr uint32_t kRtcSearchPollMs = 47;
constexpr uint32_t kRtcRestAfterTickMs = 900;
constexpr uint32_t kRtcFailRetryMs = 200;
constexpr uint32_t kMainLoopActiveSleepMs = 10;
constexpr uint32_t kMainLoopMaxSleepMs = 900;
constexpr uint32_t kUiSleepCapMs = 20;
constexpr int kDateBandX = 52;
constexpr int kDateY = 82;
constexpr int kDateBandW = 216;
constexpr int kDateH = 24;
constexpr int kTimeX = 32;
constexpr int kTimeY = 128;
constexpr int kTimeCharW = 32;
constexpr int kTimeH = 64;
constexpr const char* kPrompt = "> ";
constexpr int kHeaderY = 12;
constexpr int kHeaderH = 18;
constexpr int kBatteryBandX = 214;
constexpr int kBatteryBandW = 96;

enum class UiMode {
    Clock,
    SetTime,
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
                        char* date_line,
                        size_t date_len,
                        char* time_line,
                        size_t time_len) {

    if (rtc_ok) {
        const int weekday = weekday_from_date(dt.year, dt.month, dt.day);
        std::snprintf(date_line, date_len, "%04u-%02u-%02u %s",
                      dt.year, dt.month, dt.day, weekday_name(weekday));
        std::snprintf(time_line, time_len, "%02u:%02u:%02u",
                      dt.hour, dt.minute, dt.second);
    } else {
        std::snprintf(date_line, date_len, "---- -- -- ---");
        std::snprintf(time_line, time_len, "--:--:--");
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

    for (int i = 0; i < 8; ++i) {
        if (time_line[i] != previous_time[i]) {
            char ch[2] = {time_line[i], '\0'};
            picoment::display::draw_spleen_native_text_band(
                kTimeX + i * kTimeCharW, kTimeY, kTimeCharW, kTimeH, ch,
                picoment::font::SpleenNativeSize::S32x64,
                rtc_ok ? kWhite : kWarn, kBlack);
            previous_time[i] = time_line[i];
        }
    }
    previous_time[8] = '\0';
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
    (void)probes;
    BatteryStatus startup_battery = read_battery_status();
    std::printf("STARTUP BATTERY %s raw=0x%02X percent=%u charging=%u\r\n",
                startup_battery.ok ? "PASS" : "FAIL",
                startup_battery.raw,
                startup_battery.percent,
                startup_battery.charging ? 1u : 0u);
    print_help();
    draw_clock_frame();

    char previous_date[40] = "";
    char previous_time[9] = "        ";
    char previous_battery[16] = "";
    uint8_t last_second = 255;
    bool have_rtc_sample = false;
    uint32_t next_rtc_read_ms = 0;
    bool uart_poll_enabled = uart_should_stay_awake(startup_battery);
    UiMode ui_mode = UiMode::Clock;
    SetTimeModel set_time = {};

    auto force_clock_redraw = [&]() {
        draw_clock_frame();
        previous_date[0] = '\0';
        std::snprintf(previous_time, sizeof(previous_time), "        ");
        previous_battery[0] = '\0';
        have_rtc_sample = false;
        last_second = 255;
        next_rtc_read_ms = 0;
    };

    while (true) {
        const uint32_t now_ms = to_ms_since_boot(get_absolute_time());

        if (uart_poll_enabled) {
            (void)poll_uart_commands();
        }

        picoment::keyboard::KeyEvent event = {};
        while (picoment::keyboard::read_event(&event)) {
            if (event.state != picoment::keyboard::KeyState::Pressed) {
                continue;
            }

            if (ui_mode == UiMode::Clock) {
                if (event.key == picoment::keys::F8) {
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
        }

        if (ui_mode == UiMode::Clock && time_reached(now_ms, next_rtc_read_ms)) {

            ds3231_datetime_t dt = {};
            const bool rtc_ok = ds3231_read_time(CLOCK_I2C_PORT, &dt) &&
                                is_valid_datetime(dt);
            if (rtc_ok && !have_rtc_sample) {
                last_second = dt.second;
                have_rtc_sample = true;
                next_rtc_read_ms = now_ms + kRtcSearchPollMs;
            } else if (rtc_ok && dt.second != last_second) {
                BatteryStatus battery = read_battery_status();
                uart_poll_enabled = uart_should_stay_awake(battery);
                char date_line[40];
                char time_line[24];
                format_clock_lines(dt, true, date_line, sizeof(date_line),
                                   time_line, sizeof(time_line));
                draw_clock_delta(date_line, time_line,
                                 previous_date, previous_time, true);
                draw_battery_delta(battery, previous_battery);
                last_second = dt.second;
                next_rtc_read_ms = now_ms + kRtcRestAfterTickMs;
            } else if (rtc_ok) {
                next_rtc_read_ms = now_ms + kRtcSearchPollMs;
            } else if (!rtc_ok) {
                have_rtc_sample = false;
                last_second = 255;
                BatteryStatus battery = read_battery_status();
                uart_poll_enabled = uart_should_stay_awake(battery);
                char date_line[40];
                char time_line[24];
                format_clock_lines(dt, false, date_line, sizeof(date_line),
                                   time_line, sizeof(time_line));
                draw_clock_delta(date_line, time_line,
                                 previous_date, previous_time, false);
                draw_battery_delta(battery, previous_battery);
                next_rtc_read_ms = now_ms + kRtcFailRetryMs;
            }
        }

        if (ui_mode == UiMode::SetTime) {
            sleep_ms(uart_poll_enabled ? kMainLoopActiveSleepMs : kUiSleepCapMs);
        } else {
            sleep_ms(main_loop_sleep_ms(next_rtc_read_ms, uart_poll_enabled, true));
        }
    }
}
