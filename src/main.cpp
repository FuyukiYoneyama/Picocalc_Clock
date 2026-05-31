#include <cstdio>
#include <cstring>
#include <cstdlib>

#include "hardware/gpio.h"
#include "hardware/i2c.h"
#include "pico/stdlib.h"

#include "alarm_sound.h"
#include "alarm/alarm_model.h"
#include "alarm/alarm_ui.h"
#include "app/screenshot_service.h"
#include "app/settings_editor.h"
#include "app/set_time_editor.h"
#include "app/uart_commands.h"
#include "clock/analog_render.h"
#include "clock/calendar_render.h"
#include "clock/clock_help.h"
#include "clock/clock_render.h"
#include "clock/clock_time.h"
#include "ds3231.h"
#include "font/cozette_font.h"
#include "life/life_runtime.h"
#include "picocalc_clock_build_info.h"
#include "platform/backlight_control.h"
#include "platform/battery.h"
#include "platform/picocalc_audio_pwm.h"
#include "platform/picocalc_display.h"
#include "platform/picocalc_key_table.h"
#include "platform/picocalc_keyboard.h"
#include "platform/startup_probe.h"
#include "settings/settings_store.h"
#include "version.h"

#define CLOCK_I2C_PORT i2c1
#define CLOCK_I2C_SDA_PIN 6
#define CLOCK_I2C_SCL_PIN 7
#define CLOCK_I2C_SPEED_HZ 400000
#define I2C_ADDR_KEYBOARD 0x1F
#define I2C_ADDR_AT24C32_EXPECTED 0x57
#define I2C_SCAN_TIMEOUT_US 10000

namespace {

constexpr uint16_t kBlack = 0x0000;
constexpr uint16_t kWhite = 0xffff;
constexpr uint16_t kDim = 0x7bef;
constexpr uint16_t kWarn = 0xfde0;
constexpr uint16_t kHighlight = 0xff80;
constexpr uint16_t kHighlightDigit = 0x07ff;
constexpr uint32_t kRtcSearchPollMs = 47;
constexpr uint32_t kRtcRestAfterTickMs = 900;
constexpr uint32_t kRtcFailRetryMs = 200;
constexpr uint32_t kMainLoopActiveSleepMs = 10;
constexpr uint32_t kMainLoopMaxSleepMs = 900;
constexpr uint32_t kUiSleepCapMs = 20;
constexpr uint32_t kClockIdleKeySlowAfterMs = 60000;
constexpr uint32_t kClockIdleKeySleepCapMs = 100;
constexpr uint32_t kBatteryReadIntervalMs = 60000;
constexpr uint32_t kAlarmLoopSleepMs = 2;
constexpr uint32_t kAlarmAutoStopMs = 60000;
constexpr uint32_t kColonBlinkMs = 1000;
constexpr uint32_t kLifeHourlyMaxMs = 60000;
constexpr uint32_t kLifeLoopSleepMs = 30;
constexpr int kAlarmBandX = 78;
constexpr int kAlarmBandY = 246;
constexpr int kAlarmBandW = 164;
constexpr int kAlarmBandH = 24;

enum class UiMode {
    Clock,
    ClockHelp,
    SetTime,
    SetAlarm,
    SetSettings,
    Life,
    AlarmRinging,
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

const char* raw_key_name(uint8_t key) {
    return picoment::keys::name(key);
}

bool handle_backlight_key_event(const picoment::keyboard::KeyEvent& event,
                                UiMode ui_mode,
                                BacklightState* state) {
    const bool pressed =
        event.state == picoment::keyboard::KeyState::Pressed;
    const bool released =
        event.state == picoment::keyboard::KeyState::Released;

    if (pressed && event.key == picoment::keys::Power) {
        if (ui_mode == UiMode::AlarmRinging) {
            state->user_off = !state->user_off;
            state->space_peek_active = false;
            state->alarm_forced_on = state->user_off;
            std::printf("BACKLIGHT user=%s\r\n",
                        state->user_off ? "off" : "on");
            return true;
        }

        if (!state->user_off) {
            if (backlight_turn_off(state)) {
                state->user_off = true;
                state->space_peek_active = false;
                state->alarm_forced_on = false;
                std::puts("BACKLIGHT user=off");
            }
        } else if (backlight_cancel_user_off(state)) {
            std::puts("BACKLIGHT user=on");
        }
        return true;
    }

    if (pressed &&
        (event.key == picoment::keys::F6 ||
         event.key == picoment::keys::F7 ||
         event.key == picoment::keys::F8 ||
         event.key == 'L' ||
         event.key == 'l')) {
        if (state->user_off || state->space_peek_active ||
            state->alarm_forced_on) {
            if (backlight_cancel_user_off(state)) {
                std::printf("BACKLIGHT interactive=on key=%s\r\n",
                            raw_key_name(event.key));
            }
        }
        return false;
    }

    if (pressed && event.key == picoment::keys::Space && state->user_off &&
        !state->space_peek_active) {
        if (backlight_turn_on(state)) {
            state->space_peek_active = true;
            std::puts("BACKLIGHT peek=on");
        }
        return false;
    }

    if (released && event.key == picoment::keys::Space &&
        state->space_peek_active) {
        if (state->user_off && ui_mode != UiMode::AlarmRinging &&
            !state->alarm_forced_on) {
            if (backlight_turn_off(state)) {
                state->space_peek_active = false;
                std::puts("BACKLIGHT peek=off");
            }
        } else {
            state->space_peek_active = false;
        }
        return false;
    }

    return false;
}

uint32_t main_loop_sleep_ms(uint32_t next_rtc_read_ms,
                            bool uart_poll_enabled,
                            uint32_t ui_sleep_cap_ms) {
    const uint32_t now_ms = to_ms_since_boot(get_absolute_time());
    uint32_t wait_ms = ms_until(now_ms, next_rtc_read_ms);

    if (ui_sleep_cap_ms != 0 && wait_ms > ui_sleep_cap_ms) {
        wait_ms = ui_sleep_cap_ms;
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
    std::puts("Picocalc_Clock init display -> keyboard -> i2c probes -> rtc display");

    picoment::display::init();
    picoment::keyboard::init();
#if defined(PICO_VBUS_PIN)
    gpio_init(PICO_VBUS_PIN);
    gpio_set_dir(PICO_VBUS_PIN, GPIO_IN);
#endif
    const StartupProbeConfig startup_probe_config = {
        CLOCK_I2C_PORT,
        CLOCK_I2C_SDA_PIN,
        CLOCK_I2C_SCL_PIN,
        CLOCK_I2C_SPEED_HZ,
        I2C_ADDR_KEYBOARD,
        I2C_ADDR_AT24C32_EXPECTED,
        I2C_SCAN_TIMEOUT_US,
    };
    i2c_bus_init(startup_probe_config);
    ProbeResult probes = run_startup_probes(startup_probe_config);
    BatteryStatus startup_battery = read_battery_status(CLOCK_I2C_PORT);
    BacklightState backlight = {
        false,
        false,
        false,
        kDefaultRestoreBacklight,
    };
    remember_restore_backlight(&backlight);
    std::printf("STARTUP BATTERY %s raw=0x%02X percent=%u charging=%u\r\n",
                startup_battery.ok ? "PASS" : "FAIL",
                startup_battery.raw,
                startup_battery.percent,
                startup_battery.charging ? 1u : 0u);
#if defined(PICO_VBUS_PIN)
    std::printf("STARTUP VBUS gpio=%u present=%u\r\n",
                static_cast<unsigned>(PICO_VBUS_PIN),
                gpio_get(PICO_VBUS_PIN) ? 1u : 0u);
#endif
    AlarmSettings alarms[kAlarmCount];
    set_default_alarms(alarms);
    AppSettings app_settings = default_app_settings();
    uint32_t settings_sequence = 0;
    if (probes.eeprom_ok) {
        (void)load_settings_from_eeprom(CLOCK_I2C_PORT,
                                        alarms,
                                        &app_settings,
                                        &settings_sequence);
    } else {
        std::puts("SETTINGS eeprom load skip reason=probe_fail");
    }
    print_uart_help(CLOCK_I2C_PORT);
    draw_clock_frame();

    char previous_date[40] = "";
    char previous_time[9] = "        ";
    char previous_moon[20] = "";
    char previous_battery[16] = "";
    char previous_alarm[24] = "";
    uint8_t previous_style = 0xff;
    AnalogHandState previous_analog_hand = {};
    uint8_t last_second = 255;
    bool have_rtc_sample = false;
    ds3231_datetime_t latest_dt = {};
    bool latest_dt_valid = false;
    bool latest_rtc_ok = false;
    BatteryStatus latest_battery = startup_battery;
    bool colon_visible = true;
    uint32_t next_rtc_read_ms = 0;
    uint32_t next_colon_blink_ms = 0;
    bool uart_poll_enabled = uart_should_stay_awake();
    UiMode ui_mode = UiMode::Clock;
    SetTimeModel set_time = {};
    AlarmEditModel alarm_edit = {};
    SettingsEditModel settings_edit = {};
    AlarmFireRecord last_alarm_fire = {};
    LifeHourRecord last_life_hour = {};
    LifeRuntime life_runtime = {};
    AlarmMatch ringing_alarm = {};
    ds3231_datetime_t ringing_dt = {};
    uint32_t alarm_started_ms = 0;
    size_t clock_help_page = 0;
    bool home_active = false;
    uint32_t last_keyboard_activity_ms = to_ms_since_boot(get_absolute_time());
    uint32_t next_battery_read_ms = 0;
    bool calendar_peek_active = false;

    auto force_clock_redraw = [&]() {
        draw_clock_frame();
        previous_date[0] = '\0';
        std::snprintf(previous_time, sizeof(previous_time), "        ");
        previous_moon[0] = '\0';
        previous_battery[0] = '\0';
        previous_alarm[0] = '\0';
        previous_style = 0xff;
        previous_analog_hand.valid = false;
        life_runtime.active = false;
        have_rtc_sample = false;
        latest_dt_valid = false;
        latest_rtc_ok = false;
        last_second = 255;
        next_rtc_read_ms = 0;
        next_colon_blink_ms = 0;
        colon_visible = true;
    };

    // Home captures the current screen without changing the drawn UI. If an
    // alarm is already due, screenshot tones are muted so the alarm can take
    // over cleanly on the next loop.
    auto screenshot_alarm_pending = [&]() {
        ds3231_datetime_t dt = {};
        if (!(ds3231_read_time(CLOCK_I2C_PORT, &dt) &&
              is_valid_datetime(dt))) {
            return false;
        }
        const AlarmMatch alarm_match = find_alarm_match(alarms, dt);
        return alarm_match.found && !same_alarm_minute(last_alarm_fire, dt);
    };

    auto enter_life = [&](bool hourly) {
        calendar_peek_active = false;
        ui_mode = UiMode::Life;
        std::printf("UI mode=life source=%s\r\n", hourly ? "hourly" : "manual");
        start_life(&life_runtime, hourly, to_ms_since_boot(get_absolute_time()));
    };

    auto exit_life = [&](const char* reason) {
        stop_life(&life_runtime, reason);
        ui_mode = UiMode::Clock;
        std::puts("UI mode=clock");
        force_clock_redraw();
    };

    while (true) {
        const uint32_t now_ms = to_ms_since_boot(get_absolute_time());

        alarm_sound_service(now_ms);
        if (ui_mode == UiMode::AlarmRinging &&
            time_reached(now_ms, alarm_started_ms + kAlarmAutoStopMs)) {
            alarm_sound_stop();
            record_alarm_minute(&last_alarm_fire, ringing_dt);
            std::puts("ALARM auto stop timeout=60s");
            alarm_sound_shutdown();
            ui_mode = UiMode::Clock;
            backlight_alarm_stopped(&backlight);
            std::puts("UI mode=clock");
            force_clock_redraw();
        }

        if (uart_poll_enabled) {
            (void)poll_uart_commands(CLOCK_I2C_PORT);
        }

        picoment::keyboard::KeyEvent event = {};
        while (picoment::keyboard::read_event(&event)) {
            last_keyboard_activity_ms = now_ms;
            if (ui_mode == UiMode::Life &&
                event.key == picoment::keys::Space &&
                event.state == picoment::keyboard::KeyState::Pressed) {
                exit_life("space");
                continue;
            }
            if (handle_backlight_key_event(event, ui_mode, &backlight)) {
                continue;
            }
            if (event.key == picoment::keys::Home &&
                event.state == picoment::keyboard::KeyState::Released) {
                home_active = false;
                continue;
            }
            if (event.key == picoment::keys::Home &&
                event.state == picoment::keyboard::KeyState::Pressed &&
                !home_active) {
                home_active = true;
                const bool suppress_sounds =
                    alarm_sound_active() ||
                    ui_mode == UiMode::AlarmRinging ||
                    screenshot_alarm_pending();
                (void)capture_screenshot_with_sounds(suppress_sounds);
                continue;
            }
            if (ui_mode == UiMode::Clock &&
                (event.key == 'c' || event.key == 'C')) {
                if (event.state == picoment::keyboard::KeyState::Pressed &&
                    !calendar_peek_active) {
                    calendar_peek_active = true;
                    std::puts("CLOCK calendar peek=on");
                    force_clock_redraw();
                } else if (event.state == picoment::keyboard::KeyState::Released &&
                           calendar_peek_active) {
                    calendar_peek_active = false;
                    std::puts("CLOCK calendar peek=off");
                    force_clock_redraw();
                }
                continue;
            }
            if (event.state != picoment::keyboard::KeyState::Pressed) {
                continue;
            }

            if (ui_mode == UiMode::ClockHelp) {
                if (event.key == picoment::keys::F10 ||
                    event.key == picoment::keys::Escape) {
                    ui_mode = UiMode::Clock;
                    std::puts("UI mode=clock");
                    force_clock_redraw();
                } else if ((event.key == picoment::keys::Right ||
                            event.key == picoment::keys::Down) &&
                           clock_help_page + 1 < kClockHelpPageCount) {
                    ++clock_help_page;
                    draw_clock_help_screen(clock_help_page);
                } else if ((event.key == picoment::keys::Left ||
                            event.key == picoment::keys::Up) &&
                           clock_help_page > 0) {
                    --clock_help_page;
                    draw_clock_help_screen(clock_help_page);
                }
                continue;
            }

            if (ui_mode == UiMode::Clock) {
                if (event.key == picoment::keys::F10) {
                    calendar_peek_active = false;
                    clock_help_page = 0;
                    ui_mode = UiMode::ClockHelp;
                    std::puts("UI mode=clock-help");
                    draw_clock_help_screen(clock_help_page);
                } else if (event.key == 'L' || event.key == 'l') {
                    enter_life(false);
                } else if (event.key == picoment::keys::F6) {
                    calendar_peek_active = false;
                    alarm_edit = make_alarm_edit_model(alarms);
                    ui_mode = UiMode::SetAlarm;
                    std::puts("UI mode=set-alarm");
                    draw_set_alarm_screen_full(alarm_edit);
                } else if (event.key == picoment::keys::F7) {
                    calendar_peek_active = false;
                    settings_edit = make_settings_edit_model(app_settings);
                    ui_mode = UiMode::SetSettings;
                    std::puts("UI mode=settings");
                    draw_settings_screen(settings_edit);
                } else if (event.key == picoment::keys::F8) {
                    ds3231_datetime_t dt = {};
                    if (ds3231_read_time(CLOCK_I2C_PORT, &dt) &&
                        is_valid_datetime(dt)) {
                        set_time = make_set_time_model(dt);
                        calendar_peek_active = false;
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
                            (void)save_settings_to_eeprom(CLOCK_I2C_PORT,
                                                          alarms,
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
                            (void)save_settings_to_eeprom(CLOCK_I2C_PORT,
                                                          alarms,
                                                          app_settings,
                                                          &settings_sequence);
                        } else {
                            std::puts("SETTINGS eeprom save skip reason=probe_fail");
                        }
                    } else {
                        std::puts("SETTINGS eeprom save skip reason=unchanged");
                    }
                    const char* style_name = "digital";
                    if (app_settings.clock_style == kClockStyleAnalog) {
                        style_name = "analog";
                    } else if (app_settings.clock_style == kClockStyleCalendar) {
                        style_name = "calendar";
                    }
                    std::printf("SETTINGS app seconds=%u style=%s life=%u\r\n",
                                app_settings.show_seconds ? 1u : 0u,
                                style_name,
                                app_settings.life_hourly_enabled ? 1u : 0u);
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
                    alarm_sound_shutdown();
                    ui_mode = UiMode::Clock;
                    backlight_alarm_stopped(&backlight);
                    std::puts("UI mode=clock");
                    force_clock_redraw();
                }
                continue;
            }
        }

        if (ui_mode == UiMode::Life && life_runtime.active) {
            if (step_life(&life_runtime)) {
                exit_life("stable");
            } else if (life_runtime.hourly &&
                       time_reached(now_ms,
                                    life_runtime.started_ms + kLifeHourlyMaxMs)) {
                exit_life("timeout");
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
                if (time_reached(now_ms, next_battery_read_ms)) {
                    latest_battery = read_battery_status(CLOCK_I2C_PORT);
                    next_battery_read_ms = now_ms + kBatteryReadIntervalMs;
                }
                uart_poll_enabled = uart_should_stay_awake();
                const uint8_t active_clock_style =
                    calendar_peek_active ? kClockStyleCalendar
                                         : app_settings.clock_style;
                if (active_clock_style == kClockStyleAnalog) {
                    const bool force_full_redraw =
                        previous_style != active_clock_style;
                    draw_analog_clock(dt, true, latest_battery, alarms, last_alarm_fire,
                                      app_settings.show_seconds,
                                      force_full_redraw,
                                      previous_date, previous_moon,
                                      previous_battery,
                                      previous_alarm, &previous_analog_hand);
                    previous_style = active_clock_style;
                } else if (active_clock_style == kClockStyleCalendar) {
                    const bool force_full_redraw =
                        previous_style != active_clock_style;
                    draw_calendar_clock(dt, true, latest_battery,
                                        alarms, last_alarm_fire,
                                        app_settings.show_seconds,
                                        force_full_redraw,
                                        previous_date, previous_time,
                                        previous_moon, previous_battery,
                                        previous_alarm);
                    previous_style = active_clock_style;
                } else {
                    if (previous_style != active_clock_style) {
                        draw_clock_frame();
                        previous_date[0] = '\0';
                        std::snprintf(previous_time, sizeof(previous_time), "        ");
                        previous_moon[0] = '\0';
                        previous_battery[0] = '\0';
                        previous_alarm[0] = '\0';
                        previous_style = active_clock_style;
                    }
                    char date_line[40];
                    char time_line[24];
                    char moon_line[20];
                    format_clock_lines(dt, true, app_settings.show_seconds,
                                       date_line, sizeof(date_line),
                                       time_line, sizeof(time_line));
                    format_moon_age_line(dt, true,
                                         moon_line, sizeof(moon_line));
                    if (!app_settings.show_seconds && !colon_visible) {
                        time_line[2] = ' ';
                    }
                    draw_clock_delta(date_line, time_line,
                                     previous_date, previous_time, true);
                    draw_moon_age_delta(moon_line, previous_moon, true);
                    draw_battery_delta(latest_battery, previous_battery);
                    draw_alarm_delta(alarms, dt, last_alarm_fire, previous_alarm);
                }
                AlarmMatch alarm_match = find_alarm_match(alarms, dt);
                if (alarm_match.found && !same_alarm_minute(last_alarm_fire, dt)) {
                    calendar_peek_active = false;
                    ringing_alarm = alarm_match;
                    ringing_dt = dt;
                    alarm_started_ms = now_ms;
                    ui_mode = UiMode::AlarmRinging;
                    alarm_sound_start(now_ms);
                    draw_alarm_ringing_screen(ringing_alarm);
                    backlight_alarm_started(&backlight);
                    std::printf("ALARM fire date=%04u-%02u-%02u time=%02u:%02u alarms=A%u%s\r\n",
                                dt.year, dt.month, dt.day,
                                dt.hour, dt.minute,
                                alarm_match.first_index + 1u,
                                alarm_match.count > 1 ? "+" : "");
                }
                if (ui_mode == UiMode::Clock &&
                    app_settings.life_hourly_enabled &&
                    dt.minute == 0 &&
                    dt.second == 0 &&
                    !same_life_hour(last_life_hour, dt)) {
                    record_life_hour(&last_life_hour, dt);
                    enter_life(true);
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
                if (time_reached(now_ms, next_battery_read_ms)) {
                    latest_battery = read_battery_status(CLOCK_I2C_PORT);
                    next_battery_read_ms = now_ms + kBatteryReadIntervalMs;
                }
                uart_poll_enabled = uart_should_stay_awake();
                const uint8_t active_clock_style =
                    calendar_peek_active ? kClockStyleCalendar
                                         : app_settings.clock_style;
                if (active_clock_style == kClockStyleAnalog) {
                    const bool force_full_redraw =
                        previous_style != active_clock_style;
                    draw_analog_clock(dt, false, latest_battery, alarms, last_alarm_fire,
                                      app_settings.show_seconds,
                                      force_full_redraw,
                                      previous_date, previous_moon,
                                      previous_battery,
                                      previous_alarm, &previous_analog_hand);
                    previous_style = active_clock_style;
                } else if (active_clock_style == kClockStyleCalendar) {
                    const bool force_full_redraw =
                        previous_style != active_clock_style;
                    draw_calendar_clock(dt, false, latest_battery,
                                        alarms, last_alarm_fire,
                                        app_settings.show_seconds,
                                        force_full_redraw,
                                        previous_date, previous_time,
                                        previous_moon, previous_battery,
                                        previous_alarm);
                    previous_style = active_clock_style;
                } else {
                    if (previous_style != active_clock_style) {
                        draw_clock_frame();
                        previous_date[0] = '\0';
                        std::snprintf(previous_time, sizeof(previous_time), "        ");
                        previous_moon[0] = '\0';
                        previous_battery[0] = '\0';
                        previous_alarm[0] = '\0';
                        previous_style = active_clock_style;
                    }
                    char date_line[40];
                    char time_line[24];
                    char moon_line[20];
                    format_clock_lines(dt, false, app_settings.show_seconds,
                                       date_line, sizeof(date_line),
                                       time_line, sizeof(time_line));
                    format_moon_age_line(dt, false,
                                         moon_line, sizeof(moon_line));
                    if (!app_settings.show_seconds && !colon_visible) {
                        time_line[2] = ' ';
                    }
                    draw_clock_delta(date_line, time_line,
                                     previous_date, previous_time, false);
                    draw_moon_age_delta(moon_line, previous_moon, false);
                    draw_battery_delta(latest_battery, previous_battery);
                    picoment::display::fill_rect(kAlarmBandX, kAlarmBandY,
                                                 kAlarmBandW, kAlarmBandH, kBlack);
                    previous_alarm[0] = '\0';
                }
                next_rtc_read_ms = now_ms + kRtcFailRetryMs;
            }
        }

        if (ui_mode == UiMode::Clock &&
            !calendar_peek_active &&
            app_settings.clock_style == kClockStyleDigital &&
            !app_settings.show_seconds &&
            time_reached(now_ms, next_colon_blink_ms)) {
            if (std::strlen(previous_time) == 5) {
                colon_visible = !colon_visible;
                draw_no_seconds_colon(colon_visible, latest_rtc_ok, previous_time);
            }
            next_colon_blink_ms = now_ms + kColonBlinkMs;
        }

        if (ui_mode == UiMode::Life) {
            sleep_ms(kLifeLoopSleepMs);
        } else if (ui_mode == UiMode::AlarmRinging) {
            sleep_ms(kAlarmLoopSleepMs);
        } else if (ui_mode != UiMode::Clock) {
            sleep_ms(uart_poll_enabled ? kMainLoopActiveSleepMs : kUiSleepCapMs);
        } else {
            const bool clock_key_poll_idle =
                time_reached(now_ms,
                             last_keyboard_activity_ms +
                                 kClockIdleKeySlowAfterMs);
            const uint32_t clock_ui_sleep_cap_ms =
                clock_key_poll_idle ? kClockIdleKeySleepCapMs : kUiSleepCapMs;
            uint32_t sleep_ms_value =
                main_loop_sleep_ms(next_rtc_read_ms, uart_poll_enabled,
                                   clock_ui_sleep_cap_ms);
            if (!calendar_peek_active &&
                app_settings.clock_style == kClockStyleDigital &&
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
