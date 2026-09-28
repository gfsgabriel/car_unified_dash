#ifndef COMMON_DASH_CFG_H
#define COMMON_DASH_CFG_H

#include <Arduino.h>

struct DashConfig {
  // RPM
  uint16_t rpmMax;         // fundo da escala
  uint16_t rpmOrange;      // limite inferior da zona laranja
  uint16_t rpmRedline;     // limite inferior da zona vermelha

  // Cores (RGB 8-bit)
  uint8_t baseR, baseG, baseB;
  uint8_t midR,  midG,  midB;
  uint8_t redR,  redG,  redB;

  // Modo "cor dominante": quando entra na zona, pinta a barra toda
  bool midFull;
  bool redlineFull;
};

extern DashConfig gDashCfg;

void dashConfigInit();
void dashConfigSave();
void dashConfigResetDefaults();

// Converte RGB888 → RGB565 (formato do TFT_eSPI)
uint16_t dashRGB565(uint8_t r, uint8_t g, uint8_t b);

#endif