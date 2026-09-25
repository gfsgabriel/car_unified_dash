#include "src/screens/screen_loading.h"    // ← mudou
#include "src/screens/screen_base.h"
#include "src/core/mode_manager.h"
#include "src/core/display_manager.h"
#include "src/common/boot_log.h"

static int  mostradas = 0;
static uint32_t ultimaAdicao = 0;
static uint32_t tFim = 0;
static bool  finalizando = false;

void loading_begin() {
  mostradas = 0;
  ultimaAdicao = millis();
  tFim = 0;
  finalizando = false;
  canvas.fillSprite(TFT_BLACK);
}

void loading_loop(uint32_t now) {
  int total = boot_log_count();

  // Ainda tem linhas pra mostrar (50ms entre linhas)
  if (mostradas < total) {
    if (now - ultimaAdicao >= 50) {
      mostradas++;
      ultimaAdicao = now;
    }
    return;
  }

  // Terminou de mostrar tudo — espera 800ms e vai pra home
  if (!finalizando) {
    finalizando = true;
    tFim = now;
    return;
  }

  if (now - tFim >= 800) {
    mode_set(SCREEN_HOME);
  }
}

void loading_end() {}

void loading_draw() {
  canvas.fillSprite(TFT_BLACK);
  canvas.setTextDatum(TL_DATUM);
  canvas.setTextFont(1);
  canvas.setTextSize(1);

  int total = boot_log_count();
  int limite = min(mostradas, total);

  // Mostra só as últimas N linhas que cabem na tela
  const int linhas = SCR_H / 14;
  int inicio = max(0, limite - linhas);

  for (int i = inicio; i < limite; i++) {
    LogTipo  tipo;
    String   texto;
    uint32_t ms;
    if (!boot_log_get(i, tipo, texto, ms)) continue;

    int y = (i - inicio) * 14 + 4;

    // Timestamp
    canvas.setTextColor(TFT_DARKGREY);
    canvas.setCursor(6, y);
    canvas.printf("[%5lu]", ms);

    // Texto
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
}

void loading_hover(int x, int y) {}
void loading_release(int x, int y) {}