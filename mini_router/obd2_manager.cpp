#include "obd2_manager.h"
#include "bt_manager.h"
#include <Arduino.h>
#include <Preferences.h>

static Preferences prefs;
static const uint8_t TOTAL_PIDS = 8;

struct PIDConfig {
  const char* cmd;
  const char* nome;
  uint8_t prioridade;
  uint32_t timeout;
  uint8_t dataLen;
  float valorDefault;
  float* destino;
  bool suportado;
  bool respondendo;
  float ultimoValor;
};

static PIDConfig pids[TOTAL_PIDS] = {
  { "010C", "RPM", 0, 500, 2, 0.0, &telemetria.rpm, false, false, 0.0 },
  { "0110", "MAF", 0, 500, 2, 4.5, &telemetria.maf, false, false, 0.0 },
  { "010D", "Velocidade", 1, 500, 1, 0.0, &telemetria.velocidade, false, false, 0.0 },
  { "0111", "TPS", 1, 500, 1, 0.0, &telemetria.tps, false, false, 0.0 },
  { "0149", "Pedal", 1, 500, 1, 0.0, &telemetria.pedal, false, false, 0.0 },
  { "010F", "IAT", 2, 2000, 1, 20.0, &telemetria.tempIntake, false, false, 0.0 },
  { "0105", "Coolant", 2, 2000, 1, 80.0, &telemetria.tempCoolant, false, false, 0.0 },
  { "0142", "Bateria", 2, 2000, 2, 13.8, &telemetria.bateria, false, false, 0.0 },
};

static uint32_t pidsSuportados[6] = { 0 };

struct PIDEstado {
  int timeouts;
  bool disponivel;
};
static PIDEstado estadoPIDs[TOTAL_PIDS];

static const char* initCmds[] = {
  "ATZ", "ATE0", "ATL0", "ATH0", "ATSP0",
  "0100", "0120", "0140"
};
static const uint32_t initTimeouts[] = {
  5000, 1000, 1000, 1000, 5000,
  2000, 2000, 2000
};
static const uint8_t TOTAL_INIT = 8;

static uint8_t initEtapa = 0;
static uint32_t idInitPendente = 0;
static unsigned long inicioInit = 0;
static bool initFeito = false;
static unsigned long ultimaTentativaATZ = 0;
static const unsigned long INTERVALO_ATZ_MS = 2000;

// ---------------------------------------------------------------
// Warm-up (0100 retry até ECU responder)
// ---------------------------------------------------------------
static bool warmupOk = false;
static uint32_t idWarmup = 0;
static unsigned long inicioWarmup = 0;
static unsigned long ultimaTentativaWarmup = 0;
static const unsigned long INTERVALO_WARMUP_MS = 1000;

// ---------------------------------------------------------------
// Batch auto-detect
// ---------------------------------------------------------------
static uint8_t batchSize = 6;
static uint8_t batchTestN = 6;
static uint32_t idBatchTest = 0;
static unsigned long inicioBatchTest = 0;
static int listaBatch[6];
static uint8_t nListaBatch = 0;

// ---------------------------------------------------------------
// Scheduler (H/M/L)
// ---------------------------------------------------------------
static uint8_t pidsPendentesMulti[6];
static uint8_t nPidsPendentesMulti = 0;

static uint8_t rotHigh = 0;
static uint8_t rotMed = 0;
static uint8_t rotLow = 0;

static uint8_t slotRotativo = 0;
static uint8_t singleCounter = 0;

static uint32_t idPendente = 0;
static unsigned long inicioPendente = 0;
static bool aguardandoResp = false;
static bool ultimoFoiMulti = false;

// ---------------------------------------------------------------
// Cálculo / tempo
// ---------------------------------------------------------------
static uint32_t ultimoTempoMicros = 0;
static double acumuladorMililitros = 0.0;
static uint32_t tempoInicioZeroCem = 0;
static bool cronometroRodando = false;

static unsigned long proximoCheckSuporte = 0;
static const unsigned long GRACE_INICIAL_MS = 30000;
static const unsigned long INTERVALO_CHECK_MS = 10000;

// ---------------------------------------------------------------
// FPS
// ---------------------------------------------------------------
static uint32_t contadorCiclosFPS = 0;
static unsigned long janelaFPSInicio = 0;
static float fpsTelemetria = 0.0f;
volatile uint32_t obdContadorAmostras = 0;

// ---------------------------------------------------------------
static bool isHexChar(char c) {
  return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f');
}

// ---------------------------------------------------------------
// ★ Limpa resposta do ELM327 (char-by-char)
//   - Pula espaços, tabs, \r, \n
//   - Prefixo "N:" (1-2 hex + ':') só no INÍCIO da linha
//   - Hex válido → acumula
//   - Não-hex → descarta a linha inteira (STOPPED, SEARCHING, >)
// ---------------------------------------------------------------
static String limparRespostaELM(const String& resp) {
  String out = "";
  out.reserve(resp.length());

  int n = resp.length();
  int i = 0;
  bool inicioLinha = true;

  while (i < n) {
    char c = resp[i];

    if (c == ' ' || c == '\t') {
      i++;
      continue;
    }
    if (c == '\r' || c == '\n') {
      inicioLinha = true;
      i++;
      continue;
    }

    if (inicioLinha && isHexChar(c)) {
      int j = i;
      while (j < n && isHexChar(resp[j]) && (j - i) < 2) j++;
      if (j > i && j < n && resp[j] == ':') {
        i = j + 1;
        inicioLinha = false;
        continue;
      }
    }

    inicioLinha = false;

    if (isHexChar(c)) {
      out += (char)toupper(c);
      i++;
      continue;
    }

    while (i < n && resp[i] != '\r' && resp[i] != '\n') i++;
  }

  // ★ Ajusta posição do "41"
  //   - Single-line: começa com "41"
  //   - Multi-line: "XXX41..." (3 hex header + "41")
  if (out.length() >= 2 && !out.startsWith("41")) {
    bool headerOk = (out.length() >= 5) && isHexChar(out[0]) && isHexChar(out[1]) && isHexChar(out[2]);
    if (headerOk && out.substring(3, 5) == "41") {
      out = out.substring(3);  // corta só o header de 3
    } else {
      // Fallback: primeiro "41" (caso raro)
      int idx = out.indexOf("41");
      if (idx >= 0) out = out.substring(idx);
      else out = "";
    }
  }

  return out;
}

static float decodificarPID(int idxPid, uint8_t* bytes, int n) {
  if (idxPid < 0 || idxPid >= TOTAL_PIDS) return NAN;
  if (bytes == NULL || n <= 0) return NAN;

  const char* cmd = pids[idxPid].cmd;

  if (strcmp(cmd, "010C") == 0 && n >= 2)
    return ((bytes[0] * 256.0) + bytes[1]) / 4.0;
  if (strcmp(cmd, "010D") == 0 && n >= 1)
    return (float)bytes[0];
  if (strcmp(cmd, "0110") == 0 && n >= 2)
    return ((bytes[0] * 256.0) + bytes[1]) / 100.0;
  if (strcmp(cmd, "0111") == 0 && n >= 1)
    return (bytes[0] * 100.0) / 255.0;
  if (strcmp(cmd, "0149") == 0 && n >= 1)
    return (bytes[0] * 100.0) / 255.0;
  if (strcmp(cmd, "010F") == 0 && n >= 1)
    return (float)bytes[0] - 40.0;
  if (strcmp(cmd, "0105") == 0 && n >= 1)
    return (float)bytes[0] - 40.0;
  if (strcmp(cmd, "0142") == 0 && n >= 2)
    return ((bytes[0] * 256.0) + bytes[1]) / 1000.0;
  if (strcmp(cmd, "0149") == 0 && n >= 1)
    return (bytes[0] * 200.0) / 255.0;   // ★ ×2 (D é metade de E)

  return NAN;
}

static bool parseHexBytes(const String& resp, const char* pidEsperado,
                          uint8_t* bytes, int* numBytes, int maxBytes) {
  if (numBytes) *numBytes = 0;
  if (bytes == NULL || numBytes == NULL) return false;
  if (maxBytes <= 0 || resp.length() == 0) return false;
  if (pidEsperado == NULL || strlen(pidEsperado) < 4) return false;

  String s = limparRespostaELM(resp);
  String pidEsp = String(pidEsperado).substring(2, 4);
  String prefixo = "41" + pidEsp;

  if (s.startsWith(prefixo)) {
    int b = 4;
    while (b + 1 < (int)s.length() && *numBytes < maxBytes) {
      char c1 = s[b];
      char c2 = s[b + 1];
      if (!isHexChar(c1) || !isHexChar(c2)) break;
      String hb = s.substring(b, b + 2);
      bytes[*numBytes] = (uint8_t)strtol(hb.c_str(), NULL, 16);
      (*numBytes)++;
      b += 2;
    }
    return (*numBytes > 0);
  }
  return false;
}

static uint8_t parseMultiPid(const String& resp) {
  String s = limparRespostaELM(resp);
  if (!s.startsWith("41")) return 0;
  s = s.substring(2);

  uint8_t okCount = 0;
  uint8_t ptr = 0;

  while (ptr < nPidsPendentesMulti && s.length() >= 2) {
    uint8_t idx = pidsPendentesMulti[ptr];
    String pidEsperado = String(pids[idx].cmd).substring(2, 4);
    String pidRecebido = s.substring(0, 2);

    if (pidRecebido.equalsIgnoreCase(pidEsperado)) {
      s = s.substring(2);
      uint8_t dl = pids[idx].dataLen;
      if (s.length() < dl * 2) break;

      uint8_t bytes[4];
      for (uint8_t i = 0; i < dl; i++) {
        String hb = s.substring(0, 2);
        s = s.substring(2);
        bytes[i] = (uint8_t)strtol(hb.c_str(), NULL, 16);
      }

      float val = decodificarPID(idx, bytes, dl);
      if (!isnan(val) && isfinite(val)) {
        if (!telemetria.modoExterno) *pids[idx].destino = val;
        pids[idx].ultimoValor = val;
        pids[idx].respondendo = true;
        estadoPIDs[idx].timeouts = 0;
        okCount++;
      }
      ptr++;
    } else {
      pids[idx].respondendo = false;
      estadoPIDs[idx].timeouts++;
      if (estadoPIDs[idx].timeouts >= 3) {
        estadoPIDs[idx].disponivel = false;
        if (!telemetria.modoExterno) *pids[idx].destino = pids[idx].valorDefault;
      }
      ptr++;
    }
  }

  while (ptr < nPidsPendentesMulti) {
    uint8_t idx = pidsPendentesMulti[ptr];
    pids[idx].respondendo = false;
    estadoPIDs[idx].timeouts++;
    if (estadoPIDs[idx].timeouts >= 3) {
      estadoPIDs[idx].disponivel = false;
      if (!telemetria.modoExterno) *pids[idx].destino = pids[idx].valorDefault;
    }
    ptr++;
  }

  return okCount;
}

static void parsearNegociacao(int bloco, const String& resp) {
  if (bloco < 0 || bloco >= 6) return;
  uint8_t bytes[8];
  int n = 0;
  char cmdEsperado[8];
  snprintf(cmdEsperado, sizeof(cmdEsperado), "01%02X", bloco * 0x20);
  if (!parseHexBytes(resp, cmdEsperado, bytes, &n, 8)) return;
  if (n < 4) return;
  uint32_t mask = ((uint32_t)bytes[0] << 24) | ((uint32_t)bytes[1] << 16) | ((uint32_t)bytes[2] << 8) | ((uint32_t)bytes[3]);
  pidsSuportados[bloco] = mask;
  Serial.printf("OBD: bloco %d suportados = 0x%08X\n", bloco, mask);
}

static void aplicarNegociacao() {
  for (uint8_t i = 0; i < TOTAL_PIDS; i++) {
    uint8_t pidNum = strtol(pids[i].cmd + 2, NULL, 16);
    uint8_t bloco = pidNum / 0x20;
    uint8_t bitPos = pidNum % 0x20;
    if (bloco >= 6) {
      pids[i].suportado = false;
      continue;
    }
    uint8_t shift = (bitPos == 0) ? 0 : (0x20 - bitPos);
    pids[i].suportado = (pidsSuportados[bloco] >> shift) & 1;
    Serial.printf("OBD: PID %s (%s) %s\n",
                  pids[i].cmd, pids[i].nome,
                  pids[i].suportado ? "SUPORTADO" : "NAO SUPORTADO");
  }
}

static uint8_t contarPidsPrio(uint8_t prio) {
  uint8_t n = 0;
  for (uint8_t j = 0; j < TOTAL_PIDS; j++) {
    if (pids[j].prioridade == prio && estadoPIDs[j].disponivel && pids[j].suportado) n++;
  }
  return n;
}

static bool jaNoBatch(int idx) {
  for (uint8_t i = 0; i < nPidsPendentesMulti; i++)
    if (pidsPendentesMulti[i] == idx) return true;
  return false;
}

static int proximoPIDPrioridadeUnique(uint8_t prio, uint8_t* rot) {
  for (uint8_t tent = 0; tent < TOTAL_PIDS; tent++) {
    uint8_t cur = (*rot + tent) % TOTAL_PIDS;
    if (pids[cur].prioridade != prio) continue;
    if (!estadoPIDs[cur].disponivel) continue;
    if (!pids[cur].suportado) continue;
    if (jaNoBatch(cur)) continue;
    *rot = (cur + 1) % TOTAL_PIDS;
    return cur;
  }
  return -1;
}

static int proximoPIDRotativo() {
  uint8_t pref = (slotRotativo == 0) ? 1 : 2;
  uint8_t other = (pref == 1) ? 2 : 1;

  int idx = proximoPIDPrioridadeUnique(pref, pref == 1 ? &rotMed : &rotLow);
  if (idx < 0) {
    idx = proximoPIDPrioridadeUnique(other, other == 1 ? &rotMed : &rotLow);
  }

  slotRotativo = (slotRotativo == 0) ? 1 : 0;
  return idx;
}

static uint8_t montarListaTeste(uint8_t n, int* lista) {
  uint8_t count = 0;
  for (uint8_t prio = 0; prio < 3 && count < n; prio++) {
    for (uint8_t i = 0; i < TOTAL_PIDS && count < n; i++) {
      if (pids[i].prioridade != prio) continue;
      if (!pids[i].suportado) continue;
      lista[count++] = i;
    }
  }
  return count;
}

static bool respostaCompleta(const String& resp, uint8_t n) {
  String s = limparRespostaELM(resp);
  if (!s.startsWith("41")) return false;
  for (uint8_t i = 0; i < n; i++) {
    String pidHex = String(pids[listaBatch[i]].cmd).substring(2, 4);
    if (s.indexOf(pidHex) < 0) return false;
  }
  return true;
}

// ---------------------------------------------------------------
// Init + Warm-up + Batch test
// ---------------------------------------------------------------
static void rodarInitOBD() {

  // ============ FASE 1: init AT ============
  if (initEtapa < TOTAL_INIT) {
    if (initEtapa == 0 && idInitPendente == 0) {
      if (millis() - ultimaTentativaATZ < INTERVALO_ATZ_MS) return;
    }

    if (idInitPendente != 0) {
      String resp;
      bool ok;
      if (btObterRespostaPublico(idInitPendente, resp, ok, DONO_OBD)) {
        idInitPendente = 0;
        if (!ok) {
          Serial.printf("OBD: init [%d/%d] %s FALHOU\n",
                        initEtapa + 1, TOTAL_INIT, initCmds[initEtapa]);
          initEtapa = 0;
          ultimaTentativaATZ = millis();
          return;
        }
        Serial.printf("OBD: init [%d/%d] %s OK\n",
                      initEtapa + 1, TOTAL_INIT, initCmds[initEtapa]);
        if (initEtapa >= 5 && initEtapa <= 7) {
          parsearNegociacao(initEtapa - 5, resp);
        }
        initEtapa++;
        if (initEtapa >= TOTAL_INIT) {
          aplicarNegociacao();
          warmupOk = false;
          idWarmup = 0;
          ultimaTentativaWarmup = 0;
          batchTestN = 6;
          idBatchTest = 0;
        }
        return;
      }
      if (millis() - inicioInit >= initTimeouts[initEtapa] + 2000) {
        Serial.printf("OBD: init TIMEOUT em %s\n", initCmds[initEtapa]);
        initEtapa = 0;
        idInitPendente = 0;
        ultimaTentativaATZ = millis();
      }
      return;
    }

    Serial.printf("OBD: init [%d/%d] %s\n",
                  initEtapa + 1, TOTAL_INIT, initCmds[initEtapa]);
    idInitPendente = btEnviarATPublico(initCmds[initEtapa], ">",
                                       initTimeouts[initEtapa], DONO_OBD);
    if (idInitPendente == 0) return;
    inicioInit = millis();
    if (initEtapa == 0) ultimaTentativaATZ = millis();
    return;
  }

  // ============ FASE 2: warm-up (0100 retry) ============
  if (!warmupOk) {
    // Comando em voo?
    if (idWarmup != 0) {
      String resp;
      bool ok;
      if (btObterRespostaPublico(idWarmup, resp, ok, DONO_OBD)) {
        idWarmup = 0;
        if (ok) {
          String limpo = limparRespostaELM(resp);
          if (limpo.startsWith("4100")) {
            Serial.println("OBD: warmup OK");
            warmupOk = true;
            batchTestN = 6;
            idBatchTest = 0;
            return;
          }
        }
        Serial.printf("OBD: warmup resp ruim [%.40s]\n", resp.c_str());
        return;
      }
      if (millis() - inicioWarmup >= 3000) {
        Serial.println("OBD: warmup timeout, retry");
        idWarmup = 0;
        return;
      }
      return;
    }

    // Cooldown antes de mandar de novo
    if (millis() - ultimaTentativaWarmup < INTERVALO_WARMUP_MS) return;
    ultimaTentativaWarmup = millis();

    Serial.println("OBD: warmup (0100)");
    idWarmup = btEnviarATPublico("0100", ">", 3000, DONO_OBD);
    if (idWarmup == 0) return;
    inicioWarmup = millis();
    return;
  }

  // ============ FASE 3: batch test 6→1 ============
  if (batchTestN == 0) {
    // Testou tudo, fallback
    batchSize = 1;
    Serial.println("OBD: batch size = 1 (fallback)");
    initFeito = true;
    proximoCheckSuporte = millis() + GRACE_INICIAL_MS;
    return;
  }

  // Comando em voo?
  if (idBatchTest != 0) {
    String resp;
    bool ok;
    if (btObterRespostaPublico(idBatchTest, resp, ok, DONO_OBD)) {
      idBatchTest = 0;
      if (ok && respostaCompleta(resp, batchTestN)) {
        batchSize = batchTestN;
        Serial.printf("OBD: batch size = %d OK\n", batchSize);
        initFeito = true;
        proximoCheckSuporte = millis() + GRACE_INICIAL_MS;
        return;
      }
      Serial.printf("OBD: batch=%d falhou\n", batchTestN);
      batchTestN--;
      return;
    }
    if (millis() - inicioBatchTest >= 5000) {
      Serial.printf("OBD: batch=%d timeout\n", batchTestN);
      idBatchTest = 0;
      batchTestN--;
    }
    return;
  }

  // Monta e envia próximo teste
  nListaBatch = montarListaTeste(batchTestN, listaBatch);
  if (nListaBatch < batchTestN) {
    batchTestN--;
    return;
  }

  String cmd = "01";
  for (uint8_t i = 0; i < batchTestN; i++) {
    cmd += String(pids[listaBatch[i]].cmd).substring(2);
  }

  Serial.printf("OBD: testando batch=%d\n", batchTestN);
  idBatchTest = btEnviarATPublico(cmd, ">", 5000, DONO_OBD);
  if (idBatchTest == 0) {
    batchTestN--;
    return;
  }
  inicioBatchTest = millis();
}

// ---------------------------------------------------------------
// Cálculo derivado
// ---------------------------------------------------------------
static void onUpdateCiclo() {
  obdContadorAmostras++;

  contadorCiclosFPS++;
  unsigned long agoraMs = millis();
  if (agoraMs - janelaFPSInicio >= 1000) {
    fpsTelemetria = contadorCiclosFPS * 1000.0f / (agoraMs - janelaFPSInicio);
    contadorCiclosFPS = 0;
    janelaFPSInicio = agoraMs;
  }

  if (telemetria.maf > 0.0) {
    double g_s = (double)telemetria.maf / 14.7;
    double l_s = g_s / 740.0;
    telemetria.consumo_ml_min = (float)(l_s * 1000.0 * 60.0);
  }

  if (telemetria.velocidade > 5.0) {
    telemetria.consumo_l_100km = (telemetria.consumo_ml_min * 6.0) / telemetria.velocidade;
  } else {
    telemetria.consumo_l_100km = 99.9;
  }

  uint32_t agora = micros();
  uint32_t deltaMicros = agora - ultimoTempoMicros;
  ultimoTempoMicros = agora;
  if (deltaMicros <= 2000000) {
    if (deltaMicros > 1000000) deltaMicros = 1000000;
    acumuladorMililitros += ((double)telemetria.consumo_ml_min / 60000000.0) * deltaMicros;
  }
  telemetria.consumo_total_litros = (float)(acumuladorMililitros / 1000.0);

  float iat = (telemetria.tempIntake > -30.0) ? telemetria.tempIntake : 20.0;

  if (telemetria.maf > 0.0 && telemetria.motor_litros > 0.0 && telemetria.eficiencia_ve > 0.0) {
    double maf_kg_s = telemetria.maf / 1000.0;
    double temp_k = iat + 273.15;
    double rpm_lim = (telemetria.rpm > 600.0) ? telemetria.rpm : 600.0;
    double desc_m3 = (double)telemetria.motor_litros / 1000.0;
    double num = maf_kg_s * 287.0 * temp_k;
    double den = telemetria.eficiencia_ve * desc_m3 * (rpm_lim / 120.0);
    if (den > 0.0) {
      double pa = (num / den) - 101325.0;
      telemetria.boost = (float)(pa * 0.000145038);
      if (telemetria.boost > telemetria.boost_max)
        telemetria.boost_max = telemetria.boost;
    }
  }

  if (telemetria.velocidade <= 0.1) {
    cronometroRodando = false;
  } else if (telemetria.velocidade > 0.5 && !cronometroRodando && telemetria.velocidade < 100.0) {
    tempoInicioZeroCem = millis();
    cronometroRodando = true;
  } else if (cronometroRodando && telemetria.velocidade >= 100.0) {
    telemetria.zeroCemUltimo = (float)(millis() - tempoInicioZeroCem) / 1000.0;
    cronometroRodando = false;
  }
}

void inicializarOBD2() {
  prefs.begin("ajustes", false);
  float motorSalvo = prefs.getFloat("motor", 2.5f);
  float veSalva = prefs.getFloat("ve", 0.85f);

  telemetria = { 0 };
  telemetria.boost = -15.0;
  telemetria.bateria = 13.8;
  telemetria.tempCoolant = 0.0;
  telemetria.tempIntake = 0.0;
  telemetria.maf = 0.0;
  telemetria.motor_litros = motorSalvo;
  telemetria.eficiencia_ve = veSalva;
  telemetria.modoExterno = false;
  telemetria.btConectado = false;
  telemetria.obdConectado = false;

  for (uint8_t i = 0; i < TOTAL_PIDS; i++) {
    estadoPIDs[i].timeouts = 0;
    estadoPIDs[i].disponivel = true;
    pids[i].suportado = false;
    pids[i].respondendo = false;
    pids[i].ultimoValor = 0.0;
  }

  memset(pidsSuportados, 0, sizeof(pidsSuportados));
  ultimoTempoMicros = micros();
  acumuladorMililitros = 0.0;
  rotHigh = rotMed = rotLow = 0;
  slotRotativo = 0;
  singleCounter = 0;
  contadorCiclosFPS = 0;
  janelaFPSInicio = millis();
  fpsTelemetria = 0.0f;
  obdContadorAmostras = 0;
  batchSize = 6;

  Serial.println("OBD: init");
}

// ---------------------------------------------------------------
// Scheduler de batch
// ---------------------------------------------------------------
static bool enviarBatch() {
  String cmd = "01";
  nPidsPendentesMulti = 0;

  if (batchSize <= 1) {
    int idx = -1;
    if (singleCounter == 2) {
      idx = proximoPIDRotativo();
    } else {
      idx = proximoPIDPrioridadeUnique(0, &rotHigh);
    }
    singleCounter = (singleCounter + 1) % 3;

    if (idx < 0) idx = proximoPIDPrioridadeUnique(0, &rotHigh);
    if (idx < 0) idx = proximoPIDRotativo();
    if (idx < 0) return false;

    cmd += String(pids[idx].cmd).substring(2);
    pidsPendentesMulti[0] = idx;
    nPidsPendentesMulti = 1;
    ultimoFoiMulti = false;

  } else {
    uint8_t n0 = contarPidsPrio(0);

    uint8_t slotsHigh = (batchSize + 1) / 2;
    if (slotsHigh > n0) slotsHigh = n0;
    if (slotsHigh == batchSize && batchSize > 1) slotsHigh--;

    uint8_t slotsRestantes = batchSize - slotsHigh;

    for (uint8_t i = 0; i < slotsHigh; i++) {
      int idx = proximoPIDPrioridadeUnique(0, &rotHigh);
      if (idx < 0) break;
      cmd += String(pids[idx].cmd).substring(2);
      pidsPendentesMulti[nPidsPendentesMulti++] = idx;
    }

    for (uint8_t i = 0; i < slotsRestantes; i++) {
      int idx = proximoPIDRotativo();
      if (idx < 0) break;
      cmd += String(pids[idx].cmd).substring(2);
      pidsPendentesMulti[nPidsPendentesMulti++] = idx;
    }

    while (nPidsPendentesMulti < batchSize) {
      int idx = proximoPIDPrioridadeUnique(0, &rotHigh);
      if (idx < 0) idx = proximoPIDPrioridadeUnique(1, &rotMed);
      if (idx < 0) idx = proximoPIDPrioridadeUnique(2, &rotLow);
      if (idx < 0) break;
      cmd += String(pids[idx].cmd).substring(2);
      pidsPendentesMulti[nPidsPendentesMulti++] = idx;
    }

    if (nPidsPendentesMulti == 0) return false;
    ultimoFoiMulti = (nPidsPendentesMulti > 1);
  }

  idPendente = btEnviarATPublico(cmd, ">", 2000, DONO_OBD);
  if (idPendente == 0) return false;

  inicioPendente = millis();
  aguardandoResp = true;
  return true;
}

void atualizarDadosOBD2() {
  telemetria.btConectado = false;
  telemetria.obdConectado = false;

  if (!initFeito) {
    rodarInitOBD();
    return;
  }

  telemetria.btConectado = true;
  telemetria.obdConectado = true;

  bool externo = telemetria.modoExterno;

  if (aguardandoResp) {
    String resp;
    bool ok;

    if (btObterRespostaPublico(idPendente, resp, ok, DONO_OBD)) {
      aguardandoResp = false;

      if (!ok || resp.indexOf("NO DATA") >= 0 || resp.length() == 0) {
        for (uint8_t i = 0; i < nPidsPendentesMulti; i++) {
          uint8_t idx = pidsPendentesMulti[i];
          pids[idx].respondendo = false;
          estadoPIDs[idx].timeouts++;
          if (estadoPIDs[idx].timeouts >= 3) {
            estadoPIDs[idx].disponivel = false;
            if (!externo) *pids[idx].destino = pids[idx].valorDefault;
          }
        }
      } else {
        if (ultimoFoiMulti) {
          parseMultiPid(resp);
        } else {
          uint8_t idx = pidsPendentesMulti[0];
          uint8_t bytes[8];
          int n = 0;
          if (parseHexBytes(resp, pids[idx].cmd, bytes, &n, 8)) {
            float val = decodificarPID(idx, bytes, n);
            if (!isnan(val) && isfinite(val)) {
              if (!externo) *pids[idx].destino = val;
              pids[idx].ultimoValor = val;
              pids[idx].respondendo = true;
              estadoPIDs[idx].timeouts = 0;
            }
          }
        }
      }

      bool temHigh = false;
      for (uint8_t i = 0; i < nPidsPendentesMulti; i++) {
        if (pids[pidsPendentesMulti[i]].prioridade == 0) {
          temHigh = true;
          break;
        }
      }
      if (temHigh && !externo) onUpdateCiclo();

      nPidsPendentesMulti = 0;
    } else if (millis() - inicioPendente >= 3000) {
      aguardandoResp = false;
      for (uint8_t i = 0; i < nPidsPendentesMulti; i++) {
        uint8_t idx = pidsPendentesMulti[i];
        pids[idx].respondendo = false;
        estadoPIDs[idx].timeouts++;
        if (estadoPIDs[idx].timeouts >= 3) {
          estadoPIDs[idx].disponivel = false;
          if (!externo) *pids[idx].destino = pids[idx].valorDefault;
        }
      }
      nPidsPendentesMulti = 0;
    } else {
      return;
    }
  }

  enviarBatch();

  if (proximoCheckSuporte != 0 && millis() >= proximoCheckSuporte) {
    proximoCheckSuporte = millis() + INTERVALO_CHECK_MS;
    int ativos = 0, respondendo = 0;
    for (uint8_t i = 0; i < TOTAL_PIDS; i++) {
      if (pids[i].suportado) {
        ativos++;
        if (pids[i].respondendo) respondendo++;
      }
    }
    Serial.printf("OBD: check suporte — %d/%d respondendo (batch=%d)\n",
                  respondendo, ativos, batchSize);
    if (ativos > 0 && respondendo < (ativos / 2)) {
      Serial.println("OBD: poucos PIDs respondendo — RE-NEGOCIANDO");
      obdForcarRenegociacao();
    }
  }
}

void obdForcarRenegociacao() {
  Serial.println("OBD: re-negociacao FORCADA");

  initFeito = false;
  initEtapa = 0;
  idInitPendente = 0;
  aguardandoResp = false;
  proximoCheckSuporte = 0;
  nPidsPendentesMulti = 0;

  warmupOk = false;
  idWarmup = 0;
  ultimaTentativaWarmup = 0;
  batchTestN = 6;
  idBatchTest = 0;

  rotHigh = rotMed = rotLow = 0;
  slotRotativo = 0;
  singleCounter = 0;

  for (uint8_t i = 0; i < TOTAL_PIDS; i++) {
    estadoPIDs[i].timeouts = 0;
    estadoPIDs[i].disponivel = true;
    pids[i].respondendo = false;
    pids[i].ultimoValor = 0.0;
  }

  memset(pidsSuportados, 0, sizeof(pidsSuportados));
}

uint8_t obdObterEstadoPIDs() {
  int down = 0, ativos = 0;
  for (uint8_t i = 0; i < TOTAL_PIDS; i++) {
    if (pids[i].suportado) ativos++;
    if (pids[i].suportado && !estadoPIDs[i].disponivel) down++;
  }
  if (ativos == 0) return 2;
  if (down == 0) return 0;
  if (down < ativos) return 1;
  return 2;
}

bool obdELMResponde() {
  return initFeito;
}

PIDView obterPIDView(int idx) {
  PIDView v = { "---", "---", 0.0, false, false };
  if (idx < 0 || idx >= TOTAL_PIDS) return v;
  v.cmd = pids[idx].cmd;
  v.nome = pids[idx].nome;
  v.ultimoValor = pids[idx].ultimoValor;
  v.suportado = pids[idx].suportado;
  v.respondendo = pids[idx].respondendo;
  return v;
}

int obterTotalPIDs() {
  return TOTAL_PIDS;
}

void salvarAjusteMotorFlash(float novoTamanhoLitros, float novaEficienciaVE) {
  telemetria.motor_litros = novoTamanhoLitros;
  telemetria.eficiencia_ve = novaEficienciaVE;
  prefs.putFloat("motor", novoTamanhoLitros);
  prefs.putFloat("ve", novaEficienciaVE);
}

void obdZerarAcumulador() {
  acumuladorMililitros = 0.0;
  telemetria.consumo_total_litros = 0.0;
  ultimoTempoMicros = micros();
  Serial.println("OBD: acumulador zerado");
}

float obdObterFPS() {
  return fpsTelemetria;
}