#include "src/screens/screen_obd.h"
#include "src/screens/screen_base.h"
#include "src/core/mode_manager.h"
#include "src/core/display_manager.h"
#include "src/core/uart_router.h"
#include "src/common/config.h"

#define BTN_H 44
#define BTN_Y (SCR_H - BTN_H - 8)
#define BTN_W 200
#define BTN_GAP 12

static int hoverBtn = -1;

void obd_begin() { hoverBtn = -1; }
void obd_loop(uint32_t now) {}
void obd_end() {}

void obd_draw() {
  canvas.fillSprite(TFT_BLACK);

  ObdStatusGeral st = uart_get_obd_status();

  // Header
  canvas.fillRect(0, 0, SCR_W, 30, 0x18E3);
  canvas.setTextFont(2);
  canvas.setTextColor(TFT_CYAN);
  canvas.setCursor(8, 7);
  canvas.print("OBD / PIDs");

  // Summary
  canvas.setTextFont(2);
  canvas.setTextColor(st.elmResponde ? TFT_GREEN : TFT_RED);
  canvas.setCursor(8, 38);
  canvas.printf("ELM: %s", st.elmResponde ? "OK" : "OFF");

  uint16_t corGeral;
  switch (st.estadoGeral) {
    case 0:  corGeral = TFT_GREEN;  break;
    case 1:  corGeral = TFT_YELLOW; break;
    default: corGeral = TFT_RED;    break;
  }
  canvas.setTextColor(corGeral);
  canvas.setCursor(130, 38);
  canvas.printf("PIDs: %d/%d", st.respondendo, st.total);

  canvas.drawFastHLine(0, 60, SCR_W, 0x39E7);

  // Lista
  const int y0 = 65;
  const int rowH = 22;
  int maxRows = (BTN_Y - y0 - 4) / rowH;
  if (maxRows > 10) maxRows = 10;

  canvas.setTextFont(2);
  for (int i = 0; i < st.total && i < maxRows; i++) {
    ObdPidStatus& p = st.pids[i];
    int y = y0 + i * rowH;

    uint16_t cor;
    if (!p.suportado)        cor = TFT_RED;
    else if (!p.respondendo) cor = TFT_ORANGE;
    else                     cor = TFT_GREEN;

    canvas.fillCircle(15, y + 9, 6, cor);
    canvas.drawCircle(15, y + 9, 7, TFT_WHITE);

    canvas.setTextColor(TFT_WHITE);
    canvas.setCursor(30, y + 2);
    canvas.printf("%s  %s", p.cmd, p.nome);

    canvas.setTextColor(cor);
    canvas.setCursor(SCR_W - 110, y + 2);
    canvas.printf("%8.1f", p.valor);
  }

  // Botões
  int bx0 = (SCR_W - BTN_W * 2 - BTN_GAP) / 2;
  int bx1 = bx0 + BTN_W + BTN_GAP;

  uint16_t c0 = (hoverBtn == 0) ? 0x049F : 0x18E3;
  canvas.fillRect(bx0, BTN_Y, BTN_W, BTN_H, c0);
  canvas.drawRect(bx0, BTN_Y, BTN_W, BTN_H, TFT_WHITE);
  canvas.setTextColor(TFT_WHITE);
  canvas.setTextDatum(MC_DATUM);
  canvas.drawString("FORCAR RENEG", bx0 + BTN_W / 2, BTN_Y + BTN_H / 2);

  uint16_t c1 = (hoverBtn == 1) ? 0xC000 : 0x8000;
  canvas.fillRect(bx1, BTN_Y, BTN_W, BTN_H, c1);
  canvas.drawRect(bx1, BTN_Y, BTN_W, BTN_H, TFT_WHITE);
  canvas.setTextColor(TFT_WHITE);
  canvas.drawString("VOLTAR", bx1 + BTN_W / 2, BTN_Y + BTN_H / 2);
  canvas.setTextDatum(TL_DATUM);
}

void obd_hover(int x, int y) {
  hoverBtn = -1;
  if (y < BTN_Y || y > BTN_Y + BTN_H) return;
  int bx0 = (SCR_W - BTN_W * 2 - BTN_GAP) / 2;
  int bx1 = bx0 + BTN_W + BTN_GAP;
  if (x >= bx0 && x <= bx0 + BTN_W) hoverBtn = 0;
  else if (x >= bx1 && x <= bx1 + BTN_W) hoverBtn = 1;
}

void obd_release(int x, int y) {
  if (hoverBtn == 0) uart_send_obd_reob();
  else if (hoverBtn == 1) mode_set(SCREEN_MENU);
  hoverBtn = -1;
}