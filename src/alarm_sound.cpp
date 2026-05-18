#include "alarm_sound.h"

#include <stdint.h>
#include <cstdio>

#include "config/board_config.h"
#include "platform/picocalc_audio_pwm.h"

namespace {

constexpr uint32_t kAlarmToneHz = 880;
constexpr uint32_t kAlarmOnMs = 200;
constexpr uint32_t kAlarmOffMs = 200;
constexpr uint32_t kAlarmPatternMs = kAlarmOnMs + kAlarmOffMs;
constexpr uint8_t kAlarmAmplitude = 48;
constexpr uint32_t kMaxSamplesPerService = 512;

bool g_initialized = false;
bool g_active = false;
uint32_t g_started_ms = 0;
uint32_t g_phase = 0;
uint32_t g_phase_step = 0;
uint32_t g_last_underrun_count = 0;

uint32_t phase_step_for(uint32_t frequency_hz) {
    return static_cast<uint32_t>(
        (static_cast<uint64_t>(frequency_hz) << 32) /
        picoment::board::kTargetSampleRate);
}

int16_t next_square_sample(bool tone_on) {
    if (!tone_on) {
        return 0;
    }
    g_phase += g_phase_step;
    const int16_t sample = (g_phase & 0x80000000u)
                               ? static_cast<int16_t>(-kAlarmAmplitude * 256)
                               : static_cast<int16_t>(kAlarmAmplitude * 256);
    return sample;
}

}  // namespace

void alarm_sound_init() {
    if (g_initialized) {
        return;
    }
    g_phase_step = phase_step_for(kAlarmToneHz);
    picoment::audio_pwm::init_stream();
    picoment::audio_pwm::start_stream();
    g_initialized = true;
    g_active = false;
    g_last_underrun_count = picoment::audio_pwm::stats().underrun_count;
}

void alarm_sound_start(uint32_t now_ms) {
    alarm_sound_init();
    g_started_ms = now_ms;
    g_phase = 0;
    g_active = true;
}

void alarm_sound_stop() {
    g_active = false;
}

void alarm_sound_service(uint32_t now_ms) {
    if (!g_initialized) {
        return;
    }

    const uint32_t writable = picoment::audio_pwm::writable_samples();
    const uint32_t count =
        writable > kMaxSamplesPerService ? kMaxSamplesPerService : writable;
    const uint32_t elapsed_ms = now_ms - g_started_ms;
    const bool tone_on = g_active && ((elapsed_ms % kAlarmPatternMs) < kAlarmOnMs);

    for (uint32_t i = 0; i < count; ++i) {
        const int16_t sample = next_square_sample(tone_on);
        (void)picoment::audio_pwm::write_sample(sample, sample);
    }

#if !defined(PICOCLOCK_BUILD_RELEASE)
    const picoment::audio_pwm::Stats stats = picoment::audio_pwm::stats();
    if (stats.underrun_count != g_last_underrun_count) {
        std::printf("ALARM audio underrun count=%lu\r\n",
                    static_cast<unsigned long>(stats.underrun_count));
        g_last_underrun_count = stats.underrun_count;
    }
#else
    (void)now_ms;
#endif
}

bool alarm_sound_active() {
    return g_active;
}
