#ifndef PICOCALC_CLOCK_UI_H
#define PICOCALC_CLOCK_UI_H

#include <stdint.h>

bool ui_init();
bool ui_keyboard_probe();
void ui_show_lcd_test(const char *version);

#endif
