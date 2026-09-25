#ifndef SCREENS_SCREEN_BASE_H
#define SCREENS_SCREEN_BASE_H

#include <Arduino.h>

struct Screen {
  const char* nome;
  void (*begin)();
  void (*loop)(uint32_t now);
  void (*end)();
  void (*draw)();
  void (*onTouchHover)(int x, int y);
  void (*onTouchRelease)(int x, int y);
};

#endif