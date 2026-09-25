#include "src/screens/screen_logs.h"       // ← mudou
#include "src/core/mode_manager.h"
#include "src/core/display_manager.h"
#include "src/common/boot_log.h"
#include "src/common/config.h"

// Offset de scroll (em linhas)
static int scrollOffset = 0;
static int totalLinhas  = 0;

// Botão HOME no topo
#define HOME_W   90
#define HOME_H   24
#define HOME_X   (480 - HOME_W - 4)

static int yToqueInicial = 0;
static int scrollAoIniciar = 0;
static bool arrastando = false;

void logs_begin() {
  scrollOffset = 0;
  totalLinhas  = boot_log_count();
  arrastando = false;
}

void logs_loop(uint32_t now) {}

void logs_end() {}

void logs_draw() {
  canvas.fillSprite(TFT_BLACK);

  // Header
  canvas.fillRect(0, 0, SCR_W, HOME_H + 6, 0x18E3);
  canvas.setTextFont(2);          // ← 16px sans-serif
  canvas.setTextSize(1);
  canvas.setTextColor(TFT_CYAN);
  canvas.setTextDatum(ML_DATUM);
  canvas.drawString("LOGS", 8, (HOME_H + 6) / 2);

  // Botão HOME
  canvas.fillRect(HOME_X, 3, HOME_W, HOME_H, 0x0400);
  canvas.drawRect(HOME_X, 3, HOME_W, HOME_H, TFT_WHITE);
  canvas.setTextFont(2);          // ← 16px
  canvas.setTextColor(TFT_WHITE);
  canvas.setTextDatum(MC_DATUM);
  canvas.drawString("HOME", HOME_X + HOME_W / 2, 3 + HOME_H / 2);
  canvas.setTextDatum(TL_DATUM);

  // Lista de logs
  const int yTopo       = HOME_H + 14;   // ← era +10, +14 dá mais espaço
  const int alturaLinha = 18;             // ← era 14, agora 18 (linhas respiram)
  const int linhasVisiveis = (SCR_H - yTopo) / alturaLinha;

  canvas.setTextFont(1);          // ← 8px monospace nativo (Font0 antigo)
  canvas.setTextSize(1);

  for (int i = 0; i < linhasVisiveis; i++) {
    int idx = scrollOffset + i;
    if (idx >= totalLinhas) break;

    LogTipo  tipo;
    String   texto;
    uint32_t ms;
    if (!boot_log_get(idx, tipo, texto, ms)) continue;

    int y = yTopo + i * alturaLinha;

    canvas.setTextColor(TFT_DARKGREY);
    canvas.setCursor(6, y);
    canvas.printf("[%5lu]", ms);

    uint16_t cor = TFT_WHITE;
    switch (tipo) {
      case LogTipo::OK:      cor = TFT_GREEN;     break;
      case LogTipo::WARN:    cor = TFT_YELLOW;    break;
      case LogTipo::ERR:     cor = TFT_RED;       break;
      case LogTipo::TITULO:  cor = TFT_CYAN;      break;
      default: break;
    }
    canvas.setTextColor(cor);
    canvas.setCursor(80, y);
    canvas.print(texto);
  }

  // Indicador de scroll
  canvas.setTextFont(1);
  canvas.setTextColor(TFT_DARKGREY);
  canvas.setTextDatum(BR_DATUM);
  canvas.setCursor(SCR_W - 6, SCR_H - 4);
  canvas.printf("%d/%d", scrollOffset + linhasVisiveis, totalLinhas);
  canvas.setTextDatum(TL_DATUM);
}

void logs_hover(int x, int y) {
  // Detecta início de arrasto na área da lista
  if (!arrastando && y > HOME_H + 10) {
    arrastando = true;
    yToqueInicial = y;
    scrollAoIniciar = scrollOffset;
  }
}

void logs_release(int x, int y) {
  // Tap no HOME?
  if (x >= HOME_X && x <= HOME_X + HOME_W &&
      y >= 3 && y <= 3 + HOME_H) {
    mode_set(SCREEN_HOME);
    arrastando = false;
    return;
  }

  // Arrasto terminou
  arrastando = false;
}