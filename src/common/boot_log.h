#ifndef COMMON_BOOT_LOG_H
#define COMMON_BOOT_LOG_H

#include "src/common/tipos.h"

void boot_log_init();
void boot_log_add(LogTipo tipo, const char* fmt, ...);
int  boot_log_count();
int  boot_log_capacity();
bool boot_log_get(int idx, LogTipo& tipo, String& texto, uint32_t& ms);
void boot_log_limpar();

#endif