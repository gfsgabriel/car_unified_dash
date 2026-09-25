#include "src/common/audio.h"

AudioState gAudio;

void audio_init() {
  memset(&gAudio, 0, sizeof(gAudio));
  strcpy(gAudio.titulo, "--");
  strcpy(gAudio.artista, "--");
}