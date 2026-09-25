#ifndef WIDGETS_GAUGE_H
#define WIDGETS_GAUGE_H

#include <Arduino.h>
#include <TFT_eSPI.h>

struct GaugeCfg {
  int cx, cy, raio;
  int strokeWidth;
  int subOffset, subThickness;

  float vMin, vMax;
  float inicioArco, fimArco;

  uint16_t corTrilho, corSub, corPonteiro, corLabel;

  float innerFactor, outerFactor;
  int   pointerWidth;

  int numTicks, tickWidth;

  const char* labelCentral;
  const char* valorMin;
  const char* valorMax;

  int   labelFont, centralFont;
  float labelPct, centralPct;

  // Deslocamento horizontal extra dos labels min/max
  // Útil pra compensar quando um texto é mais curto que o outro
  int labelMinXOffset;
  int labelMaxXOffset;
};

struct GaugeCache {
  TFT_eSprite* sprite = nullptr;
  int offX = 0, offY = 0;
  int w = 0, h = 0;
  bool valid = false;
};

void gaugeBuildBg(GaugeCache& cache, const GaugeCfg& cfg);
void gaugeFreeBg(GaugeCache& cache);

void drawGaugeCircular(TFT_eSprite& canvas, GaugeCache& cache,
                       const GaugeCfg& cfg, float valor);

#endif