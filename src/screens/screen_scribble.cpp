#include "src/screens/screen_scribble.h"   // ← mudou
#include "src/core/mode_manager.h"
#include "src/core/display_manager.h"
#include "src/common/config.h"

// ──── SCRIBBLE — easter egg ────
// Por enquanto: placeholder. Reusa a mesma estrutura do código de referência.

static int  corAtual = 0;
static bool modoBorracha = false;
static int  hoverBtn = -1;
static TouchPoint anterior = { 0, 0, false };
static bool primeiraVez = true;

#define COR_W        45
#define N_CORES      8
#define BOR_X        360
#define BOR_W        50
#define HOME_X       0
#define HOME_W       50
#define CLEAR_X      410
#define CLEAR_W      70

static uint16_t paleta[N_CORES] = {
  TFT_RED, TFT_GREEN, TFT_BLUE, TFT_YELLOW,
  TFT_CYAN, TFT_MAGENTA, TFT_WHITE, TFT_ORANGE
};

// Mapa temporário do desenho (RAM, some ao sair)
// TODO: usar sprite dedicado persistente

void scribble_begin() {
  if (primeiraVez) {
    canvas.fillRect(0, BARRA_H, SCR_W, SCR_H - BARRA_H, TFT_BLACK);
    primeiraVez = false;
  }
}

void scribble_loop(uint32_t now) {}

void scribble_end() {}

void scribble_draw() {
  // Barra superior
  for (int i = 0; i < N_CORES; i++) {
    canvas.fillRect(HOME_W + i * COR_W, 0, COR_W - 1, BARRA_H, paleta[i]);
  }
  // HOME
  canvas.fillRect(HOME_X, 0, HOME_W, BARRA_H, 0x0400);
  canvas.drawRect(HOME_X, 0, HOME_W, BARRA_H, TFT_BLACK);
  canvas.setTextColor(TFT_WHITE);
  canvas.setTextDatum(MC_DATUM);
  canvas.setTextSize(2);
  canvas.drawString("H", HOME_X + HOME_W / 2, BARRA_H / 2);
  // BOR
  canvas.fillRect(BOR_X, 0, BOR_W, BARRA_H, modoBorracha ? TFT_LIGHTGREY : TFT_DARKGREY);
  canvas.drawRect(BOR_X, 0, BOR_W, BARRA_H, TFT_BLACK);
  canvas.setTextColor(TFT_BLACK);
  canvas.drawString("BOR", BOR_X + BOR_W / 2, BARRA_H / 2);
  // CLEAR
  canvas.fillRect(CLEAR_X, 0, CLEAR_W, BARRA_H, TFT_DARKGREY);
  canvas.drawRect(CLEAR_X, 0, CLEAR_W, BARRA_H, TFT_BLACK);
  canvas.setTextColor(TFT_WHITE);
  canvas.drawString("CLEAR", CLEAR_X + CLEAR_W / 2, BARRA_H / 2);
  canvas.setTextDatum(TL_DATUM);
}

void scribble_hover(int x, int y) {
  if (y < BARRA_H) return;   // ignora barra no hover

  if (modoBorracha) {
    canvas.fillCircle(x, y, 20, TFT_BLACK);
    if (anterior.pressed && anterior.y >= BARRA_H) {
      canvas.drawWideLine(anterior.x, anterior.y, x, y, 40, TFT_BLACK, TFT_BLACK);
    }
  } else {
    uint16_t cor = paleta[corAtual];
    if (anterior.pressed && anterior.y >= BARRA_H) {
      canvas.drawLine(anterior.x, anterior.y, x, y, cor);
    }
  }
  anterior = { (uint16_t)x, (uint16_t)y, true };
}

void scribble_release(int x, int y) {
  anterior.pressed = false;

  if (y >= BARRA_H) return;

  if (x >= HOME_X && x <= HOME_X + HOME_W) {
    mode_set(SCREEN_HOME);
  } else if (x >= HOME_W && x < BOR_X) {
    corAtual = (x - HOME_W) / COR_W;
    modoBorracha = false;
  } else if (x >= BOR_X && x < CLEAR_X) {
    modoBorracha = true;
  } else if (x >= CLEAR_X) {
    canvas.fillRect(0, BARRA_H, SCR_W, SCR_H - BARRA_H, TFT_BLACK);
  }
}