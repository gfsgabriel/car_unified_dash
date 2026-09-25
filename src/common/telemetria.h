#ifndef COMMON_TELEMETRIA_H
#define COMMON_TELEMETRIA_H

#include <Arduino.h>

struct TelemetriaVeiculo {
  // === Bruto do OBD2 ===
  float rpm;
  float velocidade;
  float maf;
  float tps;
  float pedal;
  float tempCoolant;
  float tempIntake;
  float bateria;

  // === Calculados ===
  float boost;
  float boostMax;
  float consumo_ml_min;
  float consumo_l_100km;
  float consumo_total_litros;
  float zeroCemUltimo;

  // === Config motor (NVS) ===
  float motor_litros;
  float eficiencia_ve;

  // === Estado ===
  bool     modoExterno;
  bool     btConectado;
  bool     obdConectado;
  uint32_t lastUpdateMs;

  // === Estatísticas ===
  float    taxaOBD;          // amostras/s
};

extern TelemetriaVeiculo gTelemetria;

void telemetria_init();

#endif