#ifndef COMMON_CONFIG_H
#define COMMON_CONFIG_H

#include <Arduino.h>

#define SCR_W_DEFAULT       480
#define SCR_H_DEFAULT       320
#define BAND_HEIGHT         40
#define RENDER_PERIOD_MS    33

#define DEBOUNCE_RELEASE_MS 50
#define TOUCH_QUEUE_LEN     256

#define BARRA_H             28

#define LOG_BUFFER_SIZE     40

#define LOG_TOUCH           0
#define LOG_STATS           0

#define USE_MOCK   0    // 1 = mock, 0 = fontes reais (OBD + audio bridge)

#endif