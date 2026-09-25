#include "src/core/uart_router.h"
#include "src/core/display_manager.h"
#include "src/common/telemetria.h"
#include "src/common/audio.h"
#include "src/common/estado.h"

#define HEADER_0 0xAA
#define HEADER_1 0x55

#define T_FFT           0x01
#define T_MUSIC         0x02
#define T_TELEMETRIA    0x04
#define T_OBD_PIDS      0x05

#define F_WIFI_STATUS   0x10
#define F_AT_RESP       0x13
#define F_BTN_EVENT     0x14

#define F_WIFI_QUERY    0x30
#define F_WIFI_CONN_REQ 0x32
#define F_AT_CMD        0x34
#define F_OBD_REOB      0x35

#define RX_MINI 44
#define TX_MINI 43

static HardwareSerial SerialMini(1);
static QueueHandle_t filaDebug;
static WifiStatusMini  statusWifi = { false, 0, "", "", 0 };
static ObdStatusGeral  statusObd  = {};

static volatile uint32_t contFFT = 0, contMusic = 0, contTele = 0, contObdStat = 0;

// ===============================================================
void uart_router_init() {
  SerialMini.begin(115200, SERIAL_8N1, RX_MINI, TX_MINI);
  filaDebug = xQueueCreate(20, sizeof(String*));
}

static void pushDebug(const String& s) {
  if (!filaDebug) return;
  String* p = new String(s);
  if (xQueueSend(filaDebug, &p, 0) != pdTRUE) delete p;
}

bool uart_get_debug_linha(String& out) {
  if (!filaDebug) return false;
  String* p = nullptr;
  if (xQueueReceive(filaDebug, &p, 0) == pdTRUE) {
    out = *p; delete p; return true;
  }
  return false;
}

WifiStatusMini uart_get_wifi_status() { return statusWifi; }
ObdStatusGeral uart_get_obd_status()  { return statusObd; }

uint32_t uart_cont_fft()       { return contFFT; }
uint32_t uart_cont_music()     { return contMusic; }
uint32_t uart_cont_tele()      { return contTele; }
uint32_t uart_cont_obdstatus() { return contObdStat; }

// ===============================================================
static void write_header(uint8_t tipo, uint8_t len) {
  SerialMini.write(HEADER_0);
  SerialMini.write(HEADER_1);
  SerialMini.write(tipo);
  SerialMini.write(len);
}

void uart_send_wifi_query() { write_header(F_WIFI_QUERY, 0); }

void uart_send_wifi_connect(const String& ssid, const String& pass) {
  uint8_t pkt[2 + 33 + 65];
  int p = 0;
  pkt[p++] = HEADER_0;
  pkt[p++] = HEADER_1;
  pkt[p++] = F_WIFI_CONN_REQ;
  pkt[p++] = 33 + min((int)pass.length(), 64);
  memset(&pkt[p], 0, 33);
  ssid.toCharArray((char*)&pkt[p], 33);
  p += 33;
  pass.toCharArray((char*)&pkt[p], min((int)pass.length() + 1, 65));
  p += min((int)pass.length(), 64);
  SerialMini.write(pkt, p);
}

void uart_send_wifi_retry() {
  // SSID vazio = mini usa a última salva
  uart_send_wifi_connect("", "");
}

void uart_send_btn(uint8_t id, uint8_t estado) {
  uint8_t pkt[6] = { HEADER_0, HEADER_1, F_BTN_EVENT, 2, id, estado };
  SerialMini.write(pkt, 6);
}

void uart_send_media(uint8_t id) {
  if (id > 2) return;
  uart_send_btn(id, 1);
}

void uart_send_obd_reob() { write_header(F_OBD_REOB, 0); }

void uart_send_at(const String& cmd, const String& term, uint32_t timeout) {
  uint8_t pkt[1 + 96 + 1 + 8 + 4 + 4];
  int p = 0;
  pkt[p++] = HEADER_0;
  pkt[p++] = HEADER_1;
  pkt[p++] = F_AT_CMD;
  int lenPos = p++;
  uint8_t cl = min((int)cmd.length(), 95);
  pkt[p++] = cl;
  memcpy(&pkt[p], cmd.c_str(), cl); p += cl;
  uint8_t tl = min((int)term.length(), 7);
  pkt[p++] = tl;
  memcpy(&pkt[p], term.c_str(), tl); p += tl;
  pkt[p++] = timeout & 0xFF;
  pkt[p++] = (timeout >> 8)  & 0xFF;
  pkt[p++] = (timeout >> 16) & 0xFF;
  pkt[p++] = (timeout >> 24) & 0xFF;
  pkt[lenPos] = p - lenPos - 1;
  SerialMini.write(pkt, p);
}

// ===============================================================
// Aplicadores
// ===============================================================
static void aplicarTelemetria(const uint8_t* b, uint8_t len) {
  if (len < 69) return;
  int i = 0;
  auto getf = [&]() -> float { float f; memcpy(&f, &b[i], 4); i += 4; return f; };

  gTelemetria.rpm                  = getf();
  gTelemetria.velocidade           = getf();
  gTelemetria.maf                  = getf();
  gTelemetria.tps                  = getf();
  gTelemetria.pedal                = getf();
  gTelemetria.tempCoolant          = getf();
  gTelemetria.tempIntake           = getf();
  gTelemetria.bateria              = getf();
  gTelemetria.boost                = getf();
  gTelemetria.boostMax             = getf();
  gTelemetria.consumo_ml_min       = getf();
  gTelemetria.consumo_l_100km      = getf();
  gTelemetria.consumo_total_litros = getf();
  gTelemetria.zeroCemUltimo        = getf();
  gTelemetria.taxaOBD              = getf();

  uint8_t flags = b[i++];
  gTelemetria.btConectado  = (flags & 1) != 0;
  gTelemetria.obdConectado = (flags & 2) != 0;
  gTelemetria.modoExterno  = (flags & 4) != 0;
  gTelemetria.lastUpdateMs = millis();
}

static void aplicarFFT(const uint8_t* b, uint8_t len) {
  if (len < 9) return;
  for (int i = 0; i < FFT_BANDAS_ORIGINAIS && i < len; i++) gAudio.fftBins[i] = b[i];
  gAudio.lastFFTMs = millis();
}

static void aplicarMusic(const uint8_t* b, uint8_t len) {
  if (len < 10) return;
  gAudio.btConectado  = (b[0] & 1) != 0;
  gAudio.audioAtivo   = (b[0] & 2) != 0;
  gAudio.tempoAtualMs = ((uint32_t)b[1] << 24) | ((uint32_t)b[2] << 16) |
                        ((uint32_t)b[3] << 8)  | b[4];
  gAudio.tempoTotalMs = ((uint32_t)b[5] << 24) | ((uint32_t)b[6] << 16) |
                        ((uint32_t)b[7] << 8)  | b[8];
  uint8_t lenTit = b[9]; uint8_t idx = 10;
  if (idx + lenTit <= len) {
    char tBuf[41] = {0};
    memcpy(tBuf, &b[idx], min((int)lenTit, 40));
    strncpy(gAudio.titulo, tBuf, 40);
    idx += lenTit;
    if (idx < len) {
      uint8_t lenArt = b[idx]; idx++;
      if (idx + lenArt <= len) {
        char aBuf[41] = {0};
        memcpy(aBuf, &b[idx], min((int)lenArt, 40));
        strncpy(gAudio.artista, aBuf, 40);
      }
    }
  }
  gAudio.lastMusicMs = millis();
}

static void aplicarObdPids(const uint8_t* b, uint8_t len) {
  if (len < 4) return;
  statusObd.elmResponde = b[0] != 0;
  statusObd.estadoGeral = b[1];
  statusObd.total       = b[2];
  statusObd.respondendo = b[3];
  if (statusObd.total > OBD_MAX_PIDS) statusObd.total = OBD_MAX_PIDS;

  int i = 4;
  for (int k = 0; k < statusObd.total; k++) {
    if (i + 21 > len) break;
    ObdPidStatus& p = statusObd.pids[k];
    memset(p.cmd, 0, 4);
    memcpy(p.cmd, &b[i], 3); i += 4;
    memset(p.nome, 0, 12);
    memcpy(p.nome, &b[i], 11); i += 12;
    memcpy(&p.valor, &b[i], 4); i += 4;
    uint8_t f = b[i++];
    p.suportado   = (f & 1) != 0;
    p.respondendo = (f & 2) != 0;
  }
}

// ===============================================================
void uart_router_loop() {
  static uint8_t buf[400];
  static int bufLen = 0;

  while (SerialMini.available()) {
    if (bufLen >= (int)sizeof(buf)) bufLen = 0;
    buf[bufLen++] = SerialMini.read();
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

    uint8_t* payload = &buf[4];

    switch (tipo) {
      case T_TELEMETRIA: aplicarTelemetria(payload, len); contTele++;     break;
      case T_FFT:        aplicarFFT(payload, len);        contFFT++;      break;
      case T_MUSIC:      aplicarMusic(payload, len);      contMusic++;
                         pushDebug("[RX] Music update");  break;
      case T_OBD_PIDS:   aplicarObdPids(payload, len);    contObdStat++;  break;

      case F_WIFI_STATUS: {
        if (len < 51) break;
        statusWifi.valido = true;
        statusWifi.estado = payload[0];
        statusWifi.rssi   = (int8_t)payload[1];
        memcpy(statusWifi.ssid, &payload[2], 33);
        memcpy(statusWifi.ip,   &payload[35], 16);
        break;
      }

      case F_AT_RESP: {
        uint8_t ok = payload[0];
        String resp = "";
        for (int i = 1; i < len; i++) resp += (char)payload[i];
        pushDebug((ok ? "[RX] " : "[ERR] ") + resp);
        break;
      }
      case F_BTN_EVENT: {
        if (len < 2) break;
        pushDebug(String("[RX] Btn ") + payload[0] + " " + (payload[1] ? "down" : "up"));
        break;
      }
    }

    int restante = bufLen - (4 + len);
    if (restante > 0) memmove(buf, &buf[4 + len], restante);
    bufLen = restante;
  }

  // ★ Taxa de FFT 1 Hz
  static uint32_t ultFFT = 0;
  static uint32_t tTaxa  = 0;
  uint32_t agora = millis();
  if (agora - tTaxa >= 1000) {
    uint32_t cFFT = contFFT;
    gAudio.taxaFFT = (cFFT - ultFFT) * 1000.0f / (agora - tTaxa);
    ultFFT = cFFT;
    tTaxa  = agora;
  }
}