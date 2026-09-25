#include "src/sources/mock.h"
#include "src/common/telemetria.h"
#include "src/common/audio.h"
#include "src/common/estado.h"
#include "src/common/config.h"

static uint32_t tInicio;

void mock_init() {
  tInicio = millis();
  telemetria_init();
  audio_init();
  gTelemetria.modoExterno = true;
  gAudio.btConectado = true;
  gAudio.audioAtivo = true;
  strcpy(gAudio.titulo, "Mock Track");
  strcpy(gAudio.artista, "Mock Artist");
  gAudio.tempoTotalMs = 245000;
}

static void TaskMock(void*) {
  TickType_t lastWake = xTaskGetTickCount();
  const TickType_t period = pdMS_TO_TICKS(41);   // 24 Hz

  for (;;) {
    uint32_t agora = millis();
    float t = (agora - tInicio) / 1000.0f;

    // ---- RPM ----
    float senRPM = (sinf(t * 0.8f) + 1.0f) * 0.5f;
    gTelemetria.rpm = 800.0f + senRPM * 6200.0f;

    // ---- Velocidade ----
    float senVel = (sinf(t * 0.6f + 1.5f) + 1.0f) * 0.5f;
    gTelemetria.velocidade = senVel * 180.0f;

    // ---- TPS / pedal ----
    gTelemetria.tps   = senRPM * 100.0f;
    gTelemetria.pedal = senRPM * 95.0f + 5.0f;

    // ---- Temps ----
    gTelemetria.tempCoolant = 80.0f + sinf(t * 0.1f) * 10.0f;
    gTelemetria.tempIntake  = 25.0f + sinf(t * 0.3f) * 5.0f;
    gTelemetria.bateria     = 13.8f + sinf(t * 2.0f) * 0.3f;

    // ---- MAF ----
    gTelemetria.maf = 4.0f + senRPM * 80.0f;

    // ---- Consumo ----
    if (gTelemetria.maf > 0.0f) {
      double g_s = gTelemetria.maf / 14.7;
      double l_s = g_s / 740.0;
      gTelemetria.consumo_ml_min = l_s * 1000.0 * 60.0;
    }
    if (gTelemetria.velocidade > 5.0f) {
      gTelemetria.consumo_l_100km = (gTelemetria.consumo_ml_min * 6.0f)
                                    / gTelemetria.velocidade;
    } else {
      gTelemetria.consumo_l_100km = 99.9f;
    }

    // ---- Boost ----
    float iat = (gTelemetria.tempIntake > -30.0f) ? gTelemetria.tempIntake : 20.0f;
    if (gTelemetria.maf > 0.0f && gTelemetria.motor_litros > 0.0f) {
      double maf_kg_s = gTelemetria.maf / 1000.0;
      double temp_k = iat + 273.15;
      double rpm_lim = (gTelemetria.rpm > 600.0f) ? gTelemetria.rpm : 600.0f;
      double desc_m3 = gTelemetria.motor_litros / 1000.0;
      double num = maf_kg_s * 287.0 * temp_k;
      double den = gTelemetria.eficiencia_ve * desc_m3 * (rpm_lim / 120.0);
      if (den > 0.0) {
        double pa = (num / den) - 101325.0;
        gTelemetria.boost = pa * 0.000145038;
        if (gTelemetria.boost > gTelemetria.boostMax) {
          gTelemetria.boostMax = gTelemetria.boost;
        }
      }
    }

    gTelemetria.taxaOBD = 24.0f;
    gTelemetria.obdConectado = true;
    gTelemetria.btConectado = true;

    // ---- FFT: só os bins (a waveform é responsabilidade do render) ----
    for (int i = 0; i < FFT_BANDAS_ORIGINAIS; i++) {
      float freq = 0.4f + i * 0.25f;
      float fase = i * 0.7f;
      float s = (sinf(t * freq + fase) + 1.0f) * 0.5f;
      float ruido = (random(-30, 30)) / 255.0f;
      float v = s * 0.8f + ruido;
      if (v < 0.0f) v = 0.0f;
      if (v > 1.0f) v = 1.0f;
      gAudio.fftBins[i] = (uint8_t)(v * 255.0f);
    }
    gAudio.taxaFFT = 24.0f;
    gAudio.lastFFTMs = agora;

    // ---- Música ----
    gAudio.tempoAtualMs = (agora - tInicio) % 240000;

    // ---- Estado ----
    gEstado.uptimeMs     = agora;
    gEstado.heapLivreKB  = ESP.getFreeHeap() / 1024;
    gEstado.psramLivreKB = ESP.getFreePsram() / 1024;
    gEstado.clientesWS   = 0;
    gEstado.uartXMOK     = true;
    gEstado.uartMiniOK   = true;
    gEstado.taxaSerialXM   = 24.0f;
    gEstado.taxaSerialMini = 24.0f;

    vTaskDelayUntil(&lastWake, period);
  }
}

void mock_start_task() {
  xTaskCreatePinnedToCore(TaskMock, "mock", 4096, NULL, 2, NULL, 0);
}