#include "src/common/telemetria.h"

TelemetriaVeiculo gTelemetria;

void telemetria_init() {
  memset(&gTelemetria, 0, sizeof(gTelemetria));
  gTelemetria.motor_litros  = 2.5f;
  gTelemetria.eficiencia_ve = 0.85f;
  gTelemetria.boost         = -15.0f;
  gTelemetria.bateria       = 13.8f;
}