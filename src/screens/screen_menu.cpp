#include "src/screens/screen_menu.h"
#include "src/core/mode_manager.h"
#include "src/core/display_manager.h"
#include "src/common/config.h"

#define MENU_COLS      2
#define MENU_ROWS      3
#define MENU_BTN_W     220
#define MENU_BTN_H     80
#define MENU_GAP_X     14
#define MENU_GAP_Y     12
#define MENU_Y0        50

#define GEAR_BTN_SIZE 36
#define GEAR_BTN_X (SCR_W - GEAR_BTN_SIZE - 8)
#define GEAR_BTN_Y 6

static bool hoverGear = false;

static int hoverIdx = -1;

struct MenuItem {
  const char* label;
  ScreenId    destino;
};

static const MenuItem itens[] = {
  { "HOME",    SCREEN_HOME },
  { "OBD",     SCREEN_OBD  },
  { "WI-FI",   SCREEN_WIFI },
  { "DEBUG",   SCREEN_DEBUG },
  { "LOGS",    SCREEN_LOGS },
  { "DESENHO", SCREEN_SCRIBBLE },
};
static const int totalItens = sizeof(itens) / sizeof(itens[0]);

static void itemXY(int i, int& x, int& y) {
  int col = i % MENU_COLS;
  int row = i / MENU_COLS;
  int totalW = MENU_COLS * MENU_BTN_W + (MENU_COLS - 1) * MENU_GAP_X;
  int x0 = (SCR_W - totalW) / 2;
  x = x0 + col * (MENU_BTN_W + MENU_GAP_X);
  y = MENU_Y0 + row * (MENU_BTN_H + MENU_GAP_Y);
}

static int hitTest(int x, int y) {
  for (int i = 0; i < totalItens; i++) {
    int ix, iy;
    itemXY(i, ix, iy);
    if (x >= ix && x <= ix + MENU_BTN_W &&
        y >= iy && y <= iy + MENU_BTN_H) return i;
  }
  return -1;
}

void menu_begin() { hoverIdx = -1; }
void menu_loop(uint32_t now) {}
void menu_end() {}

static void drawGearIcon(int cx, int cy, int r, uint16_t color) {
  canvas.drawCircle(cx, cy, r, color);
  canvas.drawCircle(cx, cy, r - 1, color);
  canvas.fillCircle(cx, cy, r / 3, color);
  for (int i = 0; i < 6; i++) {
    float a = i * 60.0f * DEG_TO_RAD;
    int x1 = cx + cosf(a) * (r + 1);
    int y1 = cy + sinf(a) * (r + 1);
    int x2 = cx + cosf(a) * (r + 5);
    int y2 = cy + sinf(a) * (r + 5);
    canvas.drawLine(x1, y1, x2, y2, color);
    canvas.drawLine(x1+1, y1, x2+1, y2, color);
  }
}

void menu_draw() {
  canvas.setTextDatum(TC_DATUM);
  canvas.setTextColor(TFT_CYAN);
  canvas.setTextFont(4);
  canvas.drawString("MENU", SCR_W / 2, 12);
  canvas.setTextDatum(TL_DATUM);

  for (int i = 0; i < totalItens; i++) {
    int ix, iy;
    itemXY(i, ix, iy);
    uint16_t corFundo = (i == hoverIdx) ? 0x049F : 0x18E3;
    canvas.fillRect(ix, iy, MENU_BTN_W, MENU_BTN_H, corFundo);
    canvas.drawRect(ix, iy, MENU_BTN_W, MENU_BTN_H, TFT_WHITE);

    canvas.setTextColor(TFT_WHITE);
    canvas.setTextFont(4);
    canvas.setTextDatum(MC_DATUM);
    canvas.drawString(itens[i].label, ix + MENU_BTN_W / 2, iy + MENU_BTN_H / 2);
  }

  // Ícone de engrenagem (settings)
  uint16_t gearBg = hoverGear ? 0x049F : 0x18E3;
  canvas.fillRect(GEAR_BTN_X, GEAR_BTN_Y, GEAR_BTN_SIZE, GEAR_BTN_SIZE, gearBg);
  canvas.drawRect(GEAR_BTN_X, GEAR_BTN_Y, GEAR_BTN_SIZE, GEAR_BTN_SIZE, TFT_WHITE);
  drawGearIcon(GEAR_BTN_X + GEAR_BTN_SIZE/2, GEAR_BTN_Y + GEAR_BTN_SIZE/2, 8, TFT_WHITE);

  canvas.setTextDatum(TL_DATUM);
}

static bool gearHit(int x, int y) {
  return x >= GEAR_BTN_X && x <= GEAR_BTN_X + GEAR_BTN_SIZE &&
         y >= GEAR_BTN_Y && y <= GEAR_BTN_Y + GEAR_BTN_SIZE;
}

void menu_hover(int x, int y) {
  if (gearHit(x, y)) { hoverGear = true; hoverIdx = -1; return; }
  hoverGear = false;
  hoverIdx = hitTest(x, y);
}

void menu_release(int x, int y) {
  if (gearHit(x, y)) { mode_set(SCREEN_SETTINGS); return; }
  int idx = hitTest(x, y);
  if (idx >= 0) mode_set(itens[idx].destino);
}