#include "src/common/estado.h"

EstadoSistema gEstado;

void estado_init() {
  memset(&gEstado, 0, sizeof(gEstado));
  gEstado.uartXMOK = true;
  gEstado.uartMiniOK = true;
}