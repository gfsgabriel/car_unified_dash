#ifndef WIDGETS_BAR_H
#define WIDGETS_BAR_H

#include <Arduino.h>
#include <TFT_eSPI.h>

// Barra horizontal com borda e fill proporcional
void drawBarH(TFT_eSprite& c, int x, int y, int w, int h,
              float valor, float vMin, float vMax,
              uint16_t corFill, uint16_t corFundo, uint16_t corBorda);

// Barra horizontal com ranges de cor (vermelho acima do redline)
void drawBarHRanges(TFT_eSprite& c, int x, int y, int w, int h,
                    float valor, float vMin, float vMax, float vRedline,
                    uint16_t corNormal, uint16_t corRed,
                    uint16_t corFundo, uint16_t corBorda);

#endif