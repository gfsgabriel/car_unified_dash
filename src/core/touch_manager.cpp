#include "touch_manager.h"
#include "src/common/config.h"
#include "ST77922_Touch.h"

ST77922_TOUCH touch_st77922;
QueueHandle_t xFilaTouch;

void touch_init() {
  touch_st77922.init();
  touch_st77922.Set_Rotation(1);
  xFilaTouch = xQueueCreate(TOUCH_QUEUE_LEN, sizeof(TouchPoint));
}

static void TaskTouch(void*) {
  bool     estavel   = false;
  uint32_t ultimoTrue = 0;
  uint16_t ultX = 0, ultY = 0;

  for (;;) {
    bool agora = touch_st77922.Get_Touch();
    uint16_t x = touch_st77922.touch.x[0];
    uint16_t y = touch_st77922.touch.y[0];

    // DOWN (imediato)
    if (agora && !estavel) {
      estavel = true;
      ultimoTrue = millis();
      ultX = x; ultY = y;
      TouchPoint tp = { x, y, true };
      xQueueSend(xFilaTouch, &tp, 0);
    }

    // MOVIMENTO
    if (agora) {
      ultimoTrue = millis();
      if (estavel && (x != ultX || y != ultY)) {
        TouchPoint tp = { x, y, true };
        xQueueSend(xFilaTouch, &tp, 0);
        ultX = x; ultY = y;
      }
    }

    // UP (com debounce — agora 50ms)
    if (estavel && !agora) {
      if ((millis() - ultimoTrue) >= DEBOUNCE_RELEASE_MS) {
        estavel = false;
        TouchPoint tp = { ultX, ultY, false };
        xQueueSend(xFilaTouch, &tp, 0);
      }
    }

    vTaskDelay(pdMS_TO_TICKS(1));
  }
}

void touch_start_task() {
  xTaskCreatePinnedToCore(TaskTouch, "touch", 3072, NULL, 3, NULL, 0);
}