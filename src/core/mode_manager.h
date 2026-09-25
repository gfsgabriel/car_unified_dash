#ifndef CORE_MODE_MANAGER_H
#define CORE_MODE_MANAGER_H

#include "src/screens/screen_base.h"

enum ScreenId : uint8_t {
  SCREEN_LOADING,
  SCREEN_HOME,
  SCREEN_MENU,
  SCREEN_LOGS,
  SCREEN_SCRIBBLE,
  SCREEN_DEBUG,
  SCREEN_OBD,
  SCREEN_WIFI,
  SCREEN_COUNT
};

void     mode_init();
void     mode_set(ScreenId id);
ScreenId mode_get();
void     mode_start_task();

#endif