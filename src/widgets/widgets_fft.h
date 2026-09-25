#ifndef WIDGETS_FFT_H
#define WIDGETS_FFT_H

#include <Arduino.h>
#include <TFT_eSPI.h>

// Recebe 9 bins. Combina os 2 últimos (8k+16k) e desenha 8 barras
// com labels de frequência embaixo.
void drawFFTBars(TFT_eSprite& c, int x, int y, int w, int h,
                 const uint8_t* bins9, int numBins9,
                 uint16_t corBarra, uint16_t corFundo);

// Waveform: usa as 9 bandas (fórmula original do car_display_module.ino)
void drawWaveformFromFFT(TFT_eSprite& c, int x, int y, int w, int h,
                         const uint8_t* bins9, int numBins9,
                         uint16_t corLinha, uint16_t corFundo);

#endif