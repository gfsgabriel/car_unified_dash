#include "src/common/dash_cfg.h"
#include <Preferences.h>

static Preferences prefsDash;

DashConfig gDashCfg;

void dashConfigInit() {
  prefsDash.begin("dash", true);
  gDashCfg.rpmMax     = prefsDash.getUShort("rpm_max",     8000);
  gDashCfg.rpmOrange  = prefsDash.getUShort("rpm_orange",  5500);
  gDashCfg.rpmRedline = prefsDash.getUShort("rpm_redline", 6500);

  gDashCfg.baseR = prefsDash.getUChar("base_r", 0);
  gDashCfg.baseG = prefsDash.getUChar("base_g", 255);
  gDashCfg.baseB = prefsDash.getUChar("base_b", 0);

  gDashCfg.midR = prefsDash.getUChar("mid_r", 255);
  gDashCfg.midG = prefsDash.getUChar("mid_g", 128);
  gDashCfg.midB = prefsDash.getUChar("mid_b", 0);

  gDashCfg.redR = prefsDash.getUChar("red_r", 255);
  gDashCfg.redG = prefsDash.getUChar("red_g", 0);
  gDashCfg.redB = prefsDash.getUChar("red_b", 0);

  gDashCfg.midFull     = prefsDash.getBool("mid_full", false);
  gDashCfg.redlineFull = prefsDash.getBool("red_full", false);
  prefsDash.end();

  // Sanity
  if (gDashCfg.rpmOrange >= gDashCfg.rpmRedline)
    gDashCfg.rpmOrange = gDashCfg.rpmRedline - 100;
  if (gDashCfg.rpmRedline >= gDashCfg.rpmMax)
    gDashCfg.rpmMax = gDashCfg.rpmRedline + 100;
}

void dashConfigSave() {
  prefsDash.begin("dash", false);
  prefsDash.putUShort("rpm_max",     gDashCfg.rpmMax);
  prefsDash.putUShort("rpm_orange",  gDashCfg.rpmOrange);
  prefsDash.putUShort("rpm_redline", gDashCfg.rpmRedline);
  prefsDash.putUChar("base_r", gDashCfg.baseR);
  prefsDash.putUChar("base_g", gDashCfg.baseG);
  prefsDash.putUChar("base_b", gDashCfg.baseB);
  prefsDash.putUChar("mid_r",  gDashCfg.midR);
  prefsDash.putUChar("mid_g",  gDashCfg.midG);
  prefsDash.putUChar("mid_b",  gDashCfg.midB);
  prefsDash.putUChar("red_r",  gDashCfg.redR);
  prefsDash.putUChar("red_g",  gDashCfg.redG);
  prefsDash.putUChar("red_b",  gDashCfg.redB);
  prefsDash.putBool("mid_full", gDashCfg.midFull);
  prefsDash.putBool("red_full", gDashCfg.redlineFull);
  prefsDash.end();
}

void dashConfigResetDefaults() {
  gDashCfg.rpmMax     = 8000;
  gDashCfg.rpmOrange  = 5500;
  gDashCfg.rpmRedline = 6500;
  gDashCfg.baseR = 0;   gDashCfg.baseG = 255; gDashCfg.baseB = 0;
  gDashCfg.midR  = 255; gDashCfg.midG  = 128; gDashCfg.midB  = 0;
  gDashCfg.redR  = 255; gDashCfg.redG  = 0;   gDashCfg.redB  = 0;
  gDashCfg.midFull = false;
  gDashCfg.redlineFull = false;
  dashConfigSave();
}

uint16_t dashRGB565(uint8_t r, uint8_t g, uint8_t b) {
  return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}