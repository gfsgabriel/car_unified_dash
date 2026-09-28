#ifndef SCREENS_SCREEN_SETTINGS_H
#define SCREENS_SCREEN_SETTINGS_H
#include <Arduino.h>

void settings_begin();
void settings_loop(uint32_t now);
void settings_end();
void settings_draw();
void settings_hover(int x, int y);
void settings_release(int x, int y);

#endif