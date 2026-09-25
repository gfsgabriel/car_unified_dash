#ifndef OBD2_MANAGER_H
#define OBD2_MANAGER_H

#include "telemetria.h"
#include <Arduino.h>

struct PIDView {
  const char* cmd;
  const char* nome;
  float ultimoValor;
  bool suportado;
  bool respondendo;
};

void inicializarOBD2();
void atualizarDadosOBD2();

void obdForcarRenegociacao();
uint8_t obdObterEstadoPIDs();
bool obdELMResponde();

PIDView obterPIDView(int idx);
int obterTotalPIDs();

void salvarAjusteMotorFlash(float novoTamanhoLitros, float novaEficienciaVE);
void obdZerarAcumulador();

float obdObterFPS();

extern volatile uint32_t obdContadorAmostras;

#endif