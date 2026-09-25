#ifndef SCREENS_SCREEN_LOADING_H
#define SCREENS_SCREEN_LOADING_H

#include <Arduino.h>

void loading_begin();
void loading_loop(uint32_t now);
void loading_end();
void loading_draw();
void loading_hover(int x, int y);
void loading_release(int x, int y);

#endif