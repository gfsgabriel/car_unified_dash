#ifndef CORE_UART_ROUTER_H
#define CORE_UART_ROUTER_H

#include <Arduino.h>

struct WifiStatusMini {
  bool     valido;
  uint8_t  estado;    // 0=idle 1=conn 2=ok 3=fail
  char     ssid[33];
  char     ip[16];
  int8_t   rssi;
};

#define OBD_MAX_PIDS 12
struct ObdPidStatus {
  char  cmd[4];
  char  nome[12];
  float valor;
  bool  suportado;
  bool  respondendo;
};
struct ObdStatusGeral {
  bool         elmResponde;
  uint8_t      estadoGeral;
  uint8_t      total;
  uint8_t      respondendo;
  ObdPidStatus pids[OBD_MAX_PIDS];
};

void uart_router_init();
void uart_router_loop();

void uart_send_wifi_query();
void uart_send_wifi_connect(const String& ssid, const String& pass);
void uart_send_wifi_retry();           // conecta na última salva (ssid vazio)
void uart_send_btn(uint8_t id, uint8_t estado);
void uart_send_media(uint8_t id);
void uart_send_obd_reob();
void uart_send_at(const String& cmd, const String& term, uint32_t timeout);

WifiStatusMini  uart_get_wifi_status();
ObdStatusGeral  uart_get_obd_status();
bool            uart_get_debug_linha(String& out);

uint32_t uart_cont_fft();
uint32_t uart_cont_music();
uint32_t uart_cont_tele();
uint32_t uart_cont_obdstatus();

#endif