#include "mode_manager.h"
#include "display_manager.h"
#include "touch_manager.h"
#include "src/common/config.h"

#include "src/screens/screen_loading.h"
#include "src/screens/screen_home.h"
#include "src/screens/screen_menu.h"
#include "src/screens/screen_logs.h"
#include "src/screens/screen_scribble.h"
#include "src/screens/screen_debug.h"
#include "src/screens/screen_obd.h"
#include "src/screens/screen_wifi.h"

#define TOUCH_GRACE_MS 250

static const Screen screens[SCREEN_COUNT] = {
  { "loading",  loading_begin,  loading_loop,  loading_end,  loading_draw,  loading_hover,  loading_release  },
  { "home",     home_begin,     home_loop,     home_end,     home_draw,     home_hover,     home_release     },
  { "menu",     menu_begin,     menu_loop,     menu_end,     menu_draw,     menu_hover,     menu_release     },
  { "logs",     logs_begin,     logs_loop,     logs_end,     logs_draw,     logs_hover,     logs_release     },
  { "scribble", scribble_begin, scribble_loop, scribble_end, scribble_draw, scribble_hover, scribble_release },
  { "debug",    debug_begin,    debug_loop,    debug_end,    debug_draw,    debug_hover,    debug_release    },
  { "obd",      obd_begin,      obd_loop,      obd_end,      obd_draw,      obd_hover,      obd_release      },
  { "wifi",     wifi_begin,     wifi_loop,     wifi_end,     wifi_draw,     wifi_hover,     wifi_release     },
};

static ScreenId atual = SCREEN_LOADING;
static bool     trocou = false;

void mode_init() {
  atual = SCREEN_LOADING;
  trocou = true;
}

void mode_set(ScreenId id) {
  if (id == atual || id >= SCREEN_COUNT) return;
  if (screens[atual].end) screens[atual].end();
  atual = id;
  trocou = true;
}

ScreenId mode_get() { return atual; }

static void TaskMode(void*) {
  TouchPoint tp;
  uint32_t   tUltimaTroca = 0;

  for (;;) {
    if (trocou) {
      trocou = false;

      // ★ Limpa o canvas SÓ na troca de tela (evita resíduo entre telas,
      //    mas não apaga o que a tela desenha por conta própria)
      if (xSemaphoreTake(xCanvasMutex, portMAX_DELAY) == pdTRUE) {
        canvas.fillSprite(TFT_BLACK);
        xSemaphoreGive(xCanvasMutex);
      }

      if (screens[atual].begin) screens[atual].begin();

      TouchPoint lixo;
      while (xQueueReceive(xFilaTouch, &lixo, 0) == pdTRUE) {}

      tUltimaTroca = millis();
    }

    bool aceitaTouch = (millis() - tUltimaTroca) >= TOUCH_GRACE_MS;

    while (xQueueReceive(xFilaTouch, &tp, 0) == pdTRUE) {
      if (!aceitaTouch) continue;
      if (tp.pressed) {
        if (screens[atual].onTouchHover) screens[atual].onTouchHover(tp.x, tp.y);
      } else {
        if (screens[atual].onTouchRelease) screens[atual].onTouchRelease(tp.x, tp.y);
      }
    }

    if (screens[atual].loop) screens[atual].loop(millis());

    if (xSemaphoreTake(xCanvasMutex, portMAX_DELAY) == pdTRUE) {
      canvas.setTextFont(1);
      canvas.setTextSize(1);
      canvas.setTextDatum(TL_DATUM);
      canvas.setTextColor(TFT_WHITE);

      if (screens[atual].draw) screens[atual].draw();
      xSemaphoreGive(xCanvasMutex);
    }

    vTaskDelay(pdMS_TO_TICKS(RENDER_PERIOD_MS));
  }
}

void mode_start_task() {
  xTaskCreatePinnedToCore(TaskMode, "mode", 6144, NULL, 2, NULL, 1);
}