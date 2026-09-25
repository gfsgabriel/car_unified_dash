#ifndef CORE_DISPLAY_MANAGER_H
#define CORE_DISPLAY_MANAGER_H

#include <TFT_eSPI.h>
#include <ST77922.h>
#include "src/common/tipos.h"
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

extern TFT_eSPI     tft_qspi;      // ← ADICIONA
extern TFT_eSprite  canvas;
extern int SCR_W;
extern int SCR_H;
extern SemaphoreHandle_t xCanvasMutex;

void display_init();
void display_start_tasks();
void display_request_flush();

#endif