#include "src/widgets/widgets_fft.h"

// ===============================================================
// FFT bars — recebe 9, mostra 8 (8k+16k combinadas) com labels
// ===============================================================
void drawFFTBars(TFT_eSprite& c, int x, int y, int w, int h,
                 const uint8_t* bins9, int numBins9,
                 uint16_t corBarra, uint16_t corFundo) {
  c.fillRect(x, y, w, h, corFundo);

  const int NB = 8;   // 8 barras exibidas

  // Combina 9 → 8
  uint8_t bins[8];
  for (int i = 0; i < 7; i++) {
    bins[i] = (i < numBins9) ? bins9[i] : 0;
  }
  {
    int s = 0;
    if (numBins9 > 7) s += bins9[7];
    if (numBins9 > 8) s += bins9[8];
    bins[7] = (s > 255) ? 255 : (uint8_t)s;
  }

  // Layout
  const int labelH = 12;              // altura reservada pros labels
  const int barAreaH = h - labelH;
  const int gap = 2;
  const int bw = (w - gap * (NB - 1)) / NB;
  if (bw < 1) return;

  const char* labels[8] = { "60", "120", "240", "480", "960", "2k", "4k", "8k" };

  // Desenha barras
  for (int i = 0; i < NB; i++) {
    int bx = x + i * (bw + gap);
    int bh = (bins[i] * (barAreaH - 2)) / 255;
    if (bh < 1 && bins[i] > 0) bh = 1;

    if (bh > 0) {
      c.fillRect(bx, y + barAreaH - bh, bw, bh, corBarra);
    }

    // Label da frequência
    c.setTextDatum(TC_DATUM);
    c.setTextColor(TFT_DARKGREY);
    c.setTextFont(1);
    c.setTextSize(1);
    c.drawString(labels[i], bx + bw / 2, y + barAreaH + 2);
  }
  c.setTextDatum(TL_DATUM);
}

// ===============================================================
// Waveform: FÓRMULA ORIGINAL do car_display_module.ino
// ===============================================================
void drawWaveformFromFFT(TFT_eSprite& c, int x, int y, int w, int h,
                         const uint8_t* bins9, int numBins9,
                         uint16_t corLinha, uint16_t corFundo) {
  c.fillRect(x, y, w, h, corFundo);

  if (numBins9 <= 0) return;

  // Fase estática (avança a cada chamada)
  static float fase = 0.0f;
  fase += 0.15f;
  if (fase > TWO_PI) fase -= TWO_PI;

  // Ganho EXATO do original (9 bandas)
  const float ganhoBanda[9] = {
    14.0f, 12.0f, 10.0f, 8.0f, 6.5f, 5.0f, 3.8f, 2.5f, 1.2f
  };

  const int NUM_PONTOS = 90;         // mesmo do original
  const int yCentro = y + h / 2;
  const int alturaMax = (h / 2) - 2;

  int prevX = x;
  int prevY = yCentro;

  for (int i = 0; i < NUM_PONTOS; i++) {
    float xNorm = (float)i / (NUM_PONTOS - 1);
    float somaSenoides = 0.0f;

    int bandas = (numBins9 < 9) ? numBins9 : 9;

    for (int b = 0; b < bandas; b++) {
      float freqHarmonica = (float)(1 << b);
      float direcaoFase = (b % 2 == 0) ? 1.0f : -1.0f;
      float angulo = (xNorm * TWO_PI * freqHarmonica)
                     + (fase * direcaoFase * (1.0f + b * 0.2f));

      float f = (float)bins9[b] / 255.0f;
      somaSenoides += sinf(angulo) * (f * ganhoBanda[b]);
    }

    // Constrain EXATO do original
    if (somaSenoides >  (float)alturaMax) somaSenoides =  (float)alturaMax;
    if (somaSenoides < -(float)alturaMax) somaSenoides = -(float)alturaMax;

    int px = x + (i * (w - 1)) / (NUM_PONTOS - 1);
    int py = yCentro - (int)somaSenoides;

    if (py < y + 1)     py = y + 1;
    if (py > y + h - 2) py = y + h - 2;

    if (i > 0) {
      c.drawLine(prevX, prevY, px, py, corLinha);
    }
    prevX = px;
    prevY = py;
  }
}