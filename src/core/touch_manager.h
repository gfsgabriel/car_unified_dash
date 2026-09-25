#ifndef CORE_TOUCH_MANAGER_H
#define CORE_TOUCH_MANAGER_H

#include "src/common/tipos.h"
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

extern QueueHandle_t xFilaTouch;

void touch_init();
void touch_start_task();

#endif