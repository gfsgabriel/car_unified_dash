#include "src/screens/screen_debug.h"
#include "src/screens/screen_base.h"
#include "src/core/mode_manager.h"
#include "src/core/display_manager.h"
#include "src/common/config.h"
#include "src/core/uart_router.h"

// ===============================================================
// Tela de debug serial
// ===============================================================

#define BTN_H      36
#define BTN_GAP     4
#define TOP_H      20
#define BOT_H     100
#define MAX_LINHAS 40

static String linhas[MAX_LINHAS];
static int    qtdLinhas = 0;
static int    hoverBtn = -1;

// ---- Contadores de pacotes (Hz) ----
static uint32_t ultFFT = 0, ultMusic = 0, ultTele = 0, ultObd = 0;
static uint32_t tContadores = 0;
static float    hzFFT = 0, hzMusic = 0, hzTele = 0, hzObd = 0;

struct BtnCmd { const char* label; const char* cmd; };
static BtnCmd botoesTop[] = {
  { "WiFi Query", "QUERY" },
  { "WiFi Retry", "RETRY" },
  { "Btn 3",      "BTN3"  },
  { "Btn 5",      "BTN5"  },
};
static const int N_BTN = 4;

// ===============================================================
static void addLinha(const String& s) {
  if (qtdLinhas >= MAX_LINHAS) {
    for (int i = 0; i < MAX_LINHAS - 1; i++) linhas[i] = linhas[i + 1];
    qtdLinhas = MAX_LINHAS - 1;
  }
  linhas[qtdLinhas++] = s;
}

static void atualizarContadores() {
  uint32_t agora = millis();
  if (tContadores == 0) {
    tContadores = agora;
    ultFFT   = uart_cont_fft();
    ultMusic = uart_cont_music();
    ultTele  = uart_cont_tele();
    ultObd   = uart_cont_obdstatus();
    return;
  }
  if (agora - tContadores < 1000) return;

  uint32_t cFFT   = uart_cont_fft();
  uint32_t cMusic = uart_cont_music();
  uint32_t cTele  = uart_cont_tele();
  uint32_t cObd   = uart_cont_obdstatus();

  uint32_t dt = agora - tContadores;
  hzFFT   = (cFFT   - ultFFT)   * 1000.0f / dt;
  hzMusic = (cMusic - ultMusic) * 1000.0f / dt;
  hzTele  = (cTele  - ultTele)  * 1000.0f / dt;
  hzObd   = (cObd   - ultObd)   * 1000.0f / dt;

  ultFFT = cFFT; ultMusic = cMusic; ultTele = cTele; ultObd = cObd;
  tContadores = agora;
}

// ===============================================================
void debug_begin() {
  qtdLinhas = 0;
  hoverBtn = -1;
  tContadores = 0;
  hzFFT = hzMusic = hzTele = hzObd = 0;
  addLinha("[SYS] Debug mini iniciado");
  canvas.fillSprite(TFT_BLACK);
}

void debug_loop(uint32_t now) {
  String s;
  while (uart_get_debug_linha(s)) {
    addLinha(s);
  }
  atualizarContadores();
}

void debug_end() {}

// ===============================================================
static void desenharTerminal() {
  canvas.fillRect(0, TOP_H, SCR_W, SCR_H - TOP_H - BOT_H - 40, TFT_BLACK);
  canvas.setTextFont(1);
  canvas.setTextSize(1);

  char hdr[80];
  snprintf(hdr, sizeof(hdr), "FFT:%.0f M:%.0f T:%.0f O:%.0f",
           hzFFT, hzMusic, hzTele, hzObd);
  canvas.setTextColor(TFT_YELLOW);
  canvas.setCursor(4, TOP_H + 2);
  canvas.print(hdr);

  const int lineH = 11;
  const int yInicio = TOP_H + 14;
  const int maxLinhas = (SCR_H - yInicio - BOT_H - 40) / lineH;
  int start = (qtdLinhas > maxLinhas) ? qtdLinhas - maxLinhas : 0;

  int y = yInicio;
  for (int i = start; i < qtdLinhas; i++) {
    const String& l = linhas[i];

    uint16_t cor = TFT_WHITE;
    if (l.startsWith("[RX]")) cor = TFT_GREEN;
    else if (l.startsWith("[TX]")) cor = TFT_ORANGE;
    else if (l.startsWith("[SYS]")) cor = TFT_CYAN;
    else if (l.startsWith("[ERR]")) cor = TFT_RED;

    canvas.setTextColor(cor);
    canvas.setCursor(4, y);
    canvas.print(l);
    y += lineH;
  }
}

// ===============================================================
static void desenharBotoes() {
  const int x0 = 0;
  const int y0 = SCR_H - BOT_H;
  const int btnW = SCR_W / N_BTN - BTN_GAP;

  for (int i = 0; i < N_BTN; i++) {
    int bx = x0 + i * (btnW + BTN_GAP) + BTN_GAP / 2;
    uint16_t cor = (hoverBtn == i) ? 0x049F : 0x18E3;
    canvas.fillRect(bx, y0 + 10, btnW, BTN_H, cor);
    canvas.drawRect(bx, y0 + 10, btnW, BTN_H, TFT_WHITE);
    canvas.setTextColor(TFT_WHITE);
    canvas.setTextDatum(MC_DATUM);
    canvas.setCursor(bx + btnW / 2, y0 + 10 + BTN_H / 2);
    canvas.print(botoesTop[i].label);
    canvas.setTextDatum(TL_DATUM);
  }
}

// ===============================================================
void debug_draw() {
  canvas.fillSprite(TFT_BLACK);

  canvas.fillRect(0, 0, SCR_W, TOP_H, 0x18E3);
  canvas.setTextColor(TFT_CYAN);
  canvas.setTextFont(2);
  canvas.setCursor(6, 3);
  canvas.print("DEBUG SERIAL");

  WifiStatusMini ws = uart_get_wifi_status();
  if (ws.valido) {
    canvas.setTextFont(1);
    canvas.setTextColor(ws.estado == 2 ? TFT_GREEN : TFT_ORANGE);
    canvas.setCursor(SCR_W - 180, 6);
    canvas.printf("%s %s", ws.ssid, ws.ip);
  }

  desenharTerminal();
  desenharBotoes();

  canvas.setTextFont(1);
  canvas.setTextColor(TFT_DARKGREY);
  canvas.setCursor(4, SCR_H - 4);
  canvas.print("toque = menu");
}

// ===============================================================
void debug_hover(int x, int y) {
  hoverBtn = -1;
  if (y < SCR_H - BOT_H + 10) return;

  const int btnW = SCR_W / N_BTN - BTN_GAP;
  for (int i = 0; i < N_BTN; i++) {
    int bx = i * (btnW + BTN_GAP) + BTN_GAP / 2;
    if (x >= bx && x <= bx + btnW) { hoverBtn = i; return; }
  }
}

void debug_release(int x, int y) {
  if (hoverBtn >= 0) {
    const char* cmd = botoesTop[hoverBtn].cmd;
    if (strcmp(cmd, "QUERY") == 0) {
      uart_send_wifi_query();
      addLinha("[TX] WiFi Query");
    } else if (strcmp(cmd, "RETRY") == 0) {
      uart_send_wifi_retry();
      addLinha("[TX] WiFi Retry");
    } else if (strcmp(cmd, "BTN3") == 0) {
      uart_send_btn(3, 1);
      addLinha("[TX] Btn 3");
    } else if (strcmp(cmd, "BTN5") == 0) {
      uart_send_btn(5, 1);
      addLinha("[TX] Btn 5");
    }
    hoverBtn = -1;
    return;
  }

  mode_set(SCREEN_MENU);
}