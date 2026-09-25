#ifndef TELEMETRIA_H
#define TELEMETRIA_H

#include <Arduino.h>

struct TelemetriaVeiculo {
  float rpm;
  float velocidade;
  float maf;
  float boost;
  float boost_max;
  float tps;
  float tempCoolant;
  float tempIntake;
  float bateria;
  float pedal;

  float consumo_ml_min;
  float consumo_l_100km;
  float consumo_total_litros;

  float zeroCemUltimo;

  float motor_litros;
  float eficiencia_ve;

  bool modoExterno;

  bool btConectado;
  bool obdConectado;
};

extern TelemetriaVeiculo telemetria;

#endif