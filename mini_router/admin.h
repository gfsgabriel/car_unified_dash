#ifndef ADMIN_H
#define ADMIN_H
#include <Arduino.h>

const char ADMIN_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="pt-BR">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>Admin Mini</title>
<style>
body { font-family: Arial, sans-serif; background: #121212; color: #fff; padding: 20px; margin: 0; }
.box { max-width: 500px; margin: 0 auto; background: #1e1e1e; padding: 20px; border-radius: 8px; border: 1px solid #333; }
h2 { color: #ffaa00; margin-top: 0; border-bottom: 1px solid #333; padding-bottom: 8px; }
h3 { color: #ccc; font-size: 1em; margin-top: 0; }
.card { background: #262626; padding: 15px; border-radius: 6px; margin-bottom: 15px; border: 1px solid #444; }
.row { margin-bottom: 12px; }
label { display: block; font-weight: bold; margin-bottom: 6px; color: #ccc; font-size: 0.9em; }
input[type="range"] { width: 100%; cursor: pointer; }
input[type="number"] { width: 100%; padding: 8px; background: #333; border: 1px solid #555; color: #fff; border-radius: 4px; box-sizing: border-box; }
button { background: #008cba; color: white; padding: 12px; border: none; border-radius: 4px; cursor: pointer; font-weight: bold; width: 100%; font-size: 1em; }
button.active { background: #d9534f; }
.status { font-size: 0.85em; color: #888; margin-top: 8px; text-align: center; }
.status.ok { color: #00ff66; }
.status.err { color: #ff4444; }
.val { color: #00ff66; font-family: monospace; }
</style>
</head>
<body>
<div class="box">
  <h2>Admin Mini</h2>

  <div class="card">
    <button id="modeBtn" onclick="toggleSim()">Modo: OBD2 (Normal)</button>
    <div class="status" id="simStatus">Aguardando...</div>
  </div>

  <div class="card" id="simBox" style="opacity: 0.3; pointer-events: none; transition: opacity 0.3s;">
    <h3>Injetor</h3>

    <div class="row"><label>RPM: <span class="val" id="vRpm">0</span></label>
      <input type="range" id="rRpm" min="0" max="8000" value="0" step="50" oninput="document.getElementById('vRpm').innerText=this.value; injetar()"></div>
    <div class="row"><label>Velocidade: <span class="val" id="vVel">0</span> km/h</label>
      <input type="range" id="rVel" min="0" max="250" value="0" step="1" oninput="document.getElementById('vVel').innerText=this.value; injetar()"></div>
    <div class="row"><label>MAF: <span class="val" id="vMaf">4.5</span> g/s</label>
      <input type="range" id="rMaf" min="0" max="240" step="0.5" value="4.5" oninput="document.getElementById('vMaf').innerText=parseFloat(this.value).toFixed(1); injetar()"></div>
    <div class="row"><label>Boost: <span class="val" id="vBoost">-15.0</span> psi</label>
      <input type="range" id="rBoost" min="-15" max="30" step="0.1" value="-15" oninput="document.getElementById('vBoost').innerText=parseFloat(this.value).toFixed(1); injetar()"></div>
    <div class="row"><label>Boost Max: <span class="val" id="vBoostMax">-15.0</span> psi</label>
      <input type="range" id="rBoostMax" min="-15" max="30" step="0.1" value="-15" oninput="document.getElementById('vBoostMax').innerText=parseFloat(this.value).toFixed(1); injetar()"></div>
    <div class="row"><label>TPS: <span class="val" id="vTps">0</span> %</label>
      <input type="range" id="rTps" min="0" max="100" value="0" step="1" oninput="document.getElementById('vTps').innerText=this.value; injetar()"></div>
    <div class="row"><label>Pedal: <span class="val" id="vPedal">0</span> %</label>
      <input type="range" id="rPedal" min="0" max="100" value="0" step="1" oninput="document.getElementById('vPedal').innerText=this.value; injetar()"></div>
    <div class="row"><label>Temp Coolant: <span class="val" id="vCoolant">20</span> °C</label>
      <input type="range" id="rCoolant" min="-40" max="150" value="20" step="1" oninput="document.getElementById('vCoolant').innerText=this.value; injetar()"></div>
    <div class="row"><label>Temp Intake: <span class="val" id="vIntake">25</span> °C</label>
      <input type="range" id="rIntake" min="-40" max="100" value="25" step="1" oninput="document.getElementById('vIntake').innerText=this.value; injetar()"></div>
    <div class="row"><label>Bateria: <span class="val" id="vBat">13.8</span> V</label>
      <input type="range" id="rBat" min="8" max="15" step="0.1" value="13.8" oninput="document.getElementById('vBat').innerText=parseFloat(this.value).toFixed(1); injetar()"></div>
    <div class="row"><label>Consumo Inst.: <span class="val" id="vConsumoMl">0</span> ml/min</label>
      <input type="range" id="rConsumoMl" min="0" max="1000" step="1" value="0" oninput="document.getElementById('vConsumoMl').innerText=this.value; injetar()"></div>
    <div class="row"><label>Consumo L/100km: <span class="val" id="vConsumoL100">99.9</span></label>
      <input type="range" id="rConsumoL100" min="0" max="99" step="0.1" value="99.9" oninput="document.getElementById('vConsumoL100').innerText=parseFloat(this.value).toFixed(1); injetar()"></div>
    <div class="row"><label>Consumo Total: <span class="val" id="vConsumoTotal">0.0</span> L</label>
      <input type="range" id="rConsumoTotal" min="0" max="100" step="0.1" value="0" oninput="document.getElementById('vConsumoTotal').innerText=parseFloat(this.value).toFixed(1); injetar()"></div>
    <div class="row"><label>0-100: <span class="val" id="vZeroCem">0.00</span> s</label>
      <input type="range" id="rZeroCem" min="0" max="20" step="0.01" value="0" oninput="document.getElementById('vZeroCem').innerText=parseFloat(this.value).toFixed(2); injetar()"></div>

    <div class="status" id="injectStatus"></div>
  </div>

  <div class="card">
    <h3>Parâmetros (NVS)</h3>
    <div class="row"><label>Cilindrada (L):</label><input type="number" id="cfgEngine" step="0.1" value="2.5"></div>
    <div class="row"><label>Eficiência Volumétrica:</label><input type="number" id="cfgVE" step="0.01" value="0.85"></div>
    <button onclick="salvarConfig()">Salvar</button>
    <div class="status" id="cfgStatus"></div>
  </div>
</div>

<script>
let ws = null;
let isSim = false;
let injectTimer = null;
let wsReconnectTimer = null;

function setStatus(id, txt, cls) {
  const el = document.getElementById(id);
  el.innerText = txt;
  el.className = "status" + (cls ? " " + cls : "");
}

function initWS() {
  if (ws && ws.readyState === WebSocket.OPEN) return;
  ws = new WebSocket('ws://' + window.location.hostname + '/ws');
  ws.onopen = () => setStatus("simStatus", "WS conectado", "ok");
  ws.onmessage = (ev) => {
    let d; try { d = JSON.parse(ev.data); } catch(e) { return; }
    if (d.tipo === 'telemetria' && isSim) {
      setStatus("injectStatus",
        `rpm=${d.rpm.toFixed(0)} vel=${d.velocidade.toFixed(0)} boost=${d.boost.toFixed(1)}`,
        "ok");
    }
  };
  ws.onclose = () => {
    setStatus("simStatus", "WS caiu, reconectando...", "err");
    if (wsReconnectTimer) clearTimeout(wsReconnectTimer);
    wsReconnectTimer = setTimeout(initWS, 2000);
  };
}

function enviarInject(obj) {
  if (ws && ws.readyState === WebSocket.OPEN) {
    ws.send(JSON.stringify({ tipo: "inject", ...obj }));
  } else {
    const p = new URLSearchParams(obj);
    fetch('/api/admin/inject?' + p.toString()).catch(e => console.log(e));
  }
}

function enviarMode(external) {
  fetch('/api/admin/debug?on=' + (external ? '1' : '0'))
    .then(r => r.json())
    .then(d => setStatus("simStatus", "sim_mode = " + (d.sim_mode ? "ON" : "OFF"), "ok"))
    .catch(e => setStatus("simStatus", "erro: " + e.message, "err"));
}

function toggleSim() {
  isSim = !isSim;
  let btn = document.getElementById("modeBtn");
  let box = document.getElementById("simBox");
  if (isSim) {
    btn.innerText = "Modo: EXTERNO";
    btn.className = "active";
    box.style.opacity = "1";
    box.style.pointerEvents = "auto";
  } else {
    btn.innerText = "Modo: OBD2 (Normal)";
    btn.className = "";
    box.style.opacity = "0.3";
    box.style.pointerEvents = "none";
  }
  enviarMode(isSim);
}

function injetar() {
  if (!isSim) return;
  if (injectTimer) return;
  injectTimer = setTimeout(() => {
    injectTimer = null;
    enviarInject({
      rpm:           parseFloat(document.getElementById("rRpm").value),
      vel:           parseFloat(document.getElementById("rVel").value),
      maf:           parseFloat(document.getElementById("rMaf").value),
      boost:         parseFloat(document.getElementById("rBoost").value),
      boost_max:     parseFloat(document.getElementById("rBoostMax").value),
      tps:           parseFloat(document.getElementById("rTps").value),
      pedal:         parseFloat(document.getElementById("rPedal").value),
      temp_coolant:  parseFloat(document.getElementById("rCoolant").value),
      temp_intake:   parseFloat(document.getElementById("rIntake").value),
      bateria:       parseFloat(document.getElementById("rBat").value),
      consumo_ml:    parseFloat(document.getElementById("rConsumoMl").value),
      consumo_l100:  parseFloat(document.getElementById("rConsumoL100").value),
      consumo_total: parseFloat(document.getElementById("rConsumoTotal").value),
      zero_cem:      parseFloat(document.getElementById("rZeroCem").value)
    });
  }, 50);
}

function salvarConfig() {
  const motor = document.getElementById("cfgEngine").value;
  const ve = document.getElementById("cfgVE").value;
  setStatus("cfgStatus", "Salvando...");
  fetch(`/api/admin/set?motor=${motor}&ve=${ve}`)
    .then(r => r.json())
    .then(d => setStatus("cfgStatus", "OK: motor=" + d.motor + " ve=" + d.ve, "ok"))
    .catch(e => setStatus("cfgStatus", "erro: " + e.message, "err"));
}

window.onload = function() {
  initWS();
  fetch('/api/admin/get')
    .then(r => r.json())
    .then(d => {
      document.getElementById("cfgEngine").value = d.motor;
      document.getElementById("cfgVE").value = d.ve;
      isSim = !!d.sim_mode;
      if (isSim) {
        let btn = document.getElementById("modeBtn");
        let box = document.getElementById("simBox");
        btn.innerText = "Modo: EXTERNO";
        btn.className = "active";
        box.style.opacity = "1";
        box.style.pointerEvents = "auto";
        setStatus("simStatus", "sim_mode = ON");
      } else {
        setStatus("simStatus", "sim_mode = OFF");
      }
    });
};
</script>
</body>
</html>
)rawliteral";

#endif