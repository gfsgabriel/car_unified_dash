#ifndef SCREENS_SCREEN_LOGS_H
#define SCREENS_SCREEN_LOGS_H

#include <Arduino.h>

void logs_begin();
void logs_loop(uint32_t now);
void logs_end();
void logs_draw();
void logs_hover(int x, int y);
void logs_release(int x, int y);

#endif