#include "src/screens/screen_settings.h"
#include "src/screens/screen_base.h"
#include "src/core/mode_manager.h"
#include "src/core/display_manager.h"
#include "src/common/config.h"
#include "src/common/dash_cfg.h"

// =============================================================
// Layout — tela principal
// =============================================================
#define TOP_H        30
#define BTN_HEIGHT   40
#define ROW_BASE_Y   40
#define ROW_MID_Y    96
#define ROW_RED_Y    152
#define ROW_MAX_Y    208

#define BTN_MINUS_X  105
#define BTN_PLUS_X   250
#define BTN_WIDTH    45
#define VAL_X        155
#define VAL_W        90
#define SWATCH_X     305
#define SWATCH_W     60
#define FULL_X       375
#define FULL_W       95

#define BACK_BTN_W   100
#define BACK_BTN_H   24
#define BACK_BTN_X   (SCR_W - BACK_BTN_W - 8)
#define BACK_BTN_Y   3

#define RESET_Y      268
#define RESET_W      220
#define RESET_H      40
#define RESET_X      ((SCR_W - RESET_W) / 2)

// Color picker
#define PICK_BTN_W   90
#define PICK_BTN_H   24
#define PICK_OK_X    (SCR_W - PICK_BTN_W - 8)
#define PICK_CAN_X   (SCR_W - PICK_BTN_W * 2 - 14)

#define PICK_PREVIEW_Y 50
#define PICK_PREVIEW_H 50
#define PICK_PREVIEW_W 240

// Layout das linhas de canal (4 botões por linha)
#define CH_ROW_H      50
#define CH_LABEL_X    20          // centro do label R/G/B
#define CH_M10_X      40          // -10
#define CH_M10_W      70
#define CH_M1_X       115         // -1
#define CH_M1_W       55
#define CH_VAL_X      175         // valor
#define CH_VAL_W      130
#define CH_P1_X       310         // +1
#define CH_P1_W       55
#define CH_P10_X      370         // +10
#define CH_P10_W      70

#define PICK_CH_R_Y   125
#define PICK_CH_G_Y   178
#define PICK_CH_B_Y   231

#define RPM_STEP     100
#define RGB_STEP_1   1
#define RGB_STEP_10  10

// =============================================================
// Button IDs
// =============================================================
enum : int {
  BTN_NONE = -1,
  BTN_BACK = 1,
  BTN_RESET = 2,

  BTN_BASE_SW = 10,

  BTN_MID_MINUS = 20,
  BTN_MID_PLUS  = 22,
  BTN_MID_SW    = 23,
  BTN_MID_FULL  = 24,

  BTN_RED_MINUS = 30,
  BTN_RED_PLUS  = 32,
  BTN_RED_SW    = 33,
  BTN_RED_FULL  = 34,

  BTN_MAX_MINUS = 40,
  BTN_MAX_PLUS  = 42,

  BTN_PICK_CANCEL  = 50,
  BTN_PICK_OK      = 51,

  // Cada canal tem 4 botões: -10, -1, +1, +10
  BTN_PICK_R_M10 = 60,
  BTN_PICK_R_M1  = 61,
  BTN_PICK_R_P1  = 62,
  BTN_PICK_R_P10 = 63,

  BTN_PICK_G_M10 = 64,
  BTN_PICK_G_M1  = 65,
  BTN_PICK_G_P1  = 66,
  BTN_PICK_G_P10 = 67,

  BTN_PICK_B_M10 = 68,
  BTN_PICK_B_M1  = 69,
  BTN_PICK_B_P1  = 70,
  BTN_PICK_B_P10 = 71,
};

enum SettingsView { SV_MAIN, SV_COLOR_PICKER };

static SettingsView view = SV_MAIN;
static int hoverBtn = BTN_NONE;
static int pickingZone = 0;
static uint8_t pickR, pickG, pickB;

// =============================================================
// Helpers — mudam valores com clamp
// =============================================================
static void changeMid(int delta) {
  int v = (int)gDashCfg.rpmOrange + delta;
  if (v < 1000) v = 1000;
  if (v > (int)gDashCfg.rpmRedline - 100) v = gDashCfg.rpmRedline - 100;
  gDashCfg.rpmOrange = v;
  dashConfigSave();
}
static void changeRedline(int delta) {
  int v = (int)gDashCfg.rpmRedline + delta;
  if (v < (int)gDashCfg.rpmOrange + 100) v = gDashCfg.rpmOrange + 100;
  if (v > (int)gDashCfg.rpmMax - 100)    v = gDashCfg.rpmMax - 100;
  gDashCfg.rpmRedline = v;
  dashConfigSave();
}
static void changeMax(int delta) {
  int v = (int)gDashCfg.rpmMax + delta;
  if (v < (int)gDashCfg.rpmRedline + 100) v = gDashCfg.rpmRedline + 100;
  if (v > 12000) v = 12000;
  gDashCfg.rpmMax = v;
  dashConfigSave();
}

// =============================================================
// Widgets
// =============================================================
static void drawButton(int x, int y, int w, int h, const char* label,
                       int id, uint16_t bgNorm, uint16_t bgHover) {
  uint16_t bg = (hoverBtn == id) ? bgHover : bgNorm;
  canvas.fillRect(x, y, w, h, bg);
  canvas.drawRect(x, y, w, h, TFT_WHITE);
  canvas.setTextDatum(MC_DATUM);
  canvas.setTextFont(2);
  canvas.setTextColor(TFT_WHITE);
  canvas.drawString(label, x + w/2, y + h/2);
  canvas.setTextDatum(TL_DATUM);
}

static void drawValue(int x, int y, int w, int h, uint16_t val) {
  canvas.fillRect(x, y, w, h, 0x18E3);
  canvas.drawRect(x, y, w, h, TFT_WHITE);
  canvas.setTextDatum(MC_DATUM);
  canvas.setTextFont(2);
  canvas.setTextColor(TFT_GREEN);
  canvas.drawString(String(val), x + w/2, y + h/2);
  canvas.setTextDatum(TL_DATUM);
}

static void drawSwatch(int x, int y, int w, int h,
                       uint8_t r, uint8_t g, uint8_t b, int id) {
  uint16_t color = dashRGB565(r, g, b);
  uint16_t border = (hoverBtn == id) ? TFT_CYAN : TFT_WHITE;
  canvas.fillRect(x, y, w, h, color);
  canvas.drawRect(x, y, w, h, border);
  if (hoverBtn == id) canvas.drawRect(x+1, y+1, w-2, h-2, border);
}

static void drawToggle(int x, int y, int w, int h, const char* label,
                       bool on, int id) {
  uint16_t bg;
  if (on) bg = 0x02C0;
  else    bg = (hoverBtn == id) ? 0x049F : 0x18E3;
  canvas.fillRect(x, y, w, h, bg);
  canvas.drawRect(x, y, w, h, TFT_WHITE);
  canvas.setTextDatum(MC_DATUM);
  canvas.setTextFont(2);
  canvas.setTextColor(TFT_WHITE);
  canvas.drawString(label, x + w/2, y + h/2);
  canvas.setTextDatum(TL_DATUM);
}

static void drawLabel(int y, const char* txt) {
  canvas.setTextFont(2);
  canvas.setTextColor(TFT_WHITE);
  canvas.setTextDatum(TL_DATUM);
  canvas.setCursor(10, y + 14);
  canvas.print(txt);
}

// =============================================================
// Draw — main
// =============================================================
static void drawMainView() {
  canvas.fillRect(0, 0, SCR_W, TOP_H, 0x18E3);
  canvas.setTextFont(2);
  canvas.setTextColor(TFT_CYAN);
  canvas.setTextDatum(TL_DATUM);
  canvas.setCursor(8, 8);
  canvas.print("DASH SETTINGS");

  drawButton(BACK_BTN_X, BACK_BTN_Y, BACK_BTN_W, BACK_BTN_H,
             "VOLTAR", BTN_BACK, 0x8000, 0xC000);

  drawLabel(ROW_BASE_Y, "Base");
  drawSwatch(BTN_MINUS_X, ROW_BASE_Y + 3,
             SCR_W - BTN_MINUS_X - 10, BTN_HEIGHT,
             gDashCfg.baseR, gDashCfg.baseG, gDashCfg.baseB, BTN_BASE_SW);

  drawLabel(ROW_MID_Y, "Mid");
  drawButton(BTN_MINUS_X, ROW_MID_Y + 3, BTN_WIDTH, BTN_HEIGHT,
             "-", BTN_MID_MINUS, 0x18E3, 0x049F);
  drawValue(VAL_X, ROW_MID_Y + 3, VAL_W, BTN_HEIGHT, gDashCfg.rpmOrange);
  drawButton(BTN_PLUS_X, ROW_MID_Y + 3, BTN_WIDTH, BTN_HEIGHT,
             "+", BTN_MID_PLUS, 0x18E3, 0x049F);
  drawSwatch(SWATCH_X, ROW_MID_Y + 3, SWATCH_W, BTN_HEIGHT,
             gDashCfg.midR, gDashCfg.midG, gDashCfg.midB, BTN_MID_SW);
  drawToggle(FULL_X, ROW_MID_Y + 3, FULL_W, BTN_HEIGHT,
             gDashCfg.midFull ? "FULL Y" : "FULL N",
             gDashCfg.midFull, BTN_MID_FULL);

  drawLabel(ROW_RED_Y, "Redline");
  drawButton(BTN_MINUS_X, ROW_RED_Y + 3, BTN_WIDTH, BTN_HEIGHT,
             "-", BTN_RED_MINUS, 0x18E3, 0x049F);
  drawValue(VAL_X, ROW_RED_Y + 3, VAL_W, BTN_HEIGHT, gDashCfg.rpmRedline);
  drawButton(BTN_PLUS_X, ROW_RED_Y + 3, BTN_WIDTH, BTN_HEIGHT,
             "+", BTN_RED_PLUS, 0x18E3, 0x049F);
  drawSwatch(SWATCH_X, ROW_RED_Y + 3, SWATCH_W, BTN_HEIGHT,
             gDashCfg.redR, gDashCfg.redG, gDashCfg.redB, BTN_RED_SW);
  drawToggle(FULL_X, ROW_RED_Y + 3, FULL_W, BTN_HEIGHT,
             gDashCfg.redlineFull ? "FULL Y" : "FULL N",
             gDashCfg.redlineFull, BTN_RED_FULL);

  drawLabel(ROW_MAX_Y, "Max");
  drawButton(BTN_MINUS_X, ROW_MAX_Y + 3, BTN_WIDTH, BTN_HEIGHT,
             "-", BTN_MAX_MINUS, 0x18E3, 0x049F);
  drawValue(VAL_X, ROW_MAX_Y + 3, VAL_W, BTN_HEIGHT, gDashCfg.rpmMax);
  drawButton(BTN_PLUS_X, ROW_MAX_Y + 3, BTN_WIDTH, BTN_HEIGHT,
             "+", BTN_MAX_PLUS, 0x18E3, 0x049F);

  drawButton(RESET_X, RESET_Y, RESET_W, RESET_H,
             "RESET DEFAULT", BTN_RESET, 0x8000, 0xC000);
}

// =============================================================
// Draw — color picker
// =============================================================
static void drawChannelRow(int y, const char* name, uint8_t val,
                           int idM10, int idM1, int idP1, int idP10) {
  // Label R/G/B
  canvas.setTextDatum(MC_DATUM);
  canvas.setTextFont(4);
  canvas.setTextColor(TFT_WHITE);
  canvas.drawString(name, CH_LABEL_X, y + CH_ROW_H / 2);

  // -10
  uint16_t bg = (hoverBtn == idM10) ? 0x049F : 0x18E3;
  canvas.fillRect(CH_M10_X, y, CH_M10_W, CH_ROW_H, bg);
  canvas.drawRect(CH_M10_X, y, CH_M10_W, CH_ROW_H, TFT_WHITE);
  canvas.setTextFont(2);
  canvas.setTextColor(TFT_WHITE);
  canvas.drawString("-10", CH_M10_X + CH_M10_W/2, y + CH_ROW_H/2);

  // -1
  bg = (hoverBtn == idM1) ? 0x049F : 0x18E3;
  canvas.fillRect(CH_M1_X, y, CH_M1_W, CH_ROW_H, bg);
  canvas.drawRect(CH_M1_X, y, CH_M1_W, CH_ROW_H, TFT_WHITE);
  canvas.drawString("-1", CH_M1_X + CH_M1_W/2, y + CH_ROW_H/2);

  // valor
  canvas.fillRect(CH_VAL_X, y, CH_VAL_W, CH_ROW_H, 0x18E3);
  canvas.drawRect(CH_VAL_X, y, CH_VAL_W, CH_ROW_H, TFT_WHITE);
  canvas.setTextFont(4);
  canvas.setTextColor(TFT_GREEN);
  canvas.drawString(String(val), CH_VAL_X + CH_VAL_W/2, y + CH_ROW_H/2);

  // +1
  bg = (hoverBtn == idP1) ? 0x049F : 0x18E3;
  canvas.fillRect(CH_P1_X, y, CH_P1_W, CH_ROW_H, bg);
  canvas.drawRect(CH_P1_X, y, CH_P1_W, CH_ROW_H, TFT_WHITE);
  canvas.setTextFont(2);
  canvas.setTextColor(TFT_WHITE);
  canvas.drawString("+1", CH_P1_X + CH_P1_W/2, y + CH_ROW_H/2);

  // +10
  bg = (hoverBtn == idP10) ? 0x049F : 0x18E3;
  canvas.fillRect(CH_P10_X, y, CH_P10_W, CH_ROW_H, bg);
  canvas.drawRect(CH_P10_X, y, CH_P10_W, CH_ROW_H, TFT_WHITE);
  canvas.drawString("+10", CH_P10_X + CH_P10_W/2, y + CH_ROW_H/2);

  canvas.setTextDatum(TL_DATUM);
}

static void drawColorPickerView() {
  canvas.fillRect(0, 0, SCR_W, TOP_H, 0x18E3);
  canvas.setTextFont(2);
  canvas.setTextColor(TFT_CYAN);
  canvas.setTextDatum(TL_DATUM);
  canvas.setCursor(8, 8);
  const char* names[] = { "BASE", "MID", "REDLINE" };
  char buf[40];
  snprintf(buf, sizeof(buf), "PICK COLOR: %s", names[pickingZone]);
  canvas.print(buf);

  drawButton(PICK_CAN_X, 3, PICK_BTN_W, PICK_BTN_H,
             "CANCEL", BTN_PICK_CANCEL, 0x8000, 0xC000);
  drawButton(PICK_OK_X, 3, PICK_BTN_W, PICK_BTN_H,
             "OK", BTN_PICK_OK, 0x006400, 0x00A000);

  // Preview
  uint16_t previewColor = dashRGB565(pickR, pickG, pickB);
  int px = (SCR_W - PICK_PREVIEW_W) / 2;
  canvas.fillRect(px, PICK_PREVIEW_Y, PICK_PREVIEW_W, PICK_PREVIEW_H, previewColor);
  canvas.drawRect(px, PICK_PREVIEW_Y, PICK_PREVIEW_W, PICK_PREVIEW_H, TFT_WHITE);
  canvas.drawRect(px+1, PICK_PREVIEW_Y+1, PICK_PREVIEW_W-2, PICK_PREVIEW_H-2, TFT_WHITE);

  drawChannelRow(PICK_CH_R_Y, "R", pickR,
                 BTN_PICK_R_M10, BTN_PICK_R_M1, BTN_PICK_R_P1, BTN_PICK_R_P10);
  drawChannelRow(PICK_CH_G_Y, "G", pickG,
                 BTN_PICK_G_M10, BTN_PICK_G_M1, BTN_PICK_G_P1, BTN_PICK_G_P10);
  drawChannelRow(PICK_CH_B_Y, "B", pickB,
                 BTN_PICK_B_M10, BTN_PICK_B_M1, BTN_PICK_B_P1, BTN_PICK_B_P10);
}

// =============================================================
// Public API
// =============================================================
void settings_begin() { view = SV_MAIN; hoverBtn = BTN_NONE; }
void settings_loop(uint32_t now) {}
void settings_end() {}

void settings_draw() {
  canvas.fillSprite(TFT_BLACK);
  if (view == SV_MAIN) drawMainView();
  else                 drawColorPickerView();
}

// =============================================================
// Hit testing
// =============================================================
static int hitTestMain(int x, int y) {
  if (y >= BACK_BTN_Y && y <= BACK_BTN_Y + BACK_BTN_H &&
      x >= BACK_BTN_X && x <= BACK_BTN_X + BACK_BTN_W) return BTN_BACK;

  if (y >= ROW_BASE_Y + 3 && y <= ROW_BASE_Y + 3 + BTN_HEIGHT &&
      x >= BTN_MINUS_X && x <= SCR_W - 10) return BTN_BASE_SW;

  struct RowDef { int y, minus, plus, sw, full; };
  RowDef rows[] = {
    { ROW_MID_Y, BTN_MID_MINUS, BTN_MID_PLUS, BTN_MID_SW, BTN_MID_FULL },
    { ROW_RED_Y, BTN_RED_MINUS, BTN_RED_PLUS, BTN_RED_SW, BTN_RED_FULL },
    { ROW_MAX_Y, BTN_MAX_MINUS, BTN_MAX_PLUS, -1, -1 },
  };
  for (auto& r : rows) {
    if (y < r.y + 3 || y > r.y + 3 + BTN_HEIGHT) continue;
    if (x >= BTN_MINUS_X && x <= BTN_MINUS_X + BTN_WIDTH) return r.minus;
    if (x >= BTN_PLUS_X  && x <= BTN_PLUS_X  + BTN_WIDTH) return r.plus;
    if (r.sw > 0 && x >= SWATCH_X && x <= SWATCH_X + SWATCH_W) return r.sw;
    if (r.full > 0 && x >= FULL_X && x <= FULL_X + FULL_W) return r.full;
  }

  if (y >= RESET_Y && y <= RESET_Y + RESET_H &&
      x >= RESET_X && x <= RESET_X + RESET_W) return BTN_RESET;

  return BTN_NONE;
}

static int hitTestPicker(int x, int y) {
  if (y >= 3 && y <= 3 + PICK_BTN_H) {
    if (x >= PICK_CAN_X && x <= PICK_CAN_X + PICK_BTN_W) return BTN_PICK_CANCEL;
    if (x >= PICK_OK_X  && x <= PICK_OK_X  + PICK_BTN_W) return BTN_PICK_OK;
  }

  struct ChHit { int y; int m10, m1, p1, p10; };
  ChHit chans[] = {
    { PICK_CH_R_Y, BTN_PICK_R_M10, BTN_PICK_R_M1, BTN_PICK_R_P1, BTN_PICK_R_P10 },
    { PICK_CH_G_Y, BTN_PICK_G_M10, BTN_PICK_G_M1, BTN_PICK_G_P1, BTN_PICK_G_P10 },
    { PICK_CH_B_Y, BTN_PICK_B_M10, BTN_PICK_B_M1, BTN_PICK_B_P1, BTN_PICK_B_P10 },
  };
  for (auto& c : chans) {
    if (y < c.y || y >= c.y + CH_ROW_H) continue;
    if (x >= CH_M10_X && x < CH_M10_X + CH_M10_W) return c.m10;
    if (x >= CH_M1_X  && x < CH_M1_X  + CH_M1_W)  return c.m1;
    if (x >= CH_P1_X  && x < CH_P1_X  + CH_P1_W)  return c.p1;
    if (x >= CH_P10_X && x < CH_P10_X + CH_P10_W) return c.p10;
  }
  return BTN_NONE;
}

void settings_hover(int x, int y) {
  hoverBtn = (view == SV_MAIN) ? hitTestMain(x, y) : hitTestPicker(x, y);
}

// =============================================================
// Release
// =============================================================
static void applyRGBDelta(int delta) {
  if (pickingZone == 0) {
    int v = (int)pickR + delta;
    pickR = (v < 0) ? 0 : (v > 255 ? 255 : v);
  } else if (pickingZone == 1) {
    int v = (int)pickG + delta;
    pickG = (v < 0) ? 0 : (v > 255 ? 255 : v);
  } else {
    int v = (int)pickB + delta;
    pickB = (v < 0) ? 0 : (v > 255 ? 255 : v);
  }
}

static void onButtonPressed(int id) {
  if (id == BTN_NONE) return;

  if (view == SV_MAIN) {
    switch (id) {
      case BTN_BACK:  mode_set(SCREEN_MENU); break;
      case BTN_RESET: dashConfigResetDefaults(); break;

      case BTN_BASE_SW:
        pickingZone = 0;
        pickR = gDashCfg.baseR; pickG = gDashCfg.baseG; pickB = gDashCfg.baseB;
        view = SV_COLOR_PICKER; break;
      case BTN_MID_SW:
        pickingZone = 1;
        pickR = gDashCfg.midR; pickG = gDashCfg.midG; pickB = gDashCfg.midB;
        view = SV_COLOR_PICKER; break;
      case BTN_RED_SW:
        pickingZone = 2;
        pickR = gDashCfg.redR; pickG = gDashCfg.redG; pickB = gDashCfg.redB;
        view = SV_COLOR_PICKER; break;

      case BTN_MID_MINUS: changeMid(-RPM_STEP); break;
      case BTN_MID_PLUS:  changeMid(+RPM_STEP); break;
      case BTN_MID_FULL:
        gDashCfg.midFull = !gDashCfg.midFull;
        dashConfigSave(); break;

      case BTN_RED_MINUS: changeRedline(-RPM_STEP); break;
      case BTN_RED_PLUS:  changeRedline(+RPM_STEP); break;
      case BTN_RED_FULL:
        gDashCfg.redlineFull = !gDashCfg.redlineFull;
        dashConfigSave(); break;

      case BTN_MAX_MINUS: changeMax(-RPM_STEP); break;
      case BTN_MAX_PLUS:  changeMax(+RPM_STEP); break;
    }
  } else {
    switch (id) {
      case BTN_PICK_CANCEL: view = SV_MAIN; break;
      case BTN_PICK_OK:
        if (pickingZone == 0) { gDashCfg.baseR = pickR; gDashCfg.baseG = pickG; gDashCfg.baseB = pickB; }
        if (pickingZone == 1) { gDashCfg.midR  = pickR; gDashCfg.midG  = pickG; gDashCfg.midB  = pickB; }
        if (pickingZone == 2) { gDashCfg.redR  = pickR; gDashCfg.redG  = pickG; gDashCfg.redB  = pickB; }
        dashConfigSave();
        view = SV_MAIN; break;

      // R
      case BTN_PICK_R_M10: applyRGBDelta(-RGB_STEP_10); break;
      case BTN_PICK_R_M1:  applyRGBDelta(-RGB_STEP_1);  break;
      case BTN_PICK_R_P1:  applyRGBDelta(+RGB_STEP_1);  break;
      case BTN_PICK_R_P10: applyRGBDelta(+RGB_STEP_10); break;

      // G
      case BTN_PICK_G_M10: applyRGBDelta(-RGB_STEP_10); break;
      case BTN_PICK_G_M1:  applyRGBDelta(-RGB_STEP_1);  break;
      case BTN_PICK_G_P1:  applyRGBDelta(+RGB_STEP_1);  break;
      case BTN_PICK_G_P10: applyRGBDelta(+RGB_STEP_10); break;

      // B
      case BTN_PICK_B_M10: applyRGBDelta(-RGB_STEP_10); break;
      case BTN_PICK_B_M1:  applyRGBDelta(-RGB_STEP_1);  break;
      case BTN_PICK_B_P1:  applyRGBDelta(+RGB_STEP_1);  break;
      case BTN_PICK_B_P10: applyRGBDelta(+RGB_STEP_10); break;
    }
  }
}

void settings_release(int x, int y) {
  int id = (view == SV_MAIN) ? hitTestMain(x, y) : hitTestPicker(x, y);
  onButtonPressed(id);
  hoverBtn = BTN_NONE;
}