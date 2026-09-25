#ifndef SCREENS_SCREEN_WIFI_H
#define SCREENS_SCREEN_WIFI_H
#include <Arduino.h>

void wifi_begin();
void wifi_loop(uint32_t now);
void wifi_end();
void wifi_draw();
void wifi_hover(int x, int y);
void wifi_release(int x, int y);

#endif