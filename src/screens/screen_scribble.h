#ifndef SCREENS_SCREEN_SCRIBBLE_H
#define SCREENS_SCREEN_SCRIBBLE_H

#include <Arduino.h>

void scribble_begin();
void scribble_loop(uint32_t now);
void scribble_end();
void scribble_draw();
void scribble_hover(int x, int y);
void scribble_release(int x, int y);

#endif