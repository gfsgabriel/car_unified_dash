#include "src/screens/screen_wifi.h"
#include "src/screens/screen_base.h"
#include "src/core/mode_manager.h"
#include "src/core/display_manager.h"
#include "src/core/uart_router.h"
#include "src/common/config.h"

// ===============================================================
// Estados
// ===============================================================
enum WifiView {
  WV_MAIN,
  WV_KBD_SSID,
  WV_KBD_PASS,
  WV_CONNECTING,
};

enum KbdLayer { KBD_LOWER, KBD_UPPER, KBD_SYM };

static WifiView  view       = WV_MAIN;
static KbdLayer  kbdLayer   = KBD_LOWER;
static String    kbdBuffer  = "";
static String    ssidAlvo   = "";
static uint32_t  tConexaoIni = 0;
static int       hoverIdx   = -1;
static int       hoverKey   = -1;
static int       hoverBtn   = -1;

// Teclado layout
#define KBD_ROW_H      40
#define KBD_Y0         60
#define KBD_NUM_Y      (KBD_Y0)
#define KBD_L1_Y       (KBD_Y0 + KBD_ROW_H)
#define KBD_L2_Y       (KBD_Y0 + KBD_ROW_H * 2)
#define KBD_L3_Y       (KBD_Y0 + KBD_ROW_H * 3)
#define KBD_BTN1_Y     (KBD_Y0 + KBD_ROW_H * 4)
#define KBD_BTN2_Y     (KBD_Y0 + KBD_ROW_H * 4 + 50)

// Teclas
static const char* KEY_NUMS = "1234567890";
static const char* KEY_ROW_LOWER[3] = { "qwertyuiop", "asdfghjkl", "zxcvbnm" };
static const char* KEY_ROW_UPPER[3] = { "QWERTYUIOP", "ASDFGHJKL", "ZXCVBNM" };
static const char* KEY_ROW_SYM[3]   = { "!@#$%^&*()", "-_=+[]{};:", "'\",.<>/?\\|" };

static const char* linhaAtual(int row) {
  if (row == 0) return KEY_NUMS;
  if (kbdLayer == KBD_SYM) return KEY_ROW_SYM[row - 1];
  return (kbdLayer == KBD_UPPER) ? KEY_ROW_UPPER[row - 1] : KEY_ROW_LOWER[row - 1];
}

static int teclaHit(int x, int y) {
  int rows[4] = { KBD_NUM_Y, KBD_L1_Y, KBD_L2_Y, KBD_L3_Y };
  for (int r = 0; r < 4; r++) {
    int yTop = rows[r];
    int yBot = yTop + KBD_ROW_H;
    if (y < yTop || y >= yBot) continue;
    const char* row = linhaAtual(r);
    int len = strlen(row);
    if (len == 0) return -1;
    int kw = 48;
    int total = len * kw;
    int x0 = (SCR_W - total) / 2;
    if (x < x0) return -1;
    int col = (x - x0) / kw;
    if (col < 0 || col >= len) return -1;
    return r * 10 + col;
  }
  return -1;
}

static char teclaChar(int hit) {
  int r = hit / 10;
  int c = hit % 10;
  const char* row = linhaAtual(r);
  if (c < (int)strlen(row)) return row[c];
  return 0;
}

static int btnHit(int x, int y) {
  if (y >= KBD_BTN1_Y && y < KBD_BTN1_Y + 50) {
    int w = SCR_W / 3;
    return (x / w) + 10;
  }
  if (y >= KBD_BTN2_Y && y < KBD_BTN2_Y + 50) {
    int w = SCR_W / 2;
    return (x / w) + 20;
  }
  return -1;
}

// ===============================================================
// Desenho
// ===============================================================
static void desenharHeader(const char* titulo) {
  canvas.fillRect(0, 0, SCR_W, 28, 0x18E3);
  canvas.setTextFont(2);
  canvas.setTextColor(TFT_CYAN);
  canvas.setCursor(8, 6);
  canvas.print(titulo);
}

static void desenharMain() {
  desenharHeader("WI-FI");

  WifiStatusMini st = uart_get_wifi_status();

  canvas.setTextFont(2);
  if (st.estado == 2) {
    canvas.setTextColor(TFT_GREEN);
    canvas.setCursor(20, 60);
    canvas.printf("SSID: %s", st.ssid);
    canvas.setTextColor(TFT_WHITE);
    canvas.setCursor(20, 90);
    canvas.printf("IP: %s", st.ip);
    canvas.setTextColor(TFT_DARKGREY);
    canvas.setCursor(20, 120);
    canvas.printf("RSSI: %d dBm", st.rssi);
  } else if (st.estado == 1) {
    canvas.setTextColor(TFT_YELLOW);
    canvas.setCursor(20, 80);
    canvas.print("Conectando...");
  } else if (st.estado == 3) {
    canvas.setTextColor(TFT_RED);
    canvas.setCursor(20, 80);
    canvas.print("Falha na conexao");
  } else {
    canvas.setTextColor(TFT_DARKGREY);
    canvas.setCursor(20, 80);
    canvas.print("Desconectado");
  }

  // Botões
  const int by = SCR_H - 56;
  const int bw = SCR_W / 3;
  const char* labels[3] = { "NOVA", "RETRY", "VOLTAR" };
  for (int i = 0; i < 3; i++) {
    uint16_t cor = (hoverBtn == i) ? 0x049F : ((i == 2) ? 0x8000 : 0x18E3);
    canvas.fillRect(i * bw + 2, by, bw - 4, 48, cor);
    canvas.drawRect(i * bw + 2, by, bw - 4, 48, TFT_WHITE);
    canvas.setTextColor(TFT_WHITE);
    canvas.setTextDatum(MC_DATUM);
    canvas.drawString(labels[i], i * bw + bw / 2, by + 24);
  }
  canvas.setTextDatum(TL_DATUM);
}

static void desenharTeclado() {
  const char* titulo = (view == WV_KBD_SSID) ? "SSID" : "SENHA";
  desenharHeader(titulo);

  // Campo de entrada
  canvas.fillRect(8, 30, SCR_W - 16, 26, 0x18E3);
  canvas.drawRect(8, 30, SCR_W - 16, 26, TFT_WHITE);
  canvas.setTextFont(2);
  canvas.setTextColor(TFT_GREEN);
  canvas.setCursor(14, 34);
  canvas.print(kbdBuffer);
  if ((millis() / 500) % 2 == 0) {
    int cx = 14 + canvas.textWidth(kbdBuffer);
    if (cx < SCR_W - 20) canvas.drawFastVLine(cx, 33, 20, TFT_GREEN);
  }

  // Teclas
  int rows[4] = { KBD_NUM_Y, KBD_L1_Y, KBD_L2_Y, KBD_L3_Y };
  for (int r = 0; r < 4; r++) {
    const char* row = linhaAtual(r);
    int len = strlen(row);
    if (len == 0) continue;
    int kw = 48;
    int total = len * kw;
    int x0 = (SCR_W - total) / 2;
    int y  = rows[r];

    for (int i = 0; i < len; i++) {
      int x = x0 + i * kw;
      int hit = r * 10 + i;
      uint16_t cor = (hoverKey == hit) ? 0x049F : 0x18E3;
      canvas.fillRect(x + 1, y + 1, kw - 2, KBD_ROW_H - 2, cor);
      canvas.drawRect(x + 1, y + 1, kw - 2, KBD_ROW_H - 2, TFT_WHITE);
      canvas.setTextColor(TFT_WHITE);
      canvas.setTextDatum(MC_DATUM);
      canvas.setTextFont(2);
      char buf[2] = { row[i], 0 };
      canvas.drawString(buf, x + kw / 2, y + KBD_ROW_H / 2);
    }
  }
  canvas.setTextDatum(TL_DATUM);

  // Botões 1 (SHIFT / SYM / BK)
  const int bw3 = SCR_W / 3;
  const char* l1[3] = {
    (kbdLayer == KBD_UPPER) ? "ABC" : "abc",
    (kbdLayer == KBD_SYM)   ? "#$%" : "SYM",
    "BK"
  };
  for (int i = 0; i < 3; i++) {
    int x = i * bw3;
    uint16_t cor = 0x18E3;
    if (hoverBtn == 10 + i) cor = 0x049F;
    if ((i == 0 && kbdLayer == KBD_UPPER) ||
        (i == 1 && kbdLayer == KBD_SYM))
      cor = (hoverBtn == 10 + i) ? 0x049F : 0x28A745;
    canvas.fillRect(x + 2, KBD_BTN1_Y, bw3 - 4, 48, cor);
    canvas.drawRect(x + 2, KBD_BTN1_Y, bw3 - 4, 48, TFT_WHITE);
    canvas.setTextColor(TFT_WHITE);
    canvas.setTextDatum(MC_DATUM);
    canvas.drawString(l1[i], x + bw3 / 2, KBD_BTN1_Y + 24);
  }

  // Botões 2 (CANCEL / OK)
  const int bw2 = SCR_W / 2;
  const char* l2[2] = { "CANCEL", "OK" };
  for (int i = 0; i < 2; i++) {
    int x = i * bw2;
    uint16_t cor = (i == 1) ? 0x006400 : 0x800000;
    if (hoverBtn == 20 + i) cor = (i == 1) ? 0x00A000 : 0xC00000;
    canvas.fillRect(x + 2, KBD_BTN2_Y, bw2 - 4, 48, cor);
    canvas.drawRect(x + 2, KBD_BTN2_Y, bw2 - 4, 48, TFT_WHITE);
    canvas.setTextColor(TFT_WHITE);
    canvas.setTextDatum(MC_DATUM);
    canvas.drawString(l2[i], x + bw2 / 2, KBD_BTN2_Y + 24);
  }
  canvas.setTextDatum(TL_DATUM);
}

static void desenharConectando() {
  canvas.setTextDatum(MC_DATUM);
  canvas.setTextFont(4);
  canvas.setTextColor(TFT_CYAN);
  canvas.drawString("CONECTANDO...", SCR_W / 2, SCR_H / 2 - 20);
  canvas.setTextFont(2);
  canvas.setTextColor(TFT_WHITE);
  canvas.drawString(ssidAlvo, SCR_W / 2, SCR_H / 2 + 20);

  static int ang = 0;
  ang = (ang + 15) % 360;
  float r = ang * DEG_TO_RAD;
  int cx = SCR_W / 2, cy = SCR_H / 2 + 70, rr = 20;
  int x1 = cx + cosf(r) * rr;
  int y1 = cy + sinf(r) * rr;
  canvas.drawCircle(cx, cy, rr, 0x39E7);
  canvas.drawLine(cx, cy, x1, y1, TFT_CYAN);

  canvas.setTextDatum(TL_DATUM);
}

// ===============================================================
void wifi_begin() {
  view = WV_MAIN;
  kbdLayer = KBD_LOWER;
  kbdBuffer = "";
  ssidAlvo = "";
  hoverIdx = hoverKey = hoverBtn = -1;
  uart_send_wifi_query();
}

void wifi_loop(uint32_t now) {
  if (view == WV_CONNECTING) {
    WifiStatusMini st = uart_get_wifi_status();
    if (st.estado == 2 || st.estado == 3) {
      view = WV_MAIN;
      hoverIdx = hoverKey = hoverBtn = -1;
    } else if (now - tConexaoIni > 15000) {
      view = WV_MAIN;
      hoverIdx = hoverKey = hoverBtn = -1;
    }
  }
}

void wifi_end() {}

void wifi_draw() {
  canvas.fillSprite(TFT_BLACK);

  switch (view) {
    case WV_MAIN:       desenharMain();       break;
    case WV_KBD_SSID:
    case WV_KBD_PASS:   desenharTeclado();    break;
    case WV_CONNECTING: desenharConectando(); break;
  }
}

// ===============================================================
void wifi_hover(int x, int y) {
  hoverIdx = hoverKey = hoverBtn = -1;

  if (view == WV_MAIN) {
    const int by = SCR_H - 56;
    if (y >= by && y <= by + 48) {
      int bw = SCR_W / 3;
      hoverBtn = x / bw;
    }
  }
  else if (view == WV_KBD_SSID || view == WV_KBD_PASS) {
    int k = teclaHit(x, y);
    if (k >= 0) { hoverKey = k; return; }
    int b = btnHit(x, y);
    if (b >= 0) { hoverBtn = b; return; }
  }
}

// ===============================================================
void wifi_release(int x, int y) {
  if (view == WV_CONNECTING) return;

  if (view == WV_MAIN) {
    const int by = SCR_H - 56;
    if (y >= by && y <= by + 48) {
      int bw = SCR_W / 3;
      int b = x / bw;
      if (b == 0) {
        // NOVA — entra SSID
        view = WV_KBD_SSID;
        kbdBuffer = "";
        kbdLayer = KBD_LOWER;
        ssidAlvo = "";
      } else if (b == 1) {
        // RETRY — conecta na última salva
        uart_send_wifi_retry();
        ssidAlvo = "(ultima salva)";
        tConexaoIni = millis();
        view = WV_CONNECTING;
      } else {
        mode_set(SCREEN_MENU);
      }
    }
    return;
  }

  if (view == WV_KBD_SSID || view == WV_KBD_PASS) {
    int k = hoverKey;
    if (k >= 0) {
      char c = teclaChar(k);
      if (c && kbdBuffer.length() < 63) kbdBuffer += c;
      hoverKey = -1;
      return;
    }

    int b = hoverBtn;
    if (b == 10) {
      if (kbdLayer == KBD_LOWER) kbdLayer = KBD_UPPER;
      else if (kbdLayer == KBD_UPPER) kbdLayer = KBD_LOWER;
    } else if (b == 11) {
      if (kbdLayer == KBD_SYM) kbdLayer = KBD_LOWER;
      else kbdLayer = KBD_SYM;
    } else if (b == 12) {
      if (kbdBuffer.length() > 0) kbdBuffer.remove(kbdBuffer.length() - 1);
    } else if (b == 20) {
      view = WV_MAIN;
      kbdBuffer = "";
    } else if (b == 21) {
      if (view == WV_KBD_SSID) {
        ssidAlvo = kbdBuffer;
        kbdBuffer = "";
        view = WV_KBD_PASS;
        kbdLayer = KBD_LOWER;
      } else if (view == WV_KBD_PASS) {
        uart_send_wifi_connect(ssidAlvo, kbdBuffer);
        tConexaoIni = millis();
        view = WV_CONNECTING;
      }
    }
    hoverBtn = -1;
    return;
  }
}