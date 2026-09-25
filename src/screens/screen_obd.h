#ifndef SCREENS_SCREEN_OBD_H
#define SCREENS_SCREEN_OBD_H
#include <Arduino.h>

void obd_begin();
void obd_loop(uint32_t now);
void obd_end();
void obd_draw();
void obd_hover(int x, int y);
void obd_release(int x, int y);

#endif