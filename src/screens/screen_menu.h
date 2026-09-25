#ifndef SCREENS_SCREEN_MENU_H
#define SCREENS_SCREEN_MENU_H

#include <Arduino.h>

void menu_begin();
void menu_loop(uint32_t now);
void menu_end();
void menu_draw();
void menu_hover(int x, int y);
void menu_release(int x, int y);

#endif