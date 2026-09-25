#include "src/widgets/widgets_gauge.h"
#include "src/core/display_manager.h"

// ===============================================================
// Helpers
// ===============================================================
static void pontoAngulo(int cx, int cy, int r, float angDeg, int& x, int& y) {
  float rad = angDeg * DEG_TO_RAD;
  x = cx + (int)(cosf(rad) * r);
  y = cy + (int)(sinf(rad) * r);
}

static float sweepAngle(float start, float end) {
  float s = end - start;
  while (s < 0) s += 360.0f;
  return s;
}

static void drawStringBold(TFT_eSprite& spr, const char* txt, int x, int y) {
  spr.drawString(txt, x, y);
  spr.drawString(txt, x + 1, y);
}

// ===============================================================
// Tick radial (perpendicular à tangente, com espessura)
// ===============================================================
static void drawTickRadial(TFT_eSprite& c, int cx, int cy,
                           int rIn, int rOut, float angDeg,
                           int width, uint16_t cor) {
  float rad = angDeg * DEG_TO_RAD;
  float cosA = cosf(rad);
  float sinA = sinf(rad);
  float perpX = -sinA;
  float perpY =  cosA;

  int halfW = width / 2;
  for (int i = -halfW; i <= halfW; i++) {
    int x1 = cx + (int)(cosA * rIn  + perpX * i);
    int y1 = cy + (int)(sinA * rIn  + perpY * i);
    int x2 = cx + (int)(cosA * rOut + perpX * i);
    int y2 = cy + (int)(sinA * rOut + perpY * i);
    c.drawLine(x1, y1, x2, y2, cor);
  }
}

// ===============================================================
// Constrói o background (com clipping defensivo + corte 2px acima do centro)
// ===============================================================
void gaugeBuildBg(GaugeCache& cache, const GaugeCfg& cfg) {
  const int R = cfg.raio;
  const int rInner = R - cfg.strokeWidth;
  const int subROuter = R + cfg.subOffset + cfg.subThickness;
  const int subRInner = R + cfg.subOffset;

  const int margin = 4;
  const int cutAboveCenter = 2;   // sprite termina 2px acima do centro

  int idealX = cfg.cx - subROuter - margin;
  int idealY = cfg.cy - subROuter - margin;
  int idealW = 2 * subROuter + 2 * margin;
  int idealH = subROuter + margin - cutAboveCenter;

  // Clip: nunca ultrapassa o canvas
  const int canvasW = canvas.width();
  const int canvasH = canvas.height();

  cache.offX = idealX;
  cache.offY = idealY;
  cache.w    = idealW;
  cache.h    = idealH;

  if (cache.offX < 0) { cache.w += cache.offX; cache.offX = 0; }
  if (cache.offX + cache.w > canvasW) cache.w = canvasW - cache.offX;
  if (cache.offY < 0) { cache.h += cache.offY; cache.offY = 0; }
  if (cache.offY + cache.h > canvasH) cache.h = canvasH - cache.offY;

  if (cache.w <= 0 || cache.h <= 0) {
    Serial.printf("gaugeBuildBg: dimensoes invalidas (%dx%d)\n", cache.w, cache.h);
    cache.valid = false;
    return;
  }

  if (!cache.sprite) cache.sprite = new TFT_eSprite(&tft_qspi);
  cache.sprite->setColorDepth(16);
  cache.sprite->createSprite(cache.w, cache.h);
  cache.sprite->setSwapBytes(true);
  cache.sprite->fillSprite(TFT_BLACK);

  const int lcx = cfg.cx - cache.offX;
  const int lcy = cfg.cy - cache.offY;   // 2px abaixo do fundo do sprite

  // ---- 4 círculos formam os 2 anéis ----
  cache.sprite->fillCircle(lcx, lcy, subROuter,  cfg.corSub);
  cache.sprite->fillCircle(lcx, lcy, subRInner,  TFT_BLACK);
  cache.sprite->fillCircle(lcx, lcy, R,          cfg.corTrilho);
  cache.sprite->fillCircle(lcx, lcy, rInner,     TFT_BLACK);

  // ---- Máscara inferior: retângulo abaixo do centro ----
  if (lcy < cache.h) {
    cache.sprite->fillRect(0, lcy, cache.w, cache.h - lcy, TFT_BLACK);
  }

  // ---- 2 triângulos em 30° ----
  {
    const int big = 1000;
    const float tan30 = 0.57735f;
    cache.sprite->fillTriangle(lcx, lcy,
      lcx - big, lcy, lcx - big, lcy - (int)(big * tan30), TFT_BLACK);
    cache.sprite->fillTriangle(lcx, lcy,
      lcx + big, lcy, lcx + big, lcy - (int)(big * tan30), TFT_BLACK);
  }

  // ---- Ticks ----
  float sweep = sweepAngle(cfg.inicioArco, cfg.fimArco);
  for (int i = 0; i < cfg.numTicks; i++) {
    float p = (cfg.numTicks > 1) ? ((float)i / (cfg.numTicks - 1)) : 0.5f;
    float ang = cfg.inicioArco + sweep * p;
    drawTickRadial(*cache.sprite, lcx, lcy, rInner - 1, R + 1,
                   ang, cfg.tickWidth, TFT_BLACK);
  }

  // ---- Labels ----
  cache.sprite->setTextDatum(MC_DATUM);
  cache.sprite->setTextColor(cfg.corLabel);
  cache.sprite->setTextSize(1);

  const bool grande = (R > 70);
  const int labelYOffset = grande ? 16 : 10;

  if (cfg.valorMin) {
    cache.sprite->setTextFont(cfg.labelFont);
    int x, y;
    pontoAngulo(lcx, lcy, (int)(R * cfg.labelPct), cfg.inicioArco, x, y);
    drawStringBold(*cache.sprite, cfg.valorMin,
                   x + cfg.labelMinXOffset, y + labelYOffset);
  }
  if (cfg.valorMax) {
    cache.sprite->setTextFont(cfg.labelFont);
    int x, y;
    pontoAngulo(lcx, lcy, (int)(R * cfg.labelPct), cfg.fimArco, x, y);
    drawStringBold(*cache.sprite, cfg.valorMax,
                   x + cfg.labelMaxXOffset, y + labelYOffset);
  }
  if (cfg.labelCentral) {
    cache.sprite->setTextFont(cfg.centralFont);
    float angMid = cfg.inicioArco + sweep * 0.5f;
    int x, y;
    pontoAngulo(lcx, lcy, (int)(R * cfg.centralPct), angMid, x, y);
    drawStringBold(*cache.sprite, cfg.labelCentral, x, y);
  }

  cache.sprite->setTextDatum(TL_DATUM);
  cache.valid = true;
}

void gaugeFreeBg(GaugeCache& cache) {
  if (cache.sprite) {
    cache.sprite->deleteSprite();
    delete cache.sprite;
    cache.sprite = nullptr;
  }
  cache.valid = false;
}

// ===============================================================
// Draw (bg + ponteiro triangular, SEM hub central)
// ===============================================================
void drawGaugeCircular(TFT_eSprite& canvas, GaugeCache& cache,
                       const GaugeCfg& cfg, float valor) {
  if (!cache.valid) return;
  if (!cache.sprite) return;

  if (cache.offX < 0 || cache.offY < 0) return;
  if (cache.offX + cache.w > canvas.width())  return;
  if (cache.offY + cache.h > canvas.height()) return;

  cache.sprite->pushToSprite(&canvas, cache.offX, cache.offY);

  if (valor < cfg.vMin) valor = cfg.vMin;
  if (valor > cfg.vMax) valor = cfg.vMax;
  float pct = (valor - cfg.vMin) / (cfg.vMax - cfg.vMin);

  const int R = cfg.raio;
  float sweep = sweepAngle(cfg.inicioArco, cfg.fimArco);
  float angP = cfg.inicioArco + sweep * pct;

  float rad   = angP * DEG_TO_RAD;
  float cosA  = cosf(rad);
  float sinA  = sinf(rad);
  float perpX = -sinA;
  float perpY =  cosA;

  // Ponteiro triangular
  int tipX = cfg.cx + (int)(cosA * R * cfg.outerFactor);
  int tipY = cfg.cy + (int)(sinA * R * cfg.outerFactor);

  int rIn  = (int)(R * cfg.innerFactor);
  int baseCX = cfg.cx + (int)(cosA * rIn);
  int baseCY = cfg.cy + (int)(sinA * rIn);
  int halfW = cfg.pointerWidth / 2;

  int bx1 = baseCX + (int)(perpX * halfW);
  int by1 = baseCY + (int)(perpY * halfW);
  int bx2 = baseCX - (int)(perpX * halfW);
  int by2 = baseCY - (int)(perpY * halfW);

  canvas.fillTriangle(tipX, tipY, bx1, by1, bx2, by2, cfg.corPonteiro);
}