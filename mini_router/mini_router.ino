#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <Preferences.h>
#include <ArduinoJson.h>

#include "index.h"
#include "bt.h"
#include "admin.h"
#include "bt_manager.h"
#include "obd2_manager.h"
#include "telemetria.h"

// ===============================================================
// PINAGEM
// ===============================================================
#define RX_WROOM  5
#define TX_WROOM  6

#define RX_FREE    2
#define TX_FREE    1

#define RX_XM      4
#define TX_XM      7

#define LED_PIN   21

// 5 botões físicos: PREV, PLAY, NEXT, VOL-, VOL+
const uint8_t PINOS_BTN[5] = { 8, 9, 10, 11, 12 };
const int NUM_BOTOES = 5;

// ===============================================================
// PROTOCOLO
// ===============================================================
#define HEADER_0 0xAA
#define HEADER_1 0x55

#define T_FFT          0x01
#define T_MUSIC        0x02
#define T_MEDIA_CMD    0x03
#define T_TELEMETRIA   0x04
#define T_OBD_PIDS     0x05

#define F_WIFI_QUERY    0x30
#define F_WIFI_CONN_REQ 0x32
#define F_AT_CMD        0x34
#define F_OBD_REOB      0x35

#define F_WIFI_STATUS   0x10
#define F_AT_RESP       0x13
#define F_BTN_EVENT     0x14

#define WF_IDLE        0
#define WF_CONECTANDO  1
#define WF_CONECTADO   2
#define WF_FALHA       3

#define WIFI_TIMEOUT_CONEXAO_MS 10000

#define PKT_MAX_PAYLOAD 256

struct __attribute__((packed)) PacoteSerial {
  uint8_t tipo;
  uint8_t len;
  uint8_t dados[PKT_MAX_PAYLOAD];
};

// ===============================================================
// OBJETOS
// ===============================================================
AsyncWebServer   server(80);
AsyncWebSocket   ws("/ws");
Preferences      preferences;

HardwareSerial   SerialFree(2);

TelemetriaVeiculo telemetria;

// ===============================================================
// FILAS
// ===============================================================
QueueHandle_t filaWroomRx;
QueueHandle_t filaFreenoveRx;
QueueHandle_t filaFreenoveTx;
QueueHandle_t filaBotoesQueue;

// ===============================================================
// REDES SALVAS — só a última que conectou
// ===============================================================
String wifiSavedSSID = "";
String wifiSavedPass = "";

// ===============================================================
// BOTÕES
// ===============================================================
struct BotaoISR {
  uint8_t pino;
  volatile bool estadoEstavel;
  volatile unsigned long ultimaMudanca;
};
volatile BotaoISR botoes[NUM_BOTOES];
const unsigned long DEBOUNCE_MS = 10;

void IRAM_ATTR isrBotao(void* arg) {
  int id = (int)arg;
  unsigned long agora = millis();
  bool leitura = digitalRead(botoes[id].pino);
  if ((agora - botoes[id].ultimaMudanca) >= DEBOUNCE_MS) {
    if (leitura != botoes[id].estadoEstavel) {
      botoes[id].estadoEstavel = leitura;
      botoes[id].ultimaMudanca = agora;
      if (botoes[id].estadoEstavel == LOW) {
        BaseType_t woken = pdFALSE;
        xQueueSendFromISR(filaBotoesQueue, &id, &woken);
      }
    }
  } else {
    botoes[id].ultimaMudanca = agora;
  }
}

// ===============================================================
// ESTADO
// ===============================================================
uint8_t  fft32[32] = {0};
String   musicaTitulo  = "Sem Faixa";
String   musicaArtista = "Desconhecido";
uint32_t tempoAtualMs  = 0;
uint32_t tempoTotalMs  = 0;
bool     btConectado   = false;
bool     audioAtivo    = false;

// ★ Volume local (verdade do mini), 0-100
uint8_t  volumeMini = 100;
const uint8_t VOLUME_STEP = 5;

uint8_t  wifiEstado   = WF_IDLE;
String   wifiAlvoSSID = "";
String   wifiAlvoPass = "";
unsigned long tInicioConexao = 0;

uint32_t tUltimoWifiStatus = 0;
#define WIFI_STATUS_PERIOD 1000

// ===============================================================
// ENVIO — WROOM
// ===============================================================
void enviarParaWroom(uint8_t tipo, const uint8_t* payload, uint8_t len) {
  uint8_t pkt[4 + PKT_MAX_PAYLOAD];
  pkt[0] = HEADER_0;
  pkt[1] = HEADER_1;
  pkt[2] = tipo;
  pkt[3] = len;
  if (len > 0 && payload) memcpy(&pkt[4], payload, len);
  Serial1.write(pkt, 4 + len);
}

// ===============================================================
// ENVIO — FREENOVE
// ===============================================================
void enfileirarPkt(QueueHandle_t q, uint8_t tipo,
                   const uint8_t* payload, uint8_t len) {
  if (len > PKT_MAX_PAYLOAD) return;
  PacoteSerial p;
  p.tipo = tipo;
  p.len  = len;
  if (len > 0 && payload) memcpy(p.dados, payload, len);
  xQueueSend(q, &p, 0);
}

void enviarParaFreenove(uint8_t tipo, const uint8_t* payload, uint8_t len) {
  enfileirarPkt(filaFreenoveTx, tipo, payload, len);
}

// ===============================================================
// NVS — apenas SSID+senha da última rede
// ===============================================================
void carregarWifiSalvo() {
  preferences.begin("wifi", true);
  wifiSavedSSID = preferences.getString("ssid", "");
  wifiSavedPass = preferences.getString("pass", "");
  preferences.end();
  Serial.printf("[WiFi] salvo: \"%s\"\n", wifiSavedSSID.c_str());
}

void salvarWifi(const String& ssid, const String& pass) {
  preferences.begin("wifi", false);
  preferences.putString("ssid", ssid);
  preferences.putString("pass", pass);
  preferences.end();
  wifiSavedSSID = ssid;
  wifiSavedPass = pass;
}

// ===============================================================
// CONEXÃO
// ===============================================================
void conectarNaRede(const String& ssid, const String& pass) {
  Serial.printf("[WiFi] tentando %s\n", ssid.c_str());
  WiFi.disconnect(true);
  delay(100);
  WiFi.begin(ssid.c_str(), pass.c_str());
  wifiAlvoSSID = ssid;
  wifiAlvoPass = pass;
  wifiEstado = WF_CONECTANDO;
  tInicioConexao = millis();
}

// ===============================================================
// TASK: RX WROOM
// ===============================================================
void TaskWroomRx(void*) {
  static uint8_t buf[300];
  static int bufLen = 0;

  for (;;) {
    while (Serial1.available()) {
      if (bufLen >= (int)sizeof(buf)) bufLen = 0;
      buf[bufLen++] = Serial1.read();
    }

    while (true) {
      int start = -1;
      for (int i = 0; i < bufLen - 1; i++) {
        if (buf[i] == HEADER_0 && buf[i + 1] == HEADER_1) { start = i; break; }
      }
      if (start < 0) { bufLen = 0; break; }
      if (start > 0) {
        memmove(buf, &buf[start], bufLen - start);
        bufLen -= start;
      }
      if (bufLen < 4) break;

      uint8_t tipo = buf[2];
      uint8_t len  = buf[3];
      if (bufLen < 4 + len) break;

      PacoteSerial p;
      p.tipo = tipo;
      p.len  = len;
      if (len > 0) memcpy(p.dados, &buf[4], len);
      xQueueSend(filaWroomRx, &p, 0);

      if (tipo != T_FFT) {
        Serial.printf("[RX-WROOM] tipo=0x%02X len=%u\n", tipo, len);
      }

      memmove(buf, &buf[4 + len], bufLen - (4 + len));
      bufLen -= (4 + len);
    }

    vTaskDelay(pdMS_TO_TICKS(1));
  }
}

// ===============================================================
// TASK: RX FREENOVE
// ===============================================================
void TaskFreenoveRx(void*) {
  static uint8_t buf[300];
  static int bufLen = 0;

  for (;;) {
    while (SerialFree.available()) {
      if (bufLen >= (int)sizeof(buf)) bufLen = 0;
      buf[bufLen++] = SerialFree.read();
    }

    while (true) {
      int start = -1;
      for (int i = 0; i < bufLen - 1; i++) {
        if (buf[i] == HEADER_0 && buf[i + 1] == HEADER_1) { start = i; break; }
      }
      if (start < 0) { bufLen = 0; break; }
      if (start > 0) {
        memmove(buf, &buf[start], bufLen - start);
        bufLen -= start;
      }
      if (bufLen < 4) break;

      uint8_t tipo = buf[2];
      uint8_t len  = buf[3];
      if (bufLen < 4 + len) break;

      PacoteSerial p;
      p.tipo = tipo;
      p.len  = len;
      if (len > 0) memcpy(p.dados, &buf[4], len);
      xQueueSend(filaFreenoveRx, &p, 0);

      Serial.printf("[RX-FREENOVE] tipo=0x%02X len=%u\n", tipo, len);

      memmove(buf, &buf[4 + len], bufLen - (4 + len));
      bufLen -= (4 + len);
    }

    vTaskDelay(pdMS_TO_TICKS(1));
  }
}

// ===============================================================
// TASK: TX FREENOVE
// ===============================================================
void TaskFreenoveTx(void*) {
  PacoteSerial p;
  for (;;) {
    if (xQueueReceive(filaFreenoveTx, &p, portMAX_DELAY) == pdTRUE) {
      SerialFree.write(HEADER_0);
      SerialFree.write(HEADER_1);
      SerialFree.write(p.tipo);
      SerialFree.write(p.len);
      if (p.len > 0) SerialFree.write(p.dados, p.len);
    }
  }
}

// ===============================================================
// TASK: TELEMETRIA → FREENOVE (30 Hz)
// ===============================================================
static void TaskTelemetria(void*) {
  TickType_t lastWake = xTaskGetTickCount();
  const TickType_t period = pdMS_TO_TICKS(33);

  for (;;) {
    uint8_t p[69];
    int i = 0;

    auto putf = [&](float f) { memcpy(&p[i], &f, 4); i += 4; };

    putf(telemetria.rpm);
    putf(telemetria.velocidade);
    putf(telemetria.maf);
    putf(telemetria.tps);
    putf(telemetria.pedal);
    putf(telemetria.tempCoolant);
    putf(telemetria.tempIntake);
    putf(telemetria.bateria);
    putf(telemetria.boost);
    putf(telemetria.boost_max);
    putf(telemetria.consumo_ml_min);
    putf(telemetria.consumo_l_100km);
    putf(telemetria.consumo_total_litros);
    putf(telemetria.zeroCemUltimo);
    putf(telemetria.modoExterno ? 30.0f : obdObterFPS());

    uint8_t flags = 0;
    if (telemetria.btConectado)  flags |= (1 << 0);
    if (telemetria.obdConectado) flags |= (1 << 1);
    if (telemetria.modoExterno)  flags |= (1 << 2);
    p[i++] = flags;

    uint32_t cont = obdContadorAmostras;
    memcpy(&p[i], &cont, 4); i += 4;

    uint32_t upt = millis();
    memcpy(&p[i], &upt, 4); i += 4;

    enviarParaFreenove(T_TELEMETRIA, p, i);

    vTaskDelayUntil(&lastWake, period);
  }
}

// ===============================================================
// TASK: STATUS OBD → FREENOVE (1 Hz)
// ===============================================================
static void TaskObdStatus(void*) {
  TickType_t lastWake = xTaskGetTickCount();
  const TickType_t period = pdMS_TO_TICKS(1000);

  for (;;) {
    int total = obterTotalPIDs();
    if (total > 12) total = 12;

    uint8_t p[4 + 12 * 21];
    int i = 0;

    uint8_t respondendo = 0;
    for (int k = 0; k < total; k++) {
      PIDView v = obterPIDView(k);
      if (v.suportado && v.respondendo) respondendo++;
    }

    p[i++] = obdELMResponde() ? 1 : 0;
    p[i++] = obdObterEstadoPIDs();
    p[i++] = (uint8_t)total;
    p[i++] = respondendo;

    for (int k = 0; k < total; k++) {
      PIDView v = obterPIDView(k);

      memset(&p[i], 0, 4);
      if (v.cmd) strncpy((char*)&p[i], v.cmd, 3);
      i += 4;

      memset(&p[i], 0, 12);
      if (v.nome) strncpy((char*)&p[i], v.nome, 11);
      i += 12;

      memcpy(&p[i], &v.ultimoValor, 4); i += 4;

      uint8_t flags = 0;
      if (v.suportado)   flags |= 1;
      if (v.respondendo) flags |= 2;
      p[i++] = flags;
    }

    enviarParaFreenove(T_OBD_PIDS, p, i);

    vTaskDelayUntil(&lastWake, period);
  }
}

// ===============================================================
// TASK: OBD (core 0)
// ===============================================================
static void vTaskOBD(void *pv) {
  for (;;) {
    atualizarDadosOBD2();
    vTaskDelay(1);
  }
}

// ===============================================================
// WIFI STATUS → FREENOVE
// ===============================================================
void enviarWifiStatus() {
  uint8_t p[51];
  int i = 0;
  p[i++] = wifiEstado;
  p[i++] = (wifiEstado == WF_CONECTADO) ? (int8_t)WiFi.RSSI() : 0;
  memset(&p[i], 0, 33);
  if (wifiEstado == WF_CONECTADO) WiFi.SSID().toCharArray((char*)&p[i], 33);
  i += 33;
  memset(&p[i], 0, 16);
  if (wifiEstado == WF_CONECTADO) WiFi.localIP().toString().toCharArray((char*)&p[i], 16);
  enviarParaFreenove(F_WIFI_STATUS, p, 51);
}

// ===============================================================
// PROCESSADOR — WROOM
// ===============================================================
void processarPacoteWroom(const PacoteSerial& p) {
  if (p.tipo == T_FFT && p.len == 32) {
    memcpy(fft32, p.dados, 32);
    ws.binaryAll(fft32, 32);
    enviarParaFreenove(T_FFT, p.dados, 32);
  }
  else if (p.tipo == T_MUSIC && p.len >= 10) {
    btConectado  = (p.dados[0] & (1 << 0)) != 0;
    audioAtivo   = (p.dados[0] & (1 << 1)) != 0;
    tempoAtualMs = ((uint32_t)p.dados[1] << 24) | ((uint32_t)p.dados[2] << 16) |
                   ((uint32_t)p.dados[3] << 8)  | p.dados[4];
    tempoTotalMs = ((uint32_t)p.dados[5] << 24) | ((uint32_t)p.dados[6] << 16) |
                   ((uint32_t)p.dados[7] << 8)  | p.dados[8];

    uint8_t lenTit = p.dados[9];
    uint8_t idx = 10;
    if (idx + lenTit <= p.len) {
      char tBuf[41] = {0};
      memcpy(tBuf, &p.dados[idx], min((int)lenTit, 40));
      musicaTitulo = String(tBuf);
      idx += lenTit;
      if (idx < p.len) {
        uint8_t lenArt = p.dados[idx];
        idx++;
        if (idx + lenArt <= p.len) {
          char aBuf[41] = {0};
          memcpy(aBuf, &p.dados[idx], min((int)lenArt, 40));
          musicaArtista = String(aBuf);
        }
      }
    }
    enviarParaFreenove(T_MUSIC, p.dados, p.len);
  }
}

// ===============================================================
// Echo AT — estado compartilhado
// ===============================================================
static uint32_t idEchoPendente = 0;
static unsigned long inicioEcho = 0;
static uint32_t timeoutEchoAtual = 3000;

// ===============================================================
// PROCESSADOR — FREENOVE
// ===============================================================
void processarPacoteFreenove(const PacoteSerial& p) {
  switch (p.tipo) {
    case F_BTN_EVENT: {
      if (p.len < 2) break;
      int id = p.dados[0];
      xQueueSend(filaBotoesQueue, &id, 0);
      Serial.printf("[FREENOVE] btn id=%d down=%d\n", id, p.dados[1]);
      break;
    }

    case F_OBD_REOB:
      Serial.println("[FREENOVE] forcar re-negociacao OBD");
      obdForcarRenegociacao();
      break;

    case F_WIFI_QUERY:
      enviarWifiStatus();
      break;

    case F_WIFI_CONN_REQ: {
      if (p.len < 33) break;
      char ssid[33] = {0};
      memcpy(ssid, p.dados, 32);
      char pass[65] = {0};
      if (p.len > 33) memcpy(pass, &p.dados[33], min((int)p.len - 33, 64));

      String s(ssid);
      String pw(pass);

      if (s.length() == 0) {
        if (wifiSavedSSID.length() == 0) {
          Serial.println("[FREENOVE] retry sem salva");
          break;
        }
        s  = wifiSavedSSID;
        pw = wifiSavedPass;
        Serial.printf("[FREENOVE] retry: %s\n", s.c_str());
      } else {
        Serial.printf("[FREENOVE] connect: %s (pw fornecida)\n", s.c_str());
      }

      conectarNaRede(s, pw);
      break;
    }

    case F_AT_CMD: {
      if (p.len < 2) break;
      int i = 0;
      uint8_t cmdLen = p.dados[i++];
      if (i + cmdLen > p.len) break;
      String cmd = "";
      for (int k = 0; k < cmdLen; k++) cmd += (char)p.dados[i + k];
      i += cmdLen;

      uint8_t termLen = p.dados[i++];
      String term = "";
      for (int k = 0; k < termLen; k++) term += (char)p.dados[i + k];
      i += termLen;

      uint32_t timeout = 3000;
      if (i + 4 <= p.len) {
        timeout = ((uint32_t)p.dados[i]) | ((uint32_t)p.dados[i+1] << 8) |
                  ((uint32_t)p.dados[i+2] << 16) | ((uint32_t)p.dados[i+3] << 24);
      }

      if (idEchoPendente != 0) {
        Serial.println("[FREENOVE] AT cmd ignorado (echo pendente)");
        break;
      }
      timeoutEchoAtual = timeout;
      idEchoPendente = btEnviarATPublico(cmd, term, timeout, DONO_DEBUG);
      inicioEcho = millis();
      break;
    }

    default:
      Serial.printf("[FREENOVE] tipo desconhecido 0x%02X len=%u\n", p.tipo, p.len);
      break;
  }
}

// ===============================================================
// AT RESPONSE POLLING — responde via WS
// ===============================================================
void processarRespostaAt() {
  if (idEchoPendente == 0) return;

  String resp;
  bool ok;
  if (btObterRespostaPublico(idEchoPendente, resp, ok, DONO_DEBUG)) {
    StaticJsonDocument<640> doc;
    doc["tipo"] = "echo_resp";
    doc["ok"] = ok;
    doc["resp"] = resp;
    String buf; serializeJson(doc, buf);
    ws.textAll(buf);

    uint8_t pkt[1 + 256];
    pkt[0] = ok ? 1 : 0;
    uint16_t rl = min((int)resp.length(), 254);
    memcpy(&pkt[1], resp.c_str(), rl);
    enviarParaFreenove(F_AT_RESP, pkt, 1 + rl);

    idEchoPendente = 0;
  }
  else if (millis() - inicioEcho > timeoutEchoAtual + 2000) {
    StaticJsonDocument<128> doc;
    doc["tipo"] = "echo_resp";
    doc["ok"] = false;
    doc["resp"] = "(timeout do servidor)";
    String buf; serializeJson(doc, buf);
    ws.textAll(buf);

    uint8_t pkt[1] = {0};
    enviarParaFreenove(F_AT_RESP, pkt, 1);

    idEchoPendente = 0;
  }
}

// ===============================================================
// BOTÕES — mídia + volume absoluto
// ===============================================================
void processarAcaoBotao(int idBtn) {
  if (idBtn < 0 || idBtn > 4) return;

  switch (idBtn) {
    case 0: {   // PREV
      uint8_t p[1] = { 1 };
      enviarParaWroom(T_MEDIA_CMD, p, 1);
      Serial.println("[BTN] PREV -> WROOM");
      break;
    }
    case 1: {   // PLAY/PAUSE
      uint8_t p[1] = { 3 };
      enviarParaWroom(T_MEDIA_CMD, p, 1);
      Serial.println("[BTN] PLAY/PAUSE -> WROOM");
      break;
    }
    case 2: {   // NEXT
      uint8_t p[1] = { 2 };
      enviarParaWroom(T_MEDIA_CMD, p, 1);
      Serial.println("[BTN] NEXT -> WROOM");
      break;
    }
    case 3: {   // VOL -
      if (volumeMini >= VOLUME_STEP) volumeMini -= VOLUME_STEP;
      else                            volumeMini = 0;
      uint8_t raw = (uint8_t)((uint16_t)volumeMini * 127 / 100);
      uint8_t p[2] = { 6, raw };
      enviarParaWroom(T_MEDIA_CMD, p, 2);
      Serial.printf("[BTN] VOL- %u%% (raw=%u)\n", volumeMini, raw);
      break;
    }
    case 4: {   // VOL +
      if ((uint16_t)volumeMini + VOLUME_STEP <= 100) volumeMini += VOLUME_STEP;
      else                                            volumeMini = 100;
      uint8_t raw = (uint8_t)((uint16_t)volumeMini * 127 / 100);
      uint8_t p[2] = { 6, raw };
      enviarParaWroom(T_MEDIA_CMD, p, 2);
      Serial.printf("[BTN] VOL+ %u%% (raw=%u)\n", volumeMini, raw);
      break;
    }
  }
}

// ===============================================================
// WS
// ===============================================================
void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client,
               AwsEventType type, void *arg, uint8_t *data, size_t len) {
  if (type == WS_EVT_CONNECT) {
    client->text("SYS|conectado");
  } else if (type == WS_EVT_DISCONNECT) {
  } else if (type == WS_EVT_DATA) {
    AwsFrameInfo *info = (AwsFrameInfo*)arg;
    if (info->final && info->index == 0 && info->len == len &&
        info->opcode == WS_TEXT) {
      String msg = "";
      for (size_t i = 0; i < len; i++) msg += (char)data[i];

      if (msg.startsWith("{")) {
        StaticJsonDocument<640> doc;
        DeserializationError err = deserializeJson(doc, msg);
        if (err) return;
        const char* tipo = doc["tipo"] | "";

        if (strcmp(tipo, "inject") == 0) {
          if (!telemetria.modoExterno) return;
          if (doc.containsKey("rpm"))           telemetria.rpm              = doc["rpm"];
          if (doc.containsKey("vel"))           telemetria.velocidade       = doc["vel"];
          if (doc.containsKey("maf"))           telemetria.maf              = doc["maf"];
          if (doc.containsKey("boost"))         telemetria.boost            = doc["boost"];
          if (doc.containsKey("boost_max"))     telemetria.boost_max        = doc["boost_max"];
          if (doc.containsKey("tps"))           telemetria.tps              = doc["tps"];
          if (doc.containsKey("pedal"))         telemetria.pedal            = doc["pedal"];
          if (doc.containsKey("temp_coolant"))  telemetria.tempCoolant      = doc["temp_coolant"];
          if (doc.containsKey("temp_intake"))   telemetria.tempIntake       = doc["temp_intake"];
          if (doc.containsKey("bateria"))       telemetria.bateria          = doc["bateria"];
          if (doc.containsKey("consumo_ml"))    telemetria.consumo_ml_min   = doc["consumo_ml"];
          if (doc.containsKey("consumo_l100"))  telemetria.consumo_l_100km  = doc["consumo_l100"];
          if (doc.containsKey("consumo_total")) telemetria.consumo_total_litros = doc["consumo_total"];
          if (doc.containsKey("zero_cem"))      telemetria.zeroCemUltimo    = doc["zero_cem"];
        }
        else if (strcmp(tipo, "mode") == 0) {
          bool novo = doc["external"] | false;
          if (telemetria.modoExterno && !novo) obdZerarAcumulador();
          telemetria.modoExterno = novo;
        }
        else if (strcmp(tipo, "echo") == 0) {
          if (idEchoPendente != 0) {
            StaticJsonDocument<128> r;
            r["tipo"] = "echo_resp";
            r["ok"] = false;
            r["resp"] = "(ja tem comando pendente)";
            String buf; serializeJson(r, buf);
            client->text(buf);
            return;
          }
          const char* cmd  = doc["cmd"]  | "";
          const char* term = doc["term"] | ">";
          uint32_t timeout = doc["timeout"] | 3000;
          if (strlen(cmd) == 0) return;

          timeoutEchoAtual = timeout;
          idEchoPendente = btEnviarATPublico(String(cmd), String(term), timeout, DONO_DEBUG);
          inicioEcho = millis();
        }
        return;
      }

      if (msg.startsWith("SEND|")) {
        int p1 = msg.indexOf('|', 5);
        int p2 = msg.indexOf('|', p1 + 1);
        if (p1 < 0 || p2 < 0) return;

        String cmd  = msg.substring(5, p1);
        String term = msg.substring(p1 + 1, p2);
        uint32_t t  = msg.substring(p2 + 1).toInt();
        if (t < 1000) t = 3000;

        if (idEchoPendente != 0) {
          client->text("ERR|ja tem comando pendente");
          return;
        }
        timeoutEchoAtual = t;
        idEchoPendente = btEnviarATPublico(cmd, term, t, DONO_DEBUG);
        inicioEcho = millis();
        client->text("TX|" + cmd);
      }
    }
  }
}

// ===============================================================
// SETUP
// ===============================================================
void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n=== CAR ROUTER MODULE ===");

  filaWroomRx     = xQueueCreate(20, sizeof(PacoteSerial));
  filaFreenoveRx  = xQueueCreate(20, sizeof(PacoteSerial));
  filaFreenoveTx  = xQueueCreate(20, sizeof(PacoteSerial));
  filaBotoesQueue = xQueueCreate(16, sizeof(int));

  Serial1.begin(115200, SERIAL_8N1, RX_WROOM, TX_WROOM);
  SerialFree.begin(115200, SERIAL_8N1, RX_FREE, TX_FREE);
  Serial.println("[UART] WROOM + Freenove OK");

  xTaskCreatePinnedToCore(TaskWroomRx,    "WroomRx",    4096, NULL, 2, NULL, 0);
  xTaskCreatePinnedToCore(TaskFreenoveRx, "FreeRx",     4096, NULL, 2, NULL, 0);
  xTaskCreatePinnedToCore(TaskFreenoveTx, "FreeTx",     4096, NULL, 2, NULL, 0);
  xTaskCreatePinnedToCore(TaskTelemetria, "Telemetria", 4096, NULL, 1, NULL, 1);
  xTaskCreatePinnedToCore(TaskObdStatus,  "ObdStatus",  4096, NULL, 1, NULL, 1);

  for (int i = 0; i < NUM_BOTOES; i++) {
    botoes[i].pino = PINOS_BTN[i];
    botoes[i].estadoEstavel = HIGH;
    botoes[i].ultimaMudanca = 0;
    pinMode(PINOS_BTN[i], INPUT_PULLUP);
    attachInterruptArg(PINOS_BTN[i], isrBotao, (void*)i, CHANGE);
  }

  // ==================== WiFi ====================
  WiFi.mode(WIFI_STA);
  WiFi.setHostname("mini-car");
  WiFi.setAutoReconnect(false); 

  carregarWifiSalvo();

  if (wifiSavedSSID.length() > 0) {
    conectarNaRede(wifiSavedSSID, wifiSavedPass);
  } else {
    Serial.println("[WiFi] nenhuma rede salva (use menu WiFi no FREENOVE)");
    wifiEstado = WF_FALHA;
  }

  // ==================== BT ====================
  inicializarBTManager();

  // ==================== OBD ====================
  inicializarOBD2();
  xTaskCreatePinnedToCore(vTaskOBD, "TaskOBD", 6144, NULL, 2, NULL, 0);

  // ==================== Rotas HTTP ====================
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *req){
    req->send_P(200, "text/html", INDEX_HTML);
  });
  server.on("/bt", HTTP_GET, [](AsyncWebServerRequest *req){
    req->send_P(200, "text/html", BT_HTML);
  });
  server.on("/admin", HTTP_GET, [](AsyncWebServerRequest *req){
    req->send_P(200, "text/html", ADMIN_HTML);
  });
  server.on("/cmd", HTTP_GET, [](AsyncWebServerRequest *req){
    if (req->hasParam("btn")) {
      int id = req->getParam("btn")->value().toInt();
      if (id >= 0 && id <= 6) xQueueSend(filaBotoesQueue, &id, 0);
    }
    req->send(200, "text/plain", "OK");
  });

  server.on("/api/admin/get", HTTP_GET, [](AsyncWebServerRequest *req){
    StaticJsonDocument<256> doc;
    doc["motor"] = telemetria.motor_litros;
    doc["ve"] = telemetria.eficiencia_ve;
    doc["sim_mode"] = telemetria.modoExterno;
    String buf; serializeJson(doc, buf);
    req->send(200, "application/json", buf);
  });
  server.on("/api/admin/set", HTTP_GET, [](AsyncWebServerRequest *req){
    float novoMotor = telemetria.motor_litros;
    float novaVE = telemetria.eficiencia_ve;
    if (req->hasParam("motor")) novoMotor = req->getParam("motor")->value().toFloat();
    if (req->hasParam("ve"))    novaVE    = req->getParam("ve")->value().toFloat();
    salvarAjusteMotorFlash(novoMotor, novaVE);
    StaticJsonDocument<128> doc;
    doc["ok"] = true;
    doc["motor"] = telemetria.motor_litros;
    doc["ve"] = telemetria.eficiencia_ve;
    String buf; serializeJson(doc, buf);
    req->send(200, "application/json", buf);
  });
  server.on("/api/admin/debug", HTTP_GET, [](AsyncWebServerRequest *req){
    if (req->hasParam("on")) {
      int v = req->getParam("on")->value().toInt();
      bool novo = (v != 0);
      if (telemetria.modoExterno && !novo) obdZerarAcumulador();
      telemetria.modoExterno = novo;
    }
    StaticJsonDocument<64> doc;
    doc["sim_mode"] = telemetria.modoExterno;
    String buf; serializeJson(doc, buf);
    req->send(200, "application/json", buf);
  });
  server.on("/api/admin/inject", HTTP_GET, [](AsyncWebServerRequest *req){
    if (!telemetria.modoExterno) {
      req->send(409, "application/json", "{\"erro\":\"modo externo desligado\"}");
      return;
    }
    int count = 0;
    if (req->hasParam("rpm"))           { telemetria.rpm              = req->getParam("rpm")->value().toFloat(); count++; }
    if (req->hasParam("vel"))           { telemetria.velocidade       = req->getParam("vel")->value().toFloat(); count++; }
    if (req->hasParam("maf"))           { telemetria.maf              = req->getParam("maf")->value().toFloat(); count++; }
    if (req->hasParam("boost"))         { telemetria.boost            = req->getParam("boost")->value().toFloat(); count++; }
    if (req->hasParam("boost_max"))     { telemetria.boost_max        = req->getParam("boost_max")->value().toFloat(); count++; }
    if (req->hasParam("tps"))           { telemetria.tps              = req->getParam("tps")->value().toFloat(); count++; }
    if (req->hasParam("pedal"))         { telemetria.pedal            = req->getParam("pedal")->value().toFloat(); count++; }
    if (req->hasParam("temp_coolant"))  { telemetria.tempCoolant      = req->getParam("temp_coolant")->value().toFloat(); count++; }
    if (req->hasParam("temp_intake"))   { telemetria.tempIntake       = req->getParam("temp_intake")->value().toFloat(); count++; }
    if (req->hasParam("bateria"))       { telemetria.bateria          = req->getParam("bateria")->value().toFloat(); count++; }
    if (req->hasParam("consumo_ml"))    { telemetria.consumo_ml_min   = req->getParam("consumo_ml")->value().toFloat(); count++; }
    if (req->hasParam("consumo_l100"))  { telemetria.consumo_l_100km  = req->getParam("consumo_l100")->value().toFloat(); count++; }
    if (req->hasParam("consumo_total")) { telemetria.consumo_total_litros = req->getParam("consumo_total")->value().toFloat(); count++; }
    if (req->hasParam("zero_cem"))      { telemetria.zeroCemUltimo    = req->getParam("zero_cem")->value().toFloat(); count++; }

    StaticJsonDocument<128> doc;
    doc["ok"] = true;
    doc["count"] = count;
    String buf; serializeJson(doc, buf);
    req->send(200, "application/json", buf);
  });

  server.on("/api/bt/debug", HTTP_GET, [](AsyncWebServerRequest *req){
    if (req->hasParam("on")) {
      int v = req->getParam("on")->value().toInt();
      if (v != 0) { btIniciarDebug(); req->send(200, "text/plain", "debug ATIVADO"); }
      else        { btPararDebug();  req->send(200, "text/plain", "debug DESATIVADO"); }
    } else {
      req->send(200, "text/plain", btDebugAtivo() ? "ATIVO" : "INATIVO");
    }
  });

  server.on("/api/bt/baud", HTTP_GET, [](AsyncWebServerRequest *req){
    if (req->hasParam("set")) {
      uint32_t novo = (uint32_t)req->getParam("set")->value().toInt();
      bool atualizarXM = req->hasParam("xm") &&
                         (req->getParam("xm")->value().toInt() != 0);
      StaticJsonDocument<128> doc;
      doc["ok"] = true;
      doc["baud"] = novo;
      doc["xm_atualizado"] = atualizarXM;
      doc["restart"] = true;
      String buf; serializeJson(doc, buf);
      req->send(200, "application/json", buf);
      delay(200);
      btMudarBaud(novo, atualizarXM);
      return;
    }
    StaticJsonDocument<64> doc;
    doc["baud"] = btObterBaud();
    String buf; serializeJson(doc, buf);
    req->send(200, "application/json", buf);
  });

  ws.onEvent(onWsEvent);
  server.addHandler(&ws);
  server.begin();

  Serial.println("=== PRONTO ===");
}

// ===============================================================
// LOOP
// ===============================================================
void loop() {
  PacoteSerial p;

  while (xQueueReceive(filaWroomRx, &p, 0) == pdTRUE) {
    processarPacoteWroom(p);
  }

  while (xQueueReceive(filaFreenoveRx, &p, 0) == pdTRUE) {
    processarPacoteFreenove(p);
  }

  processarRespostaAt();

  // ==================== WiFi state machine ====================
  if (wifiEstado == WF_CONECTANDO) {
    if (WiFi.status() == WL_CONNECTED) {
      wifiEstado = WF_CONECTADO;

      String atualSSID = WiFi.SSID();
      Serial.printf("[WiFi] conectado: %s / %s\n",
                    atualSSID.c_str(), WiFi.localIP().toString().c_str());

      salvarWifi(atualSSID, wifiAlvoPass);

      enviarWifiStatus();
    }
    else if (millis() - tInicioConexao > WIFI_TIMEOUT_CONEXAO_MS) {
      wifiEstado = WF_FALHA;
      Serial.println("[WiFi] timeout, sem retry");
      enviarWifiStatus();
    }
  }
  else if (wifiEstado == WF_CONECTADO && WiFi.status() != WL_CONNECTED) {
    wifiEstado = WF_FALHA;
    Serial.println("[WiFi] conexao perdida, sem retry");
    enviarWifiStatus();
  }

  if (millis() - tUltimoWifiStatus >= WIFI_STATUS_PERIOD) {
    tUltimoWifiStatus = millis();
    enviarWifiStatus();
  }

  int idBtn;
  while (xQueueReceive(filaBotoesQueue, &idBtn, 0) == pdTRUE) {
    processarAcaoBotao(idBtn);
  }

  // ==================== Flush debug BT pro WS ====================
  if (btDebugAtivo()) {
    if (ws.count() > 0) {
      String dbg = btObterDebug();
      if (dbg.length() > 2) {
        DynamicJsonDocument doc(dbg.length() * 2 + 64);
        doc["tipo"] = "bt_debug";
        doc["data"] = dbg;
        String buf;
        serializeJson(doc, buf);
        ws.textAll(buf);
      }
    } else {
      btObterDebug();
    }
  }

  ws.cleanupClients();

  vTaskDelay(pdMS_TO_TICKS(1));
}