#include "src/widgets/widgets_bar.h"

void drawBarH(TFT_eSprite& c, int x, int y, int w, int h,
              float valor, float vMin, float vMax,
              uint16_t corFill, uint16_t corFundo, uint16_t corBorda) {
  // Fundo + borda
  c.fillRect(x, y, w, h, corFundo);
  c.drawRect(x, y, w, h, corBorda);

  // Clamp
  if (valor < vMin) valor = vMin;
  if (valor > vMax) valor = vMax;
  float pct = (valor - vMin) / (vMax - vMin);

  int fillW = (int)((w - 2) * pct);
  if (fillW > 0) {
    c.fillRect(x + 1, y + 1, fillW, h - 2, corFill);
  }
}

void drawBarHRanges(TFT_eSprite& c, int x, int y, int w, int h,
                    float valor, float vMin, float vMax, float vRedline,
                    uint16_t corNormal, uint16_t corRed,
                    uint16_t corFundo, uint16_t corBorda) {
  c.fillRect(x, y, w, h, corFundo);
  c.drawRect(x, y, w, h, corBorda);

  if (valor < vMin) valor = vMin;
  if (valor > vMax) valor = vMax;

  // Posição do redline em pixels
  int redPx = (int)((w - 2) * ((vRedline - vMin) / (vMax - vMin)));

  // Preenche até o valor
  int fillW = (int)((w - 2) * ((valor - vMin) / (vMax - vMin)));

  // Parte verde (até redline ou até fillW)
  int verdeW = (fillW < redPx) ? fillW : redPx;
  if (verdeW > 0) {
    c.fillRect(x + 1, y + 1, verdeW, h - 2, corNormal);
  }

  // Parte vermelha (acima do redline)
  if (fillW > redPx) {
    c.fillRect(x + 1 + redPx, y + 1, fillW - redPx, h - 2, corRed);
  }
}