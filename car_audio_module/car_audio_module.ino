#include "ESP_I2S.h"
#include "BluetoothA2DPSink.h"
#include <arduinoFFT.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#define RX1_PIN 16
#define TX1_PIN 17
#define HEADER_0 0xAA
#define HEADER_1 0x55

const uint8_t I2S_SCK = 25; 
const uint8_t I2S_SDOUT = 26; 
const uint8_t I2S_WS = 27; 

I2SClass i2s;
BluetoothA2DPSink a2dp_sink;

struct ComandoSerial {
  uint8_t cmd;
  uint8_t param;
};
QueueHandle_t filaComandos;

// --- CONFIGURAÇÃO DA NOVA FFT DE ALTA RESOLUÇÃO ---
#define SAMPLES 512              // Aumentado para 512 para dar resolução nos graves (~86Hz por bin)
#define SAMPLING_FREQ 44100
#define NUM_BARRAS 32            // Mantido em 32 para preservar o tamanho fixo do seu pacote serial (68 bytes)

double vReal[SAMPLES];
double vImag[SAMPLES];
ArduinoFFT<double> FFT = ArduinoFFT<double>(vReal, vImag, SAMPLES, SAMPLING_FREQ);
uint8_t fftOutput[NUM_BARRAS];   // Envia 32 bytes fixos, mas usaremos apenas os 9 primeiros

// Buffer circular simples e seguro para transferir amostras entre o áudio e o Loop
int16_t bufferAmostras[SAMPLES];
volatile int idxBuffer = 0;
portMUX_TYPE mtxBuffer = portMUX_INITIALIZER_UNLOCKED;

static double picoMaximoGeral = 1000.0;
String tituloMusica = "--";
String nomeArtista = "--";
volatile uint32_t tempoAtualMs = 0;
uint32_t tempoTotalMs = 0;
unsigned long ultimaAtualizacaoPlayPos = 0;
bool bluetoothConectado = false;
bool estadoAudioIntencional = false;
bool pendingMetadataSend = true;
uint8_t volumeAtual = 100; // 0 a 127

void enviarFFTSerial() {
  uint8_t totalLen = 4 + NUM_BARRAS;
  uint8_t pacote[68];
  pacote[0] = HEADER_0;
  pacote[1] = HEADER_1;
  pacote[2] = 0x01; 
  pacote[3] = NUM_BARRAS; 
  
  for (int i = 0; i < NUM_BARRAS; i++) {
    pacote[4 + i] = fftOutput[i];
  }
  Serial1.write(pacote, totalLen);
}

// --- TRANSMISSÃO SERIAL DE METADADOS CORRIGIDA ---
void enviarMetadadosSerial() {
  // Se não estiver tocando, manda "--". Se der play, envia o texto armazenado.
  String titExibir = estadoAudioIntencional ? tituloMusica : "--";
  String artExibir = estadoAudioIntencional ? nomeArtista : "--";
  uint8_t lenTitulo = titExibir.length();
  uint8_t lenArtista = artExibir.length();
  
  if (lenTitulo > 40) lenTitulo = 40;
  if (lenArtista > 40) lenArtista = 40;
  uint8_t payloadLen = 1 + 4 + 4 + 1 + lenTitulo + 1 + lenArtista;
  uint8_t pacote[120];
  pacote[0] = HEADER_0;
  pacote[1] = HEADER_1;
  pacote[2] = 0x02; 
  pacote[3] = payloadLen;
  uint8_t flags = 0;
  if (bluetoothConectado) flags |= (1 << 0);
  if (estadoAudioIntencional) flags |= (1 << 1);
  pacote[4] = flags;
  
  pacote[5] = (tempoAtualMs >> 24) & 0xFF;
  pacote[6] = (tempoAtualMs >> 16) & 0xFF;
  pacote[7] = (tempoAtualMs >> 8) & 0xFF;
  pacote[8] = tempoAtualMs & 0xFF;
  
  pacote[9] = (tempoTotalMs >> 24) & 0xFF;
  pacote[10] = (tempoTotalMs >> 16) & 0xFF;
  pacote[11] = (tempoTotalMs >> 8) & 0xFF;
  pacote[12] = tempoTotalMs & 0xFF;
  
  pacote[13] = lenTitulo;
  memcpy(&pacote[14], titExibir.c_str(), lenTitulo);
  
  int idx = 14 + lenTitulo;
  pacote[idx] = lenArtista;
  memcpy(&pacote[idx + 1], artExibir.c_str(), lenArtista);
  Serial1.write(pacote, 4 + payloadLen);
  pendingMetadataSend = false;
}

// O callback do Bluetooth agora APENAS cospe o som no I2S e copia as amostras brutas
void audio_data_stream(const uint8_t *data, uint32_t length) {
  i2s.write((uint8_t*)data, length);
  
  int16_t *samples = (int16_t*)data;
  uint32_t sample_count = length / 4; // Canais estéreo de 16 bits
  
  portENTER_CRITICAL_ISR(&mtxBuffer);
  for (uint32_t i = 0; i < sample_count; i++) {
    if (idxBuffer < SAMPLES) {
      // Captura o canal esquerdo para a análise
      bufferAmostras[idxBuffer] = samples[i * 2];
      idxBuffer++;
    }
  }
  portEXIT_CRITICAL_ISR(&mtxBuffer);
}

// Função auxiliar para calcular a média de uma faixa de bins da FFT
double obterMediaBins(int binInicial, int binFinal) {
  double soma = 0;
  for (int i = binInicial; i <= binFinal; i++) {
    soma += vReal[i];
  }
  return soma / (binFinal - binInicial + 1);
}

// Executa a matemática pura da FFT e agrupa nas frequências geométricas desejadas
void processarFFTPura() {
  // Copia as amostras do buffer circular de forma segura e limpa para o array real da FFT
  portENTER_CRITICAL(&mtxBuffer);
  int amostrasDisponiveis = idxBuffer;
  for (int i = 0; i < SAMPLES; i++) {
    if (i < amostrasDisponiveis) {
      vReal[i] = (double)bufferAmostras[i];
    } else {
      vReal[i] = 0.0; // Se não houver amostras novas (áudio pausado), preenche com silêncio
    }
    vImag[i] = 0.0;
  }
  idxBuffer = 0; // Reseta o ponteiro para o próximo ciclo de captura
  portEXIT_CRITICAL(&mtxBuffer);

  // Se houver sinal, calcula o espectro real
  FFT.windowing(FFTWindow::Hamming, FFTDirection::Forward);
  FFT.compute(FFTDirection::Forward);
  FFT.complexToMagnitude();

  // Zera todo o pacote de saída para garantir o preenchimento (padding) dos 32 bytes fixos
  memset(fftOutput, 0, NUM_BARRAS);

  double valoresBarras[9];
  double maiorNoFrame = 0.0;

  // MAPEAMENTO DAS 9 BANDAS REAIS (Janelas baseadas em bins de ~86.1Hz)
  valoresBarras[0] = obterMediaBins(0, 1);    // Alvo: ~60 Hz   (Bins 0 a 1)
  valoresBarras[1] = obterMediaBins(2, 2);    // Alvo: ~120 Hz  (Bin 2)
  valoresBarras[2] = obterMediaBins(3, 4);    // Alvo: ~240 Hz  (Bins 3 a 4)
  valoresBarras[3] = obterMediaBins(5, 8);    // Alvo: ~480 Hz  (Bins 5 a 8)
  valoresBarras[4] = obterMediaBins(9, 16);   // Alvo: ~960 Hz  (Bins 9 a 16)
  valoresBarras[5] = obterMediaBins(17, 32);  // Alvo: ~2 kHz   (Bins 17 a 32)
  valoresBarras[6] = obterMediaBins(33, 65);  // Alvo: ~4 kHz   (Bins 33 to 65)
  valoresBarras[7] = obterMediaBins(66, 127); // Alvo: ~8 kHz   (Bins 66 a 127)
  valoresBarras[8] = obterMediaBins(128, 255);// Alvo: ~16 kHz  (Bins 128 a 255)

  // Encontra o maior pico para controle de ganho automático dinâmico
  for (int i = 0; i < 9; i++) {
    if (valoresBarras[i] > maiorNoFrame) maiorNoFrame = valoresBarras[i];
  }

  if (maiorNoFrame > picoMaximoGeral) {
    picoMaximoGeral = maiorNoFrame;
  } else {
    picoMaximoGeral *= 0.95; // Decaimento suave das barras
    if (picoMaximoGeral < 300.0) picoMaximoGeral = 300.0;
  }

  // Preenche apenas os 9 primeiros bytes do pacote serial, o resto (9 a 31) vai como 0 fixo
  for (int i = 0; i < 9; i++) {
    double norm = valoresBarras[i] / picoMaximoGeral;
    norm = constrain(norm, 0.0, 1.0);
    fftOutput[i] = (uint8_t)(norm * 255.0);
  }
}

void avrc_metadata_callback(uint8_t id, const uint8_t *text) {
  String conteudo = String((char*)text);
  if (id == ESP_AVRC_MD_ATTR_TITLE) {
    tituloMusica = (conteudo.length() > 0) ? conteudo : "--";
  } else if (id == ESP_AVRC_MD_ATTR_ARTIST) {
    nomeArtista = (conteudo.length() > 0) ? conteudo : "--";
  } else if (id == ESP_AVRC_MD_ATTR_PLAYING_TIME) {
    tempoTotalMs = conteudo.toInt();
  }
  pendingMetadataSend = true;
}

void avrc_rn_play_pos_callback(uint32_t play_pos) {
  tempoAtualMs = play_pos;
}

void avrc_rn_track_change_callback(uint8_t *id) {
  tempoAtualMs = 0;
  tempoTotalMs = 0;
  tituloMusica = "--";
  nomeArtista = "--";
  pendingMetadataSend = true;
}

void connection_state_changed(esp_a2d_connection_state_t state, void *ptr) {
  if (state == ESP_A2D_CONNECTION_STATE_CONNECTED) {
    bluetoothConectado = true;
  } else {
    bluetoothConectado = false;
    tituloMusica = "--";
    nomeArtista = "--";
    tempoAtualMs = 0;
    tempoTotalMs = 0;
    estadoAudioIntencional = false;
  }
  pendingMetadataSend = true;
}

void audio_state_changed(esp_a2d_audio_state_t state, void *ptr) {
  estadoAudioIntencional = (state == ESP_A2D_AUDIO_STATE_STARTED);
  pendingMetadataSend = true;
}

void avrc_rn_playstatus_callback(esp_avrc_playback_stat_t playback) {
  switch (playback) {
    case ESP_AVRC_PLAYBACK_STOPPED:
      estadoAudioIntencional = false;
      break;
    case ESP_AVRC_PLAYBACK_PLAYING:
      estadoAudioIntencional = true;
      break;
    case ESP_AVRC_PLAYBACK_PAUSED:
      estadoAudioIntencional = false;
      break;
    default:
      break;
  }
  pendingMetadataSend = true;
}

void processarSerialIn() {
  while (Serial1.available() >= 4) {
    if (Serial1.peek() != HEADER_0) { Serial1.read(); continue; }
    Serial1.read(); 
    if (Serial1.read() != HEADER_1) continue; 
    uint8_t type = Serial1.read();
    uint8_t len = Serial1.read();
    if (type == 0x03) { 
      while (Serial1.available() < len);
      ComandoSerial c;
      c.cmd = Serial1.read();
      c.param = (len > 1) ? Serial1.read() : 0;
      xQueueSend(filaComandos, &c, 0);
    }
  }
}

void processarFilaComandos() {
  ComandoSerial c;
  while (xQueueReceive(filaComandos, &c, 0) == pdTRUE) {
    switch (c.cmd) {
      case 1: // Previous
        a2dp_sink.previous();
        pendingMetadataSend = true;
        break;

      case 2: // Next
        a2dp_sink.next();
        pendingMetadataSend = true;
        break;

      case 3: // Play/Pause
        //estadoAudioIntencional = !estadoAudioIntencional;
        if (estadoAudioIntencional) {
          a2dp_sink.pause();
        } else {
          a2dp_sink.play();
        }
        pendingMetadataSend = true;
        break;

      case 4: // Vol +
        volumeAtual = (volumeAtual <= 119) ? volumeAtual + 8 : 127;
        a2dp_sink.set_volume(volumeAtual);
        break;

      case 5: // Vol -
        volumeAtual = (volumeAtual >= 8) ? volumeAtual - 8 : 0;
        a2dp_sink.set_volume(volumeAtual);
        break;

      case 6: // Set Volume Direto (param 0-127)
        volumeAtual = constrain(c.param, 0, 127);
        a2dp_sink.set_volume(volumeAtual);
        break;

      case 7: // Forçar Reconexão BT
        a2dp_sink.reconnect();
        break;
    }
  }
}

void setup() {
  Serial.begin(9600);
  Serial1.begin(115200, SERIAL_8N1, RX1_PIN, TX1_PIN);
  filaComandos = xQueueCreate(10, sizeof(ComandoSerial));
  
  i2s.setPins(I2S_SCK, I2S_WS, I2S_SDOUT);
  i2s.begin(I2S_MODE_STD, 44100, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO, I2S_STD_SLOT_BOTH);
  
  a2dp_sink.set_stream_reader(audio_data_stream, false);
  a2dp_sink.set_avrc_metadata_callback(avrc_metadata_callback);
  a2dp_sink.set_on_connection_state_changed(connection_state_changed);
  //a2dp_sink.set_on_audio_state_changed(audio_state_changed);
  a2dp_sink.set_avrc_rn_playstatus_callback(avrc_rn_playstatus_callback);

  // Solicita posição da reprodução ao BT a cada 1 segundo
  a2dp_sink.set_avrc_rn_play_pos_callback(avrc_rn_play_pos_callback);
  a2dp_sink.set_avrc_rn_track_change_callback(avrc_rn_track_change_callback);
  a2dp_sink.set_auto_reconnect(true, 10000);
  a2dp_sink.start("Volvo C30 Audio");
}


void loop() {
  processarSerialIn();
  processarFilaComandos();
  
  static unsigned long ultimoIncremento = 0;
  if (millis() - ultimoIncremento >= 1000) {
    ultimoIncremento = millis();
    if (estadoAudioIntencional) {
      tempoAtualMs += 1000;
    }
  }
  
  // --- CALCULO E ENVIO DESACOPLADO DA FFT (24 FPS / 41ms) ---
  static unsigned long ultimoFrameFFT = 0;
  if (millis() - ultimoFrameFFT >= 41) {
    ultimoFrameFFT = millis();
    
    // Processa a FFT lendo o buffer de som (ou o silêncio se estiver pausado/em chamada)
    processarFFTPura();
    
    // Envia o pacote de 32 bytes fixos com segurança via Serial1
    enviarFFTSerial();
  }
  
  static unsigned long ultimoEnvioMeta = 0;
  if (pendingMetadataSend || (millis() - ultimoEnvioMeta >= 1000)) {
    ultimoEnvioMeta = millis();
    enviarMetadadosSerial();
  }
  
  delay(1); // Mantém o cão de guarda (Watchdog) alimentado sem engasgar
}
