#ifndef COMMON_ESTADO_H
#define COMMON_ESTADO_H

#include <Arduino.h>

struct EstadoSistema {
  uint32_t uptimeMs;
  uint16_t heapLivreKB;
  uint16_t psramLivreKB;
  uint8_t  clientesWS;
  bool     uartXMOK;
  bool     uartMiniOK;

  float    fpsRender;
  uint32_t tempoDrawMs;
  uint32_t tempoFlushMs;
  float    taxaSerialXM;
  float    taxaSerialMini;
};

extern EstadoSistema gEstado;

void estado_init();

#endif