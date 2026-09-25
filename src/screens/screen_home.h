#ifndef SCREENS_SCREEN_HOME_H
#define SCREENS_SCREEN_HOME_H

#include <Arduino.h>

void home_begin();
void home_loop(uint32_t now);
void home_end();
void home_draw();
void home_hover(int x, int y);
void home_release(int x, int y);

#endif