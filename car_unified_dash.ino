#include "src/common/config.h"
#include "src/common/boot_log.h"
#include "src/common/telemetria.h"
#include "src/common/audio.h"
#include "src/common/estado.h"

#include "src/core/display_manager.h"
#include "src/core/touch_manager.h"
#include "src/core/mode_manager.h"
#include "src/core/uart_router.h"

#include "src/sources/mock.h"

void setup() {
  Serial.begin(115200);
  delay(1000);

  boot_log_init();
  boot_log_add(LogTipo::TITULO, "=== CAR UNIFIED DASH v1.0 ===");
  boot_log_add(LogTipo::INFO,   "ESP32-S3 240MHz 8MB PSRAM");

  telemetria_init();
  audio_init();
  estado_init();
  boot_log_add(LogTipo::OK, "Structs inicializadas");

  display_init();
  boot_log_add(LogTipo::OK, "Display ST77922 %dx%d", SCR_W, SCR_H);

  touch_init();
  boot_log_add(LogTipo::OK, "Touch inicializado");

  mode_init();
  boot_log_add(LogTipo::OK, "Mode manager pronto");

  uart_router_init();
  boot_log_add(LogTipo::OK, "UART mini OK");

#if USE_MOCK
  mock_init();
  mock_start_task();
  boot_log_add(LogTipo::WARN, "MODO MOCK ativo (dados fake)");
#else
  boot_log_add(LogTipo::OK, "Fonte: mini via serial");
#endif

  mode_start_task();
  touch_start_task();
  display_start_tasks();

  boot_log_add(LogTipo::OK, "Sistema pronto");
  Serial.println("Boot completo");
}

void loop() {
  uart_router_loop();

  // ★ Estado de sistema 1 Hz
  static uint32_t ultEstado = 0;
  if (millis() - ultEstado >= 1000) {
    ultEstado = millis();
    gEstado.uptimeMs     = millis();
    gEstado.heapLivreKB  = ESP.getFreeHeap() / 1024;
    gEstado.psramLivreKB = ESP.getFreePsram() / 1024;
  }

  vTaskDelay(pdMS_TO_TICKS(1));
}