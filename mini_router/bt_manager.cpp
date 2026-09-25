#include "bt_manager.h"
#include <Preferences.h>
#include <ArduinoJson.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include <freertos/semphr.h>

static Preferences prefsBT;
static uint32_t xmBaud = XM_BAUD_DEFAULT;

#define MOCK_USB 0

#if MOCK_USB
  #define XM_SERIAL Serial
#else
  static HardwareSerial serialXM(0);
  #define XM_SERIAL serialXM
#endif

static SemaphoreHandle_t xmSerialMutex = NULL;

static inline bool xmLock(uint32_t ms = portMAX_DELAY) {
  if (!xmSerialMutex) return false;
  return (xSemaphoreTake(xmSerialMutex, pdMS_TO_TICKS(ms)) == pdTRUE);
}
static inline void xmUnlock() {
  if (xmSerialMutex) xSemaphoreGive(xmSerialMutex);
}

struct ComandoAT {
  char cmd[96];
  char terminador[32];
  uint32_t timeout;
  uint32_t id;
  DonoAT dono;
};

struct RespostaAT {
  uint32_t id;
  char resp[512];
  size_t len;
  bool ok;
};

static QueueHandle_t xFilaComandosAT  = NULL;
static QueueHandle_t xFilaRespOBD     = NULL;
static QueueHandle_t xFilaRespDebug   = NULL;
static uint32_t proximoID = 1;

// ---------------------------------------------------------------
// Debug — ring buffer com campos separados (vai virar JSON)
// ---------------------------------------------------------------
#define BT_DEBUG_MAX      50
#define BT_DEBUG_CMD_LEN  48
#define BT_DEBUG_RESP_LEN 220
#define BT_DEBUG_JSON_MAX 30    // últimas N msgs enviadas ao browser

struct MsgDebug {
  char tipo;                            // 'T', 'R', 'S'
  char dono[8];                         // "OBD" ou "DEBUG"
  char cmd[BT_DEBUG_CMD_LEN];
  char resp[BT_DEBUG_RESP_LEN];
  uint32_t ms;
  uint32_t dur;
  bool ok;
};

static MsgDebug debugCircular[BT_DEBUG_MAX];
static int debugIdx = 0;
static int debugCount = 0;
static SemaphoreHandle_t debugMutex = NULL;
static volatile bool _debugAtivo = false;

static const char* nomeDono(DonoAT d) {
  switch (d) {
    case DONO_OBD:   return "OBD";
    case DONO_DEBUG: return "DEBUG";
  }
  return "?";
}

void btIniciarDebug() {
  if (debugMutex == NULL) debugMutex = xSemaphoreCreateMutex();
  _debugAtivo = true;
  debugIdx = 0;
  debugCount = 0;
  Serial.println("BT: debug ATIVADO");
}

void btPararDebug() {
  _debugAtivo = false;
  debugIdx = 0;
  debugCount = 0;
  Serial.println("BT: debug DESATIVADO");
}

bool btDebugAtivo() { return _debugAtivo; }

static void postarDebugTX(DonoAT dono, const char* cmd) {
  if (!_debugAtivo || debugMutex == NULL) return;
  if (xSemaphoreTake(debugMutex, 0) != pdTRUE) return;

  MsgDebug &m = debugCircular[debugIdx];
  m.tipo = 'T';
  strncpy(m.dono, nomeDono(dono), sizeof(m.dono) - 1);
  m.dono[sizeof(m.dono) - 1] = '\0';
  strncpy(m.cmd, cmd, sizeof(m.cmd) - 1);
  m.cmd[sizeof(m.cmd) - 1] = '\0';
  m.resp[0] = '\0';
  m.ms = millis();
  m.dur = 0;
  m.ok = true;

  debugIdx = (debugIdx + 1) % BT_DEBUG_MAX;
  if (debugCount < BT_DEBUG_MAX) debugCount++;

  xSemaphoreGive(debugMutex);
}

static void postarDebugRX(DonoAT dono, const char* cmd, const char* resp,
                          uint32_t dur, bool ok) {
  if (!_debugAtivo || debugMutex == NULL) return;
  if (xSemaphoreTake(debugMutex, 0) != pdTRUE) return;

  MsgDebug &m = debugCircular[debugIdx];
  m.tipo = 'R';
  strncpy(m.dono, nomeDono(dono), sizeof(m.dono) - 1);
  m.dono[sizeof(m.dono) - 1] = '\0';
  strncpy(m.cmd, cmd, sizeof(m.cmd) - 1);
  m.cmd[sizeof(m.cmd) - 1] = '\0';
  strncpy(m.resp, resp, sizeof(m.resp) - 1);
  m.resp[sizeof(m.resp) - 1] = '\0';
  m.ms = millis();
  m.dur = dur;
  m.ok = ok;

  debugIdx = (debugIdx + 1) % BT_DEBUG_MAX;
  if (debugCount < BT_DEBUG_MAX) debugCount++;

  xSemaphoreGive(debugMutex);
}

static void postarDebugSys(const char* texto) {
  if (!_debugAtivo || debugMutex == NULL) return;
  if (xSemaphoreTake(debugMutex, 0) != pdTRUE) return;

  MsgDebug &m = debugCircular[debugIdx];
  m.tipo = 'S';
  strncpy(m.dono, "SYS", sizeof(m.dono) - 1);
  m.dono[sizeof(m.dono) - 1] = '\0';
  strncpy(m.cmd, texto, sizeof(m.cmd) - 1);
  m.cmd[sizeof(m.cmd) - 1] = '\0';
  m.resp[0] = '\0';
  m.ms = millis();
  m.dur = 0;
  m.ok = true;

  debugIdx = (debugIdx + 1) % BT_DEBUG_MAX;
  if (debugCount < BT_DEBUG_MAX) debugCount++;

  xSemaphoreGive(debugMutex);
}

// ★ Retorna JSON array de mensagens
String btObterDebug() {
  if (debugMutex == NULL) return "[]";
  if (xSemaphoreTake(debugMutex, portMAX_DELAY) != pdTRUE) return "[]";

  DynamicJsonDocument doc(20480);
  JsonArray arr = doc.to<JsonArray>();

  int totalToSend = (debugCount < BT_DEBUG_JSON_MAX) ? debugCount : BT_DEBUG_JSON_MAX;

  int inicio;
  if (debugCount < BT_DEBUG_MAX) {
    inicio = debugCount - totalToSend;
    if (inicio < 0) inicio = 0;
  } else {
    inicio = (debugIdx + (debugCount - totalToSend)) % BT_DEBUG_MAX;
  }

  for (int i = 0; i < totalToSend; i++) {
    int idx = (inicio + i) % BT_DEBUG_MAX;
    MsgDebug &m = debugCircular[idx];

    JsonObject o = arr.createNestedObject();
    o["t"] = String((char)m.tipo);
    o["d"] = String(m.dono);
    o["c"] = String(m.cmd);
    o["ms"] = m.ms;

    if (m.tipo == 'R') {
      o["r"] = String(m.resp);
      o["dur"] = m.dur;
      o["ok"] = m.ok;
    }
  }

  String out;
  serializeJson(doc, out);

  // Drenou — zera
  debugIdx = 0;
  debugCount = 0;

  xSemaphoreGive(debugMutex);
  return out;
}

// ---------------------------------------------------------------
// Task do XM (router burro)
// ---------------------------------------------------------------
void vTaskXM(void *pvParameters) {
  ComandoAT cmdAtual;
  char respBuffer[512];
  size_t respLen = 0;

  Serial.println("BT[router]: iniciada");

  for (;;) {
    if (xQueueReceive(xFilaComandosAT, &cmdAtual, 0) == pdTRUE) {

      if (!xmLock(2000)) {
        Serial.println("BT: xmLock timeout, pulando cmd");
        continue;
      }

      while (XM_SERIAL.available()) XM_SERIAL.read();

      postarDebugTX(cmdAtual.dono, cmdAtual.cmd);

#if MOCK_USB
      Serial.printf("$XM:%s\r\n", cmdAtual.cmd);
#else
      XM_SERIAL.print(cmdAtual.cmd);
      XM_SERIAL.print("\r\n");
#endif

      respLen = 0;
      respBuffer[0] = '\0';
      unsigned long inicio = millis();
      bool viuTerm = false;
      bool viuErro = false;

      while (millis() - inicio < cmdAtual.timeout) {
        while (XM_SERIAL.available() && respLen < 510) {
          respBuffer[respLen++] = XM_SERIAL.read();
          respBuffer[respLen] = '\0';
        }

        String tmp(respBuffer);
        if (strlen(cmdAtual.terminador) > 0) {
          viuTerm = (tmp.indexOf(cmdAtual.terminador) >= 0);
        }
        viuErro = (tmp.indexOf("ERROR") >= 0);

        if (viuTerm || viuErro) {
          //Serial.printf("end_terminator");
          break;
        }
        vTaskDelay(pdMS_TO_TICKS(5));
      }

      //Serial.printf("vTaskXM: saiu do while — dur=%lums term=%d err=%d resp=[%.80s]\n",millis() - inicio, (int)viuTerm, (int)viuErro, respBuffer);

      unsigned long duracao = millis() - inicio;
      bool ok = viuTerm;

      xmUnlock();

      postarDebugRX(cmdAtual.dono, cmdAtual.cmd, respBuffer, duracao, ok);

      RespostaAT r;
      r.id = cmdAtual.id;
      memcpy(r.resp, respBuffer, respLen);
      r.resp[respLen] = '\0';
      r.len = respLen;
      r.ok = ok;

      switch (cmdAtual.dono) {
        case DONO_OBD:   xQueueSend(xFilaRespOBD, &r, 0);   break;
        case DONO_DEBUG: xQueueSend(xFilaRespDebug, &r, 0); break;
      }
    }

    vTaskDelay(pdMS_TO_TICKS(5));
  }
}

static uint32_t btEnviarATInterno(String cmd, String terminador, uint32_t timeout, DonoAT dono) {
  if (xFilaComandosAT == NULL) return 0;

  ComandoAT c;
  strncpy(c.cmd, cmd.c_str(), sizeof(c.cmd) - 1);
  c.cmd[sizeof(c.cmd) - 1] = '\0';
  strncpy(c.terminador, terminador.c_str(), sizeof(c.terminador) - 1);
  c.terminador[sizeof(c.terminador) - 1] = '\0';
  c.timeout = timeout;
  c.id = proximoID++;
  c.dono = dono;

  if (xQueueSend(xFilaComandosAT, &c, 0) != pdTRUE) {
    Serial.println("BT: fila de comandos cheia!");
    return 0;
  }
  return c.id;
}

uint32_t btEnviarATPublico(String cmd, String terminador, uint32_t timeout, DonoAT dono) {
  return btEnviarATInterno(cmd, terminador, timeout, dono);
}

static bool btObterRespostaInterno(uint32_t id, String &resp, bool &ok, DonoAT dono) {
  QueueHandle_t fila = (dono == DONO_OBD) ? xFilaRespOBD : xFilaRespDebug;
  if (fila == NULL) return false;

  RespostaAT r;
  if (xQueueReceive(fila, &r, 0) != pdTRUE) return false;

  if (r.id == id) {
    resp = String(r.resp);
    ok = r.ok;
    return true;
  }
  return false;
}

bool btObterRespostaPublico(uint32_t id, String &resp, bool &ok, DonoAT dono) {
  return btObterRespostaInterno(id, resp, ok, dono);
}

// ---------------------------------------------------------------
// Baud config
// ---------------------------------------------------------------
uint32_t btObterBaud() { return xmBaud; }

void btSalvarBaud(uint32_t baud) {
  prefsBT.begin("bt_store", false);
  prefsBT.putUInt("baud", baud);
  prefsBT.end();
  Serial.printf("BT: baud %u salvo na NVS\n", baud);
}

void btMudarBaud(uint32_t novoBaud, bool atualizarXM) {
  Serial.printf("BT: mudar baud para %u (atualizarXM=%d)\n",
                novoBaud, (int)atualizarXM);

  if (atualizarXM) {
    if (xmLock(2000)) {
      while (XM_SERIAL.available()) XM_SERIAL.read();

      char cmd[64];
      snprintf(cmd, sizeof(cmd), "AT+UART=%u,0,0", novoBaud);
      Serial.printf("BT: enviando [%s]\n", cmd);

      XM_SERIAL.print(cmd);
      XM_SERIAL.print("\r\n");

      unsigned long inicio = millis();
      String resp = "";
      while (millis() - inicio < 500) {
        while (XM_SERIAL.available()) {
          resp += (char)XM_SERIAL.read();
        }
        if (resp.indexOf("OK") >= 0) break;
        vTaskDelay(pdMS_TO_TICKS(5));
      }
      Serial.printf("BT: resp [%s]\n", resp.c_str());
      xmUnlock();
    }
    delay(500);
  }

  btSalvarBaud(novoBaud);
  delay(300);
  Serial.println("BT: reiniciando...");
  ESP.restart();
}

// ---------------------------------------------------------------
// Init
// ---------------------------------------------------------------
void inicializarBTManager() {
  Serial.println("BT: inicializando...");

  prefsBT.begin("bt_store", false);   // RW
  if (!prefsBT.isKey("baud")) {
    Serial.printf("BT: NVS vazia — salvando baud default = %u\n", XM_BAUD_DEFAULT);
    prefsBT.putUInt("baud", XM_BAUD_DEFAULT);
    xmBaud = XM_BAUD_DEFAULT;
  } else {
    xmBaud = prefsBT.getUInt("baud", XM_BAUD_DEFAULT);
    Serial.printf("BT: baud carregado da NVS = %u\n", xmBaud);
  }
  prefsBT.end();
  
  if (xmBaud != 9600 && xmBaud != 19200 && xmBaud != 38400 &&
      xmBaud != 57600 && xmBaud != 115200) {
    xmBaud = XM_BAUD_DEFAULT;
    Serial.printf("BT: baud invalido na NVS, usando %u\n", xmBaud);
  }

  xmSerialMutex = xSemaphoreCreateMutex();
  debugMutex    = xSemaphoreCreateMutex();

  xFilaComandosAT = xQueueCreate(10, sizeof(ComandoAT));
  xFilaRespOBD    = xQueueCreate(10, sizeof(RespostaAT));
  xFilaRespDebug  = xQueueCreate(10, sizeof(RespostaAT));

#if MOCK_USB
  Serial.println("BT: MOCK_USB — usando Serial (USB)");
#else
  Serial.printf("BT: modo XM — RX=%d TX=%d baud=%u\n",
                PIN_XM_RX, PIN_XM_TX, xmBaud);
  serialXM.begin(xmBaud, SERIAL_8N1, PIN_XM_RX, PIN_XM_TX);
  delay(500);
#endif

  xTaskCreatePinnedToCore(vTaskXM, "TaskXM", 4096, NULL, 1, NULL, 0);
  Serial.println("BT: task iniciada");
}

void atualizarBTManager() {
  // vTaskXM cuida de tudo
}