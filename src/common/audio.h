#ifndef COMMON_AUDIO_H
#define COMMON_AUDIO_H

#include <Arduino.h>

#define FFT_BANDAS_ORIGINAIS 9   // 60..16k
#define FFT_BANDAS_EXIBIDAS  8   // 8k absorve 16k

struct AudioState {
  uint8_t  fftBins[FFT_BANDAS_ORIGINAIS];   // 9 do WROOM
  uint32_t lastFFTMs;
  float    taxaFFT;

  char     titulo[41];
  char     artista[41];
  uint32_t tempoAtualMs;
  uint32_t tempoTotalMs;
  bool     btConectado;
  bool     audioAtivo;
  uint32_t lastMusicMs;
};

extern AudioState gAudio;

void audio_init();

#endif