#ifndef BT_H
#define BT_H
#include <Arduino.h>

const char BT_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="pt-BR">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Debug BT</title>
<style>
body{font-family:'Courier New',monospace;background:#0a0a0c;color:#00ff66;padding:10px;margin:0}
h1{color:#0ff;font-size:1.1em;border-bottom:1px solid #222;padding-bottom:6px;margin:0 0 10px 0}
.tabs{display:flex;gap:4px;margin-bottom:10px}
.tab-btn{flex:1;padding:9px;background:#1a1a1e;border:1px solid #333;color:#888;font-family:inherit;font-size:0.85em;cursor:pointer;border-radius:4px}
.tab-btn.active{background:#008cba;color:#fff;border-color:#0ad;font-weight:bold}
.bar{display:flex;gap:8px;margin-bottom:8px;align-items:center;flex-wrap:wrap}
button{padding:8px 12px;background:#08c;border:none;color:#fff;font-family:inherit;font-size:.9em;border-radius:4px;cursor:pointer}
button:hover{background:#0ad}
button.r{background:#d9534f}
button.r:hover{background:#c9302c}
button.g{background:#28a745}
button.g:hover{background:#34ce57}
button.p{background:#6a4c93}
button.p:hover{background:#8259b0}
#status{color:#888;font-size:.85em;margin-left:auto}
#term,#echoTerm,#xmTerm{background:#000;border:1px solid #333;padding:10px;height:55vh;overflow-y:auto;border-radius:6px;font-size:.8em;font-family:'Courier New',monospace;white-space:pre-wrap;word-break:break-all}
.line{margin-bottom:4px;line-height:1.3em;white-space:pre-wrap}
.tx{color:#fa0}
.rx{color:#0f6}
.timeout{color:#f44}
.sys{color:#0af}
.echo-resp{color:#0ff}
.echo-sys{color:#888}
.auto-ind{color:#666;font-size:.75em;margin-left:8px}
.quick-btns{display:flex;flex-wrap:wrap;gap:5px;margin-bottom:8px}
.quick-btns button{padding:6px 10px;font-size:.85em;background:#333;border:1px solid #555}
.quick-btns button:hover{background:#444}
.input-row{display:flex;gap:5px;margin-bottom:8px}
.input-row input{flex:1;padding:10px;background:#111;border:1px solid #333;color:#0f6;font-family:inherit;font-size:.95em;border-radius:4px}
.input-row button{padding:10px 16px;background:#28a745;font-weight:bold}
.input-row button:disabled{background:#444;cursor:not-allowed}
.baud-card{background:#1a2a1a;border:1px solid #5a3a1a;padding:10px;border-radius:4px;margin-top:15px}
.baud-card h4{color:#fa0;margin:0 0 8px 0;font-size:0.95em}
.baud-info{color:#ccc;font-size:0.85em;margin-bottom:6px}
.baud-info span{color:#0f6;font-family:monospace}
.baud-info span.wait{color:#fa0}
.baud-info span.err{color:#f44}
select{width:100%;padding:10px;background:#111;border:1px solid #333;color:#0f6;font-family:inherit;font-size:.95em;border-radius:4px}
.prefix{color:#666}
.cmd-hl{color:#ff6}
.raw-hl{color:#4cf}
.meta{color:#555;font-size:.85em}
</style>
</head>
<body>
<h1>Debug BT (XM-15B) <span id="status">desconectado</span></h1>

<div class="tabs">
    <button id="tabBtnSniffer" class="tab-btn active" onclick="switchTab('sniffer')">Sniffer</button>
    <button id="tabBtnEcho" class="tab-btn" onclick="switchTab('echo')">ELM</button>
    <button id="tabBtnXM" class="tab-btn" onclick="switchTab('xm')">XM</button>
</div>

<!-- ==================== TAB 1: SNIFFER ==================== -->
<div id="tabSniffer">
    <div class="bar">
        <button id="btnDebug" onclick="toggleDebug()">Ativar Debug</button>
        <button class="r" onclick="limparSniffer()">Limpar</button>
        <span id="autoInd" class="auto-ind">[auto-scroll: ON]</span>
    </div>
    <div id="term"></div>
</div>

<!-- ==================== TAB 2: ELM ==================== -->
<div id="tabEcho" style="display:none">
    <div class="bar">
        <button class="r" onclick="limparEcho('echoTerm')">Limpar</button>
        <span id="echoStatus" class="auto-ind">Terminador: &gt;</span>
    </div>

    <div class="quick-btns">
        <button onclick="echoRapido('ELM','ATZ', 5000)">ATZ</button>
        <button onclick="echoRapido('ELM','ATE0')">ATE0</button>
        <button onclick="echoRapido('ELM','ATL0')">ATL0</button>
        <button onclick="echoRapido('ELM','ATH0')">ATH0</button>
        <button onclick="echoRapido('ELM','ATSP0', 5000)">ATSP0</button>
        <button onclick="echoRapido('ELM','ATI')">ATI</button>
        <button onclick="echoRapido('ELM','ATRV')">ATRV</button>
        <button onclick="echoRapido('ELM','ATDP')">ATDP</button>
        <button onclick="echoRapido('ELM','ATDPN')">ATDPN</button>
        <button onclick="echoRapido('ELM','0100')">0100</button>
        <button onclick="echoRapido('ELM','0120')">0120</button>
        <button onclick="echoRapido('ELM','0140')">0140</button>
        <button onclick="echoRapido('ELM','010C')">010C RPM</button>
        <button onclick="echoRapido('ELM','0110')">0110 MAF</button>
        <button onclick="echoRapido('ELM','010D')">010D Vel</button>
        <button onclick="echoRapido('ELM','0111')">0111 TPS</button>
        <button onclick="echoRapido('ELM','0149')">0149 Pedal</button>
        <button onclick="echoRapido('ELM','010F')">010F IAT</button>
        <button onclick="echoRapido('ELM','0105')">0105 Coolant</button>
        <button onclick="echoRapido('ELM','0142')">0142 Bateria</button>
        <button onclick="echoRapido('ELM','010C100D11', 8000)" style="background:#6a4c93">MULTI RPM+MAF+Vel</button>
        <button onclick="echoRapido('ELM','010C100D11490F05', 8000)" style="background:#6a4c93">MULTI 6 PIDs</button>
    </div>

    <div class="input-row">
        <input type="text" id="cmdInput" placeholder="Comando ELM (ex: 010C)" onkeydown="if(event.key==='Enter') echoEnviar('ELM')">
        <button id="btnSendEcho" onclick="echoEnviar('ELM')">Enviar</button>
    </div>

    <div id="echoTerm"></div>
</div>

<!-- ==================== TAB 3: XM ==================== -->
<div id="tabXM" style="display:none">

    <div class="bar">
        <button class="r" onclick="limparEcho('xmTerm')">Limpar</button>
        <span id="xmStatus" class="auto-ind">Terminador: OK (ou +INQ:COMPLETE)</span>
    </div>

    <div class="quick-btns">
        <button onclick="echoRapido('XM','AT')">AT (teste)</button>
        <button onclick="echoRapido('XM','ATI')">ATI (versão)</button>
        <button onclick="echoRapido('XM','AT+NAME?')">AT+NAME?</button>
        <button onclick="echoRapido('XM','AT+ADDR?')">AT+ADDR?</button>
        <button onclick="echoRapido('XM','AT+ROLE?')">AT+ROLE?</button>
        <button onclick="echoRapido('XM','AT+BIND?')">AT+BIND?</button>
        <button onclick="echoRapido('XM','AT+STATE?')">AT+STATE?</button>
        <button onclick="echoRapido('XM','AT+PSWD?')">AT+PSWD?</button>
        <button onclick="echoRapido('XM','AT+VERSION?')">AT+VERSION?</button>
        <button onclick="echoRapido('XM','AT+CMODE?')">AT+CMODE?</button>
        <button onclick="echoRapido('XM','AT+INQM?')">AT+INQM?</button>
        <button onclick="consultarUartXM()" style="background:#28a745;color:#fff">🔍 AT+UART? (consultar baud)</button>
    </div>

    <div class="quick-btns">
        <button onclick="if(confirm('AT+DISC desconecta. Continuar?')) echoRapido('XM','AT+DISC', 3000)" style="background:#a55">AT+DISC</button>
        <button onclick="if(confirm('AT+RESET reinicia o XM. Continuar?')) echoRapido('XM','AT+RESET', 3000)" style="background:#a55">AT+RESET</button>
        <button onclick="if(confirm('AT+ORGL é RESET DE FÁBRICA (apaga BIND, baud, etc). Continuar?')) echoRapido('XM','AT+ORGL', 5000)" style="background:#d9534f">AT+ORGL (factory)</button>
        <button onclick="if(confirm('AT+ROLE=1 (escravo). Continuar?')) echoRapido('XM','AT+ROLE=1', 3000)" style="background:#555">AT+ROLE=1</button>
        <button onclick="if(confirm('AT+ROLE=0 (mestre). Continuar?')) echoRapido('XM','AT+ROLE=0', 3000)" style="background:#555">AT+ROLE=0</button>
        <button onclick="if(confirm('AT+CMODE=0 (conecta só no BIND). Continuar?')) echoRapido('XM','AT+CMODE=0', 3000)" style="background:#555">AT+CMODE=0</button>
        <button onclick="if(confirm('AT+CMODE=1 (aceita qualquer). Continuar?')) echoRapido('XM','AT+CMODE=1', 3000)" style="background:#555">AT+CMODE=1</button>
    </div>

    <div class="quick-btns">
        <button onclick="if(confirm('Iniciar scan de dispositivos BT (25s)?')) echoRapido('XM','AT+INQ', 25000, 'INQ:COMPLETE')"
                style="background:#28a745;color:#fff">
            🔍 SCAN (AT+INQ)
        </button>
    </div>

    <div class="input-row">
        <input type="text" id="xmInput" placeholder="Comando XM (ex: AT+NAME?)" onkeydown="if(event.key==='Enter') echoEnviar('XM')">
        <button id="btnSendXM" onclick="echoEnviar('XM')">Enviar</button>
    </div>

    <div id="xmTerm" style="height:40vh"></div>

    <div class="baud-card">
        <h4>⚙️ Baud Rate (XM ↔ ESP)</h4>
        <div class="baud-info">
            ESP atual: <span id="baudEspAtual">---</span>
        </div>
        <div class="baud-info">
            XM atual:  <span id="baudXmAtual" class="wait">clique consultar acima</span>
        </div>

        <select id="baudSelect" style="margin-top:8px">
            <option value="9600">9600 bps</option>
            <option value="19200">19200 bps</option>
            <option value="38400">38400 bps</option>
            <option value="57600">57600 bps</option>
            <option value="115200">115200 bps</option>
        </select>

        <div class="quick-btns" style="margin:8px 0 0 0">
            <button onclick="aplicarBaudEsp(false)" style="background:#555">Aplicar só ESP</button>
            <button onclick="aplicarBaudEsp(true)" style="background:#6a4c93">Aplicar ESP + XM</button>
        </div>

        <div class="baud-info" id="baudStatus" style="margin-top:8px;color:#fa0"></div>
    </div>
</div>

<script>
let ws = null;
let reconectTimer;
let debugAtivo = false;
let autoScroll = true;
let echoPendente = false;
let echoPendenteTipo = '';

const term     = document.getElementById('term');
const echoTerm = document.getElementById('echoTerm');
const xmTerm   = document.getElementById('xmTerm');

function switchTab(tab) {
    document.getElementById('tabSniffer').style.display = (tab === 'sniffer') ? 'block' : 'none';
    document.getElementById('tabEcho').style.display    = (tab === 'echo')    ? 'block' : 'none';
    document.getElementById('tabXM').style.display      = (tab === 'xm')      ? 'block' : 'none';

    document.getElementById('tabBtnSniffer').classList.toggle('active', tab === 'sniffer');
    document.getElementById('tabBtnEcho').classList.toggle('active',    tab === 'echo');
    document.getElementById('tabBtnXM').classList.toggle('active',      tab === 'xm');

    if (tab === 'xm') lerBaudEsp();
}

// ==================== SNIFFER ====================
function escHtml(s) {
    if (!s) return '';
    return String(s).replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;');
}

function appendLinhaSys(txt) {
    let div = document.createElement('div');
    div.className = 'line sys';
    div.innerText = '[' + new Date().toLocaleTimeString() + '] ' + txt;
    term.appendChild(div);
    if (autoScroll) term.scrollTop = term.scrollHeight;
}

function renderDebugMsg(m) {
    let div = document.createElement('div');
    div.className = 'line';
    let ts = new Date().toLocaleTimeString();

    if (m.t === 'T') {
        div.classList.add('tx');
        div.innerHTML =
            `<span class="prefix">[${ts}] → [${m.d}]</span> ` +
            `<span class="cmd-hl">${escHtml(m.c)}</span>`;
    } else if (m.t === 'R') {
        div.classList.add(m.ok ? 'rx' : 'timeout');

        let raw = escHtml(m.r || '');
        raw = raw.replace(/\r\n|\r|\n/g, '<br>');

        div.innerHTML =
            `<span class="prefix">[${ts}] ← [${m.d}]</span> ` +
            `<span class="cmd-hl">${escHtml(m.c)}</span><br>` +
            `<span class="raw-hl">${raw}</span><br>` +
            `<span class="meta">(${m.dur}ms) ${m.ok ? 'OK' : 'FAIL'}</span>`;
    } else {
        div.classList.add('sys');
        div.innerHTML =
            `<span class="prefix">[${ts}]</span> ` +
            `<span class="sys">${escHtml(m.c)}</span>`;
    }

    term.appendChild(div);
    if (autoScroll) term.scrollTop = term.scrollHeight;
}

function appendLinhaAntiga(linha) {
    let div = document.createElement('div');
    div.className = 'line';
    if (linha.startsWith('→')) div.classList.add('tx');
    else if (linha.startsWith('←')) {
        if (linha.includes('TIMEOUT') || linha.includes('FAIL')) div.classList.add('timeout');
        else div.classList.add('rx');
    } else div.classList.add('sys');
    div.innerText = '[' + new Date().toLocaleTimeString() + '] ' + linha;
    term.appendChild(div);
    if (autoScroll) term.scrollTop = term.scrollHeight;
}

term.addEventListener('scroll', () => {
    const perto = (term.scrollHeight - term.scrollTop - term.clientHeight) < 30;
    if (autoScroll !== perto) {
        autoScroll = perto;
        document.getElementById('autoInd').innerText = '[auto-scroll: ' + (autoScroll ? 'ON' : 'OFF') + ']';
    }
});

function limparSniffer() { term.innerHTML = ''; }

function toggleDebug() {
    let novo = !debugAtivo;
    fetch('/api/bt/debug?on=' + (novo ? '1' : '0'))
        .then(r => r.text())
        .then(txt => {
            debugAtivo = novo;
            document.getElementById('btnDebug').innerText = debugAtivo ? 'Desativar Debug' : 'Ativar Debug';
            appendLinhaSys('[SYS] ' + txt);
        })
        .catch(e => appendLinhaSys('[ERR] ' + e.message));
}

// ==================== ECHO ====================
function getTerminalEL(tipo) { return (tipo === 'XM') ? xmTerm : echoTerm; }
function getStatusEL(tipo)   { return (tipo === 'XM') ? 'xmStatus' : 'echoStatus'; }

function appendEcho(tipo, txt, cls) {
    let t = getTerminalEL(tipo);
    let div = document.createElement('div');
    div.className = 'line ' + (cls || 'echo-resp');
    div.innerText = txt;
    t.appendChild(div);
    t.scrollTop = t.scrollHeight;
}

function limparEcho(id) { document.getElementById(id).innerHTML = ''; }

function echoRapido(tipo, cmd, timeout, termCustom) {
    document.getElementById(tipo === 'XM' ? 'xmInput' : 'cmdInput').value = cmd;
    echoEnviar(tipo, timeout, termCustom);
}

function echoEnviar(tipo, timeoutCustom, termCustom) {
    if (echoPendente) {
        appendEcho(tipo, '[SYS] aguarde resposta anterior', 'echo-sys');
        return;
    }
    const inputId = (tipo === 'XM') ? 'xmInput' : 'cmdInput';
    const cmd = document.getElementById(inputId).value.trim();
    if (!cmd) return;
    if (!ws || ws.readyState !== WebSocket.OPEN) {
        appendEcho(tipo, '[ERR] WS desconectado', 'echo-sys');
        return;
    }

    const timeout = timeoutCustom || 3000;
    const termFinal = termCustom || ((tipo === 'XM') ? 'OK' : '>');

    appendEcho(tipo, '[TX] ' + cmd + '  (term: ' + termFinal + ')', 'echo-sys');

    ws.send(JSON.stringify({
        tipo: 'echo',
        cmd: cmd,
        term: termFinal,
        timeout: timeout
    }));

    echoPendente = true;
    echoPendenteTipo = tipo;
    document.getElementById('btnSendEcho').disabled = true;
    document.getElementById('btnSendXM').disabled = true;
    document.getElementById(getStatusEL(tipo)).innerText = 'aguardando ' + termFinal + '...';
}

let aguardandoUart = false;
function consultarUartXM() {
    aguardandoUart = true;
    const el = document.getElementById('baudXmAtual');
    el.innerText = 'consultando...';
    el.className = 'wait';
    document.getElementById('xmInput').value = 'AT+UART?';
    echoEnviar('XM', 3000, 'OK');
}

// ==================== BAUD ====================
function lerBaudEsp() {
    fetch('/api/bt/baud')
        .then(r => r.json())
        .then(d => {
            document.getElementById('baudEspAtual').innerText = d.baud + ' bps';
            document.getElementById('baudSelect').value = d.baud;
        })
        .catch(e => {
            document.getElementById('baudEspAtual').innerText = 'erro';
        });
}

function aplicarBaudEsp(atualizarXM) {
    const novo = parseInt(document.getElementById('baudSelect').value);

    const msg = atualizarXM
        ? `Mudar XM + ESP para ${novo} bps?\n\nO XM vai ser reconfigurado E o ESP vai reiniciar.`
        : `Mudar apenas ESP para ${novo} bps?\n\nO ESP vai reiniciar. Use quando o XM JÁ está em ${novo}.`;

    if (!confirm(msg)) return;

    setBaudStatus('Enviando...', 'wait');

    let url = '/api/bt/baud?set=' + novo;
    if (atualizarXM) url += '&xm=1';

    fetch(url)
        .then(r => r.json())
        .then(d => {
            setBaudStatus('OK — reiniciando em ' + (atualizarXM ? 'XM+ESP' : 'ESP') + ' → ' + novo, 'ok');
            setTimeout(() => {
                setBaudStatus('Aguardando reboot...', 'wait');
                setTimeout(() => location.reload(), 6000);
            }, 1000);
        })
        .catch(e => {
            setBaudStatus('ESP reiniciando... (aguarde 6s)', 'ok');
            setTimeout(() => location.reload(), 7000);
        });
}

function setBaudStatus(txt, cls) {
    const el = document.getElementById('baudStatus');
    el.innerText = txt;
    el.style.color = (cls === 'ok') ? '#0f6' : ((cls === 'wait') ? '#fa0' : '#0af');
}

// ==================== WS ====================
function initWS() {
    if (ws && ws.readyState === WebSocket.OPEN) return;
    appendLinhaSys('[SYS] Conectando...');
    ws = new WebSocket('ws://' + window.location.hostname + '/ws');

    ws.onopen = () => {
        document.getElementById('status').innerText = 'conectado';
        appendLinhaSys('[SYS] Conectado');
    };

    ws.onmessage = (ev) => {
        let d;
        try { d = JSON.parse(ev.data); } catch(e) { return; }

        if (d.tipo === 'bt_debug') {
            let ok = false;
            try {
                let arr = JSON.parse(d.data);
                if (Array.isArray(arr)) {
                    arr.forEach(m => renderDebugMsg(m));
                    ok = true;
                }
            } catch(e) { /* fallback */ }

            if (!ok) {
                d.data.split('\n').forEach(l => { if (l.trim()) appendLinhaAntiga(l); });
            }
        }
        else if (d.tipo === 'echo_resp') {
            echoPendente = false;
            document.getElementById('btnSendEcho').disabled = false;
            document.getElementById('btnSendXM').disabled = false;

            const tipo = echoPendenteTipo;
            echoPendenteTipo = '';

            document.getElementById(getStatusEL(tipo)).innerText = d.ok ? 'OK' : 'FALHOU';
            appendEcho(tipo, '[RX] ' + (d.resp || '(vazio)'),
                       d.ok ? 'echo-resp' : 'timeout');

            if (aguardandoUart) {
                aguardandoUart = false;
                const el = document.getElementById('baudXmAtual');

                let m = (d.resp || '').match(/\+UART:(\d+)/);
                if (m) {
                    el.innerText = m[1] + ' bps';
                    el.className = '';
                    document.getElementById('baudSelect').value = m[1];
                } else {
                    el.innerText = d.ok ? '(sem resposta de UART)' : '(falhou)';
                    el.className = 'err';
                }
            }
        }
    };

    ws.onclose = () => {
        document.getElementById('status').innerText = 'desconectado';
        appendLinhaSys('[SYS] Desconectado, reconectando...');
        if (reconectTimer) clearTimeout(reconectTimer);
        reconectTimer = setTimeout(initWS, 2000);
    };

    ws.onerror = () => appendLinhaSys('[ERR] Erro WebSocket');
}

window.onload = function() {
    initWS();
    lerBaudEsp();
};
</script>
</body>
</html>
)rawliteral";

#endif