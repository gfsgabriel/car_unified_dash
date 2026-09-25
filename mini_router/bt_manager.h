#ifndef BT_MANAGER_H
#define BT_MANAGER_H

#include <Arduino.h>

#define PIN_XM_RX  4
#define PIN_XM_TX  7
#define XM_BAUD_DEFAULT 115200

enum DonoAT {
  DONO_OBD,
  DONO_DEBUG
};

void inicializarBTManager();
void atualizarBTManager();

uint32_t btEnviarATPublico(String cmd, String terminador, uint32_t timeout, DonoAT dono);
bool btObterRespostaPublico(uint32_t id, String &resp, bool &ok, DonoAT dono);

void btIniciarDebug();
void btPararDebug();
bool btDebugAtivo();

// ★ Retorna JSON array de mensagens (formato novo)
String btObterDebug();

// Baud config
uint32_t btObterBaud();
void btSalvarBaud(uint32_t baud);
void btMudarBaud(uint32_t novoBaud, bool atualizarXM);

#endif