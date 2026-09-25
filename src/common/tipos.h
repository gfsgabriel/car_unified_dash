#ifndef COMMON_TIPOS_H
#define COMMON_TIPOS_H

#include <Arduino.h>

struct TouchPoint {
  uint16_t x;
  uint16_t y;
  bool     pressed;
};

enum class LogTipo : uint8_t {
  INFO,
  OK,
  WARN,
  ERR,
  TITULO
};

#endif