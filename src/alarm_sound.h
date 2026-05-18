#ifndef PICOCALC_CLOCK_ALARM_SOUND_H
#define PICOCALC_CLOCK_ALARM_SOUND_H

#include <stdint.h>

void alarm_sound_init();
void alarm_sound_start(uint32_t now_ms);
void alarm_sound_stop();
void alarm_sound_service(uint32_t now_ms);
bool alarm_sound_active();

#endif
