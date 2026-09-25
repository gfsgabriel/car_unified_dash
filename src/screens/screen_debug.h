#ifndef SCREENS_SCREEN_DEBUG_H
#define SCREENS_SCREEN_DEBUG_H

#include <Arduino.h>

void debug_begin();
void debug_loop(uint32_t now);
void debug_end();
void debug_draw();
void debug_hover(int x, int y);
void debug_release(int x, int y);

#endif