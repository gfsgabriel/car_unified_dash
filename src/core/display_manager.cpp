#include "display_manager.h"
#include "src/common/config.h"
#include "src/common/estado.h"

TFT_eSPI      tft_qspi    = TFT_eSPI();
ST77922       tft_st77922 = ST77922();
TFT_eSprite   canvas      = TFT_eSprite(&tft_qspi);
SemaphoreHandle_t xCanvasMutex;

int SCR_W = SCR_W_DEFAULT;
int SCR_H = SCR_H_DEFAULT;

static volatile bool requestFlush = false;

void display_init() {
  tft_st77922.Init();
  tft_st77922.Set_Rotation(1);
  SCR_W = tft_st77922.Get_Width();
  SCR_H = tft_st77922.Get_Height();

  xCanvasMutex = xSemaphoreCreateMutex();

  canvas.setColorDepth(16);
  canvas.createSprite(SCR_W, SCR_H);
  canvas.setSwapBytes(true);
  canvas.fillSprite(TFT_BLACK);

  tft_st77922.Fill_Colors(0, 0, SCR_W, SCR_H, (uint16_t*)canvas.getPointer());
}

// ===============================================================
// TaskFlush — fatias horizontais
// ===============================================================
static void TaskFlush(void*) {
  TickType_t lastWake = xTaskGetTickCount();
  const TickType_t period = pdMS_TO_TICKS(RENDER_PERIOD_MS);
  const int numFaixas = SCR_H / BAND_HEIGHT;

  for (;;) {
    if (xSemaphoreTake(xCanvasMutex, portMAX_DELAY) == pdTRUE) {
      uint16_t* base = (uint16_t*)canvas.getPointer();
      for (int i = 0; i < numFaixas; i++) {
        tft_st77922.Fill_Colors(0, i * BAND_HEIGHT, SCR_W, BAND_HEIGHT,
                                base + (i * BAND_HEIGHT * SCR_W));
      }
      xSemaphoreGive(xCanvasMutex);
    }
    vTaskDelayUntil(&lastWake, period);
  }
}

void display_start_tasks() {
  xTaskCreatePinnedToCore(TaskFlush, "flush", 4096, NULL, 1, NULL, 1);
}

void display_request_flush() {
  requestFlush = true;
}

// ===============================================================
// FPS de render
// ===============================================================
static float    g_renderFps         = 0.0f;
static uint32_t g_renderFrameCount  = 0;
static uint32_t g_renderFpsLastCalc = 0;
static uint32_t g_renderLastUs      = 0;

float    displayObterRenderFps()  { return g_renderFps; }
uint32_t displayObterRenderUs()   { return g_renderLastUs; }

void renderizarDisplay() {
  uint32_t tFrame0 = micros();
  uint32_t agora = millis();

  if (g_renderFpsLastCalc == 0) {
    g_renderFpsLastCalc = agora;
  } else if (agora - g_renderFpsLastCalc >= 1000) {
    g_renderFps = g_renderFrameCount * 1000.0f / (agora - g_renderFpsLastCalc);
    g_renderFrameCount = 0;
    g_renderFpsLastCalc = agora;

    // ★ propaga pro estado global
    gEstado.fpsRender = g_renderFps;
  }
  g_renderFrameCount++;

  g_renderLastUs = micros() - tFrame0;
}