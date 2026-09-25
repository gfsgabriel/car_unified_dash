#include "boot_log.h"
#include "config.h"
#include <stdarg.h>

struct LogEntry {
  LogTipo  tipo;
  char     texto[48];
  uint32_t ms;
};

static LogEntry  buf[LOG_BUFFER_SIZE];
static int       count = 0;         // total de entradas já inseridas
static int       head  = 0;         // próxima posição

void boot_log_init() {
  count = 0;
  head = 0;
}

void boot_log_add(LogTipo tipo, const char* fmt, ...) {
  char tmp[48];
  va_list args;
  va_start(args, fmt);
  vsnprintf(tmp, sizeof(tmp), fmt, args);
  va_end(args);

  buf[head].tipo = tipo;
  strncpy(buf[head].texto, tmp, sizeof(buf[head].texto) - 1);
  buf[head].texto[sizeof(buf[head].texto) - 1] = '\0';
  buf[head].ms = millis();

  head = (head + 1) % LOG_BUFFER_SIZE;
  count++;
}

int boot_log_count()    { return count; }
int boot_log_capacity() { return LOG_BUFFER_SIZE; }

bool boot_log_get(int idx, LogTipo& tipo, String& texto, uint32_t& ms) {
  if (idx < 0 || idx >= count) return false;

  // Índice lógico → índice físico no ring buffer
  int mais_antigo = (count <= LOG_BUFFER_SIZE) ? 0 : head;
  int fisico = (mais_antigo + idx) % LOG_BUFFER_SIZE;

  tipo = buf[fisico].tipo;
  texto = String(buf[fisico].texto);
  ms = buf[fisico].ms;
  return true;
}

void boot_log_limpar() {
  count = 0;
  head = 0;
}