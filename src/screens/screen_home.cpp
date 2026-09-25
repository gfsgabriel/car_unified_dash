#include "src/screens/screen_home.h"
#include "src/screens/screen_base.h"
#include "src/core/mode_manager.h"
#include "src/core/display_manager.h"
#include "src/core/uart_router.h"
#include "src/common/config.h"
#include "src/common/telemetria.h"
#include "src/common/audio.h"
#include "src/common/estado.h"
#include "src/widgets/widgets_gauge.h"
#include "src/widgets/widgets_fft.h"

static uint32_t tInicio = 0;
static bool     backgroundsProntos = false;

static int  hoverBtnMedia  = -1;
static bool hoverHamburger = false;

// ===============================================================
// Animações
// ===============================================================
static float rpmShown   = 0.0f;
static float boostShown = -15.0f;
static float consShown  = 0.0f;
static float tpsShown   = 0.0f;
static float pedalShown = 0.0f;
static float rpmPrev    = 0.0f,   rpmTarget    = 0.0f;
static float boostPrev  = -15.0f, boostTarget  = -15.0f;
static float consPrev   = 0.0f,   consTarget   = 0.0f;
static float tpsPrev    = 0.0f,   tpsTarget    = 0.0f;
static float pedalPrev  = 0.0f,   pedalTarget  = 0.0f;
static unsigned long tInicioAnim = 0;

// ★ Máximo de consumo local (só nesta tela, reseta ao entrar)
static float consumoMaxShown = 0.0f;

static GaugeCfg   cfgBoostSmall;
static GaugeCfg   cfgConsumSmall;
static GaugeCache cacheBoostSmall;
static GaugeCache cacheConsumSmall;

// ===============================================================
// Cores do ponteiro
// ===============================================================
static uint16_t corPonteiroBoost(float b) {
  if (b > 15.0f) return TFT_RED;
  if (b > 10.0f) return TFT_ORANGE;
  return TFT_CYAN;
}
static uint16_t corPonteiroConsumo(float v) {
  if (v < 100.0f) return TFT_CYAN;
  if (v < 200.0f) return TFT_GREEN;
  if (v < 400.0f) return TFT_ORANGE;
  return TFT_RED;
}

// ===============================================================
// Animação
// ===============================================================
static void atualizarAnimacoes() {
  unsigned long agora = millis();

  float taxa = gTelemetria.taxaOBD;
  float periodo = (taxa > 0.5f) ? (1000.0f / taxa) : 200.0f;
  if (periodo > 500.0f) periodo = 500.0f;
  if (periodo < 20.0f)  periodo = 20.0f;

  const float renderPeriodMs = (float)RENDER_PERIOD_MS;

  if (periodo <= renderPeriodMs) {
    rpmShown   = rpmPrev   = rpmTarget   = gTelemetria.rpm;
    boostShown = boostPrev = boostTarget = gTelemetria.boost;
    consShown  = consPrev  = consTarget  = gTelemetria.consumo_ml_min;
    tpsShown   = tpsPrev   = tpsTarget   = gTelemetria.tps;
    pedalShown = pedalPrev = pedalTarget = gTelemetria.pedal;
    return;
  }

  if (agora - tInicioAnim >= (unsigned long)periodo) {
    rpmPrev   = rpmShown;   rpmTarget   = gTelemetria.rpm;
    boostPrev = boostShown; boostTarget = gTelemetria.boost;
    consPrev  = consShown;  consTarget  = gTelemetria.consumo_ml_min;
    tpsPrev   = tpsShown;   tpsTarget   = gTelemetria.tps;
    pedalPrev = pedalShown; pedalTarget = gTelemetria.pedal;
    tInicioAnim = agora;
  }

  float t = (agora - tInicioAnim) / periodo;
  if (t > 1.0f) t = 1.0f;
  if (t < 0.0f) t = 0.0f;

  rpmShown   = rpmPrev   + (rpmTarget   - rpmPrev)   * t;
  boostShown = boostPrev + (boostTarget - boostPrev) * t;
  consShown  = consPrev  + (consTarget  - consPrev)  * t;
  tpsShown   = tpsPrev   + (tpsTarget   - tpsPrev)   * t;
  pedalShown = pedalPrev + (pedalTarget - pedalPrev) * t;
}

// ===============================================================
// Ícones
// ===============================================================
static void desenharIconeBateria(TFT_eSprite& c, int x, int y) {
  c.drawRect(x, y + 2, 14, 9, TFT_WHITE);
  c.fillRect(x + 2, y, 3, 2, TFT_WHITE);
  c.fillRect(x + 9, y, 3, 2, TFT_WHITE);
  c.drawFastHLine(x + 2, y + 6, 3, TFT_WHITE);
  c.drawRect(x + 9, y + 5, 3, 3, TFT_WHITE);
}
static void desenharIconeCoolant(TFT_eSprite& c, int x, int y) {
  c.drawRect(x + 5, y, 4, 10, TFT_WHITE);
  c.fillCircle(x + 7, y + 10, 4, TFT_WHITE);
  c.drawFastHLine(x, y + 13, 14, TFT_CYAN);
  c.drawFastHLine(x + 2, y + 15, 10, TFT_CYAN);
}
static void desenharIconeIntake(TFT_eSprite& c, int x, int y) {
  c.drawRect(x, y + 2, 14, 8, TFT_WHITE);
  c.fillRect(x + 5, y, 4, 12, TFT_BLUE);
}
static void desenharIconeTPS(TFT_eSprite& c, int cx, int cy,
                             float tpsAnim, float tpsReal) {
  int raioBola = 6;
  int alturaTracoMax = 24;
  c.drawCircle(cx, cy, raioBola, TFT_WHITE);
  float anguloGraus = 90.0f - (tpsAnim * 90.0f / 100.0f);
  float anguloRad = anguloGraus * DEG_TO_RAD;
  int x1 = cx + (cosf(anguloRad) * (alturaTracoMax / 2));
  int y1 = cy - (sinf(anguloRad) * (alturaTracoMax / 2));
  int x2 = cx - (cosf(anguloRad) * (alturaTracoMax / 2));
  int y2 = cy + (sinf(anguloRad) * (alturaTracoMax / 2));
  c.drawLine(x1, y1, x2, y2, TFT_ORANGE);
  c.setTextFont(1);
  c.setTextSize(1);
  c.setTextColor(0xC618);
  c.setTextDatum(TC_DATUM);
  char buf[16];
  snprintf(buf, sizeof(buf), "%d%% tps", (int)tpsReal);
  c.drawString(buf, cx, cy + 14);
  c.setTextDatum(TL_DATUM);
}
static void desenharBarraPedal(TFT_eSprite& c, int x, int y,
                               int largura, int altura, float pedalPercent) {
  if (pedalPercent < 0) pedalPercent = 0;
  if (pedalPercent > 100) pedalPercent = 100;
  c.drawRect(x, y, largura, altura, TFT_WHITE);
  int altPreench = (int)(pedalPercent * (altura - 2) / 100.0);
  if (altPreench > 0) {
    c.fillRect(x + 1, y + altura - 1 - altPreench, largura - 2, altPreench, TFT_CYAN);
  }
  c.setTextFont(1);
  c.setTextColor(0xC618);
  c.setTextDatum(TC_DATUM);
  c.drawString("P", x + largura / 2, y + altura + 1);
  c.setTextDatum(TL_DATUM);
}

// ===============================================================
// Barra RPM
// ===============================================================
static void desenharBarraRPMAnaDigi(TFT_eSprite& c, float rpmAtual) {
  const float rpmMin = 0.0f;
  const float redlineStart = 6500.0f;
  const float rpmMax = 8000.0f;

  const int barraLarguraBloco = 4;
  const int barraLarguraEspaco = 1;
  const int alturaBarraRPM = 32;
  const int inicioCorteY = 18;
  const int corteVelocidadeX = 110;
  const int corteVelocidadeLarg = 100;
  const int corteRpmDigitalX = 230;
  const int corteRpmDigitalLarg = 85;

  int cicloTotal = barraLarguraBloco + barraLarguraEspaco;
  int limitePixels = (int)((rpmAtual - rpmMin) * 320.0f / (rpmMax - rpmMin));
  if (limitePixels < 0) limitePixels = 0;
  if (limitePixels > 320) limitePixels = 320;

  for (int x = 0; x < 320; x++) {
    if ((x % cicloTotal) >= barraLarguraBloco) continue;
    if (x <= limitePixels) {
      float rpmNestePixel = rpmMin + (x * (rpmMax - rpmMin) / 320.0f);
      c.drawFastVLine(x, 0, alturaBarraRPM,
                      (rpmNestePixel < redlineStart) ? TFT_GREEN : TFT_RED);
    } else {
      c.drawFastVLine(x, 0, alturaBarraRPM, 0x39E7);
    }
  }
  int alturaDoCorte = alturaBarraRPM - inicioCorteY;
  c.fillRect(corteVelocidadeX, inicioCorteY, corteVelocidadeLarg, alturaDoCorte, TFT_BLACK);
  c.fillRect(corteRpmDigitalX,  inicioCorteY, corteRpmDigitalLarg,  alturaDoCorte, TFT_BLACK);
}

// ===============================================================
static void drawLED(int x, int y, bool on) {
  if (on) {
    canvas.fillCircle(x, y, 4, TFT_RED);
    canvas.drawCircle(x, y, 5, TFT_WHITE);
  } else {
    canvas.drawCircle(x, y, 4, 0x4208);
  }
}

// ===============================================================
static void configBoost(GaugeCfg& g, int cx, int cy, int raio) {
  g.cx = cx; g.cy = cy; g.raio = raio;
  g.strokeWidth   = 12;
  g.subOffset     = 2;
  g.subThickness  = 2;
  g.vMin = -15.0f;
  g.vMax =  15.0f;
  g.inicioArco = 210.0f;
  g.fimArco    = 330.0f;
  g.corTrilho    = 0x18E3;
  g.corSub       = 0x8410;
  g.corPonteiro  = TFT_CYAN;
  g.corLabel     = TFT_WHITE;
  g.innerFactor  = 0.55f;
  g.outerFactor  = 0.98f;
  g.pointerWidth = 6;
  g.numTicks  = 5;
  g.tickWidth = 3;
  g.labelCentral = "psi";
  g.valorMin     = "-15";
  g.valorMax     = "+15";
  g.labelFont   = 2;
  g.centralFont = 2;
  g.labelPct    = 0.88f;
  g.centralPct  = 0.32f;
  g.labelMinXOffset = 0;
  g.labelMaxXOffset = 0;
}
static void configConsumo(GaugeCfg& g, int cx, int cy, int raio) {
  configBoost(g, cx, cy, raio);
  g.vMin = 0.0f;
  g.vMax = 400.0f;
  g.labelCentral = "ml/m";
  g.valorMin     = "0";
  g.valorMax     = "400";
  g.labelMinXOffset = 5;
}

// ===============================================================
// COLUNA ESQUERDA
// ===============================================================
static void drawColunaEsquerda() {
  desenharBarraRPMAnaDigi(canvas, rpmShown);

  canvas.setFreeFont(&FreeSansBold12pt7b);
  canvas.setTextSize(1);
  canvas.setTextColor(TFT_WHITE);
  canvas.setTextDatum(TR_DATUM);
  char bufRPM[8];
  snprintf(bufRPM, sizeof(bufRPM), "%d", (int)gTelemetria.rpm);
  canvas.drawString(bufRPM, 282, 20);
  canvas.setTextDatum(TL_DATUM);

  canvas.setTextFont(1);
  canvas.setTextSize(1);
  canvas.setTextColor(TFT_DARKGREY);
  canvas.drawString("rpm", 286, 25);

  int offsetHUD_Y = 42;
  canvas.setTextFont(2);
  canvas.setTextSize(1);
  canvas.setTextDatum(TL_DATUM);

  desenharIconeBateria(canvas, 6, offsetHUD_Y);
  canvas.setTextColor(TFT_WHITE);
  canvas.drawString(String(gTelemetria.bateria, 1) + "V", 26, offsetHUD_Y);

  desenharIconeCoolant(canvas, 6, offsetHUD_Y + 18);
  canvas.drawString(String((int)gTelemetria.tempCoolant) + " C", 26, offsetHUD_Y + 18);

  desenharIconeIntake(canvas, 6, offsetHUD_Y + 36);
  canvas.drawString(String((int)gTelemetria.tempIntake) + " C", 26, offsetHUD_Y + 36);

  int vel = (int)gTelemetria.velocidade;
  if (vel < 0) vel = 0;
  if (vel > 999) vel = 999;
  int c = vel / 100;
  int d = (vel % 100) / 10;
  int u = vel % 10;
  int startX = 112;
  int posY = 25;
  int largCaractere = 30;

  canvas.setTextFont(7);
  canvas.setTextSize(1);

  canvas.setTextColor((c == 0) ? TFT_BLACK : TFT_WHITE);
  canvas.setCursor(startX, posY);
  canvas.printf("%d", c);

  canvas.setTextColor((c == 0 && d == 0) ? TFT_BLACK : TFT_WHITE);
  canvas.setCursor(startX + largCaractere, posY);
  canvas.printf("%d", d);

  canvas.setTextColor(TFT_WHITE);
  canvas.setCursor(startX + largCaractere * 2, posY);
  canvas.printf("%d", u);

  canvas.setTextFont(2);
  canvas.setTextSize(1);
  canvas.setTextColor(TFT_WHITE);
  canvas.setTextDatum(ML_DATUM);
  canvas.drawString("km/h", 210, 62);

  desenharIconeTPS(canvas, 272, 56, tpsShown, gTelemetria.tps);
  desenharBarraPedal(canvas, 296, 42, 12, 28, pedalShown);

  // ---- Área cyan ----
  canvas.setTextFont(2);
  canvas.setTextSize(1);
  canvas.setTextColor(TFT_CYAN);

  canvas.setTextDatum(TL_DATUM);

  // ★ MAF acima do boost
  canvas.drawString(String("MAF: ") + String(gTelemetria.maf, 1) + " g/s", 8, 171);

  canvas.drawString(String("Boost: ") + String(gTelemetria.boost, 2) + " psi", 8, 187);
  canvas.drawString(String("Max: ") + String(gTelemetria.boostMax, 2) + " psi", 8, 203);

  // ---- Área cyan (direita) ----
  canvas.setTextDatum(TR_DATUM);

  // ★ Movidos 1 linha pra cima
  canvas.drawString(String(gTelemetria.consumo_total_litros, 3) + " L", 312, 155);

  String txtL100;
  if (gTelemetria.velocidade < 5.0f) {
    txtL100 = "-- L/100km";
  } else {
    txtL100 = String(gTelemetria.consumo_l_100km, 1) + " L/100km";
  }
  canvas.drawString(txtL100, 312, 171);

  // ★ Max do consumo instantâneo (local)
  canvas.drawString(String("Max: ") + String((int)consumoMaxShown) + " ml/min", 312, 187);

  // Consumo instantâneo (na posição original)
  canvas.drawString(String((int)gTelemetria.consumo_ml_min) + " ml/min", 312, 203);

  // ---- Gauges ----
  cfgBoostSmall.corPonteiro  = corPonteiroBoost(boostShown);
  cfgConsumSmall.corPonteiro = corPonteiroConsumo(consShown);

  drawGaugeCircular(canvas, cacheBoostSmall,  cfgBoostSmall,  boostShown);
  drawGaugeCircular(canvas, cacheConsumSmall, cfgConsumSmall, consShown);

  drawLED(18,  228, gTelemetria.boost > 15.0f);
  drawLED(302, 228, gTelemetria.consumo_ml_min > 400.0f);
}

// ===============================================================
// COLUNA DIREITA
// ===============================================================
static void desenharHamburger(TFT_eSprite& c, int x, int y, int w, int h, bool hover) {
  if (hover) {
    c.fillRect(x, y, w, h, 0x049F);
    c.drawRect(x, y, w, h, TFT_WHITE);
  } else {
    c.drawRect(x, y, w, h, 0x7BEF);
  }

  int cx = x + w / 2;
  int lw = w - 14;
  int lx = cx - lw / 2;
  c.drawFastHLine(lx, y + h / 2 - 6, lw, TFT_WHITE);
  c.drawFastHLine(lx, y + h / 2,     lw, TFT_WHITE);
  c.drawFastHLine(lx, y + h / 2 + 6, lw, TFT_WHITE);
}

static void drawColunaDireita() {
  const int x0 = 320;
  const int w  = 160;

  canvas.fillRect(x0, 0, w, 60, 0x0841);
  canvas.drawRect(x0, 0, w, 60, 0x39E7);

  canvas.setTextFont(1);
  canvas.setTextSize(1);
  canvas.setTextDatum(TL_DATUM);

  canvas.setTextColor(TFT_GREEN);
  canvas.drawString(String("FPS:") + String((int)gEstado.fpsRender) +
                    " Drw:" + String(gEstado.tempoDrawMs) + "ms",
                    x0 + 4, 3);

  canvas.setTextColor(TFT_CYAN);
  canvas.drawString(String("OBD:") + String((int)gTelemetria.taxaOBD) +
                    "Hz FFT:" + String((int)gAudio.taxaFFT) + "Hz",
                    x0 + 4, 15);

  ObdStatusGeral os = uart_get_obd_status();
  uint16_t corECU;
  if (!os.elmResponde)                    corECU = TFT_RED;
  else if (os.total == 0)                 corECU = TFT_RED;
  else if (os.respondendo == os.total)    corECU = TFT_GREEN;
  else if (os.respondendo == 0)           corECU = TFT_RED;
  else                                    corECU = TFT_YELLOW;

  canvas.setTextColor(corECU);
  canvas.drawString(String("ECU: ") + String(os.respondendo) +
                    "/" + String(os.total) + " PIDs",
                    x0 + 4, 27);

  canvas.setTextColor(TFT_YELLOW);
  canvas.drawString(String("Heap:") + String(gEstado.heapLivreKB) + "KB",
                    x0 + 4, 39);

  canvas.setTextColor(TFT_DARKGREY);
  canvas.drawString(String("UP:") + String(gEstado.uptimeMs / 1000) + "s",
                    x0 + 4, 51);

  desenharHamburger(canvas, x0 + w - 38, 6, 34, 48, hoverHamburger);

  canvas.setTextColor(TFT_CYAN);
  canvas.drawString(gAudio.artista, x0 + 4, 64);
  canvas.setTextColor(TFT_WHITE);
  canvas.drawString(gAudio.titulo, x0 + 4, 78);

  drawWaveformFromFFT(canvas, x0, 96, w, 80,
                      gAudio.fftBins, FFT_BANDAS_ORIGINAIS,
                      TFT_CYAN, 0x0000);

  drawFFTBars(canvas, x0, 176, w, 104,
              gAudio.fftBins, FFT_BANDAS_ORIGINAIS,
              TFT_GREEN, 0x0000);

  const int btnY = 284;
  const int btnH = 32;
  const int btnGap = 4;
  const int btnW = (w - btnGap * 2) / 3;
  const char* labels[3] = { "<<", ">", ">>" };
  for (int i = 0; i < 3; i++) {
    int bx = x0 + i * (btnW + btnGap);
    uint16_t corFundo = (hoverBtnMedia == i) ? 0x049F : 0x18E3;
    canvas.fillRect(bx, btnY, btnW, btnH, corFundo);
    canvas.drawRect(bx, btnY, btnW, btnH, TFT_WHITE);
    canvas.setTextColor(TFT_WHITE);
    canvas.setTextDatum(MC_DATUM);
    canvas.drawString(labels[i], bx + btnW / 2, btnY + btnH / 2);
  }
  canvas.setTextDatum(TL_DATUM);
}

// ===============================================================
void home_begin() {
  tInicio = millis();
  tInicioAnim = millis();
  consumoMaxShown = 0.0f;   // ★ reseta max local
  hoverBtnMedia = -1;
  hoverHamburger = false;

  configBoost  (cfgBoostSmall,   82, 310, 70);
  configConsumo(cfgConsumSmall, 238, 310, 70);

  if (!backgroundsProntos) {
    gaugeBuildBg(cacheBoostSmall,  cfgBoostSmall);
    gaugeBuildBg(cacheConsumSmall, cfgConsumSmall);
    backgroundsProntos = true;
  }

  canvas.fillSprite(TFT_BLACK);
}

void home_loop(uint32_t now) {}
void home_end() {}

void home_draw() {
  uint32_t tDraw = millis();

  // ★ Atualiza max local antes de desenhar
  if (gTelemetria.consumo_ml_min > consumoMaxShown) {
    consumoMaxShown = gTelemetria.consumo_ml_min;
  }

  atualizarAnimacoes();
  canvas.fillSprite(TFT_BLACK);
  drawColunaEsquerda();
  drawColunaDireita();
  gEstado.tempoDrawMs = millis() - tDraw;
}

// ===============================================================
// Touch
// ===============================================================
void home_hover(int x, int y) {
  hoverBtnMedia = -1;
  hoverHamburger = false;

  if (x >= 320 && y >= 284 && y <= 316) {
    int relX = x - 320;
    if (relX < 52)         hoverBtnMedia = 0;
    else if (relX < 108)   hoverBtnMedia = 1;
    else                   hoverBtnMedia = 2;
    return;
  }

  if (x >= 320 + 160 - 40 && x <= 320 + 160 - 2 &&
      y >= 2 && y <= 58) {
    hoverHamburger = true;
  }
}

void home_release(int x, int y) {
  if (hoverBtnMedia >= 0) {
    if (x >= 320 && y >= 284 && y <= 316) {
      int relX = x - 320;
      if      (hoverBtnMedia == 0 && relX < 52)                uart_send_media(0);
      else if (hoverBtnMedia == 1 && relX >= 52 && relX < 108) uart_send_media(1);
      else if (hoverBtnMedia == 2 && relX >= 108)              uart_send_media(2);
    }
    hoverBtnMedia = -1;
    return;
  }

  if (hoverHamburger) {
    if (x >= 320 + 160 - 40 && x <= 320 + 160 - 2 &&
        y >= 2 && y <= 58) {
      mode_set(SCREEN_MENU);
    }
    hoverHamburger = false;
    return;
  }
}