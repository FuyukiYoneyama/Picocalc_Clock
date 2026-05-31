#include "app/screenshot_service.h"

#include <stdint.h>

#include "alarm_sound.h"
#include "diagnostics/screenshot_capture.h"
#include "pico/stdlib.h"
#include "platform/picocalc_audio_pwm.h"

namespace {

bool time_reached(uint32_t now_ms, uint32_t target_ms) {
    return static_cast<int32_t>(now_ms - target_ms) >= 0;
}

struct ScreenshotToneState {
    uint32_t last_progress_ms;
};

void screenshot_progress_tone(void* context) {
    auto* state = static_cast<ScreenshotToneState*>(context);
    const uint32_t now_ms = to_ms_since_boot(get_absolute_time());
    if (state == nullptr ||
        time_reached(now_ms, state->last_progress_ms + 180)) {
        picoment::audio_pwm::play_ui_tone(880, 35, 28);
        if (state != nullptr) {
            state->last_progress_ms = now_ms;
        }
    }
}

}  // namespace

bool capture_screenshot_with_sounds(bool suppress_sounds) {
    if (alarm_sound_active() || suppress_sounds) {
        return picoment::diagnostics::capture_screenshot();
    }

    alarm_sound_init();
    picoment::audio_pwm::start_stream();
    ScreenshotToneState tone_state{0};
    const bool ok = picoment::diagnostics::capture_screenshot(
        screenshot_progress_tone, &tone_state);
    picoment::audio_pwm::play_ui_tone(ok ? 988 : 392, 70, 36);
    sleep_ms(80);
    picoment::audio_pwm::play_ui_tone(ok ? 1319 : 294, 90, 36);
    sleep_ms(100);
    alarm_sound_shutdown();
    return ok;
}
