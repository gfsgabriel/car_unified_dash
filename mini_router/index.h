#ifndef INDEX_H
#define INDEX_H
#include <Arduino.h>

const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="pt-BR">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0, user-scalable=no">
    <title>Painel FAT - Audio Control</title>
    <style>
        * { box-sizing: border-box; touch-action: manipulation; user-select: none; }
        body { font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif; background: #121212; color: #fff; margin: 0; padding: 15px; display: flex; flex-direction: column; align-items: center; }
        h2 { margin: 5px 0 15px; font-size: 1.4rem; color: #00e676; text-shadow: 0 0 10px rgba(0,230,118,0.3); }
        .container { display: flex; flex-direction: column; gap: 15px; align-items: center; width: 100%; max-width: 420px; }
        .card { background: #1e1e1e; padding: 15px; border-radius: 12px; border: 1px solid #333; box-shadow: 0 4px 15px rgba(0,0,0,0.5); width: 100%; display: flex; flex-direction: column; align-items: center; }

        .tabs { display: flex; width: 100%; gap: 5px; margin-bottom: 10px; }
        .tab-btn { flex: 1; background: #262626; border: 1px solid #444; color: #aaa; padding: 8px; font-size: 0.8rem; font-weight: bold; border-radius: 6px; cursor: pointer; transition: 0.2s; }
        .tab-btn.active { background: #1a385c; border-color: #2979ff; color: #fff; }

        canvas { background: #000; border-radius: 8px; border: 1px solid #444; width: 100%; height: 160px; }

        .hardware-panel { display: flex; flex-direction: column; gap: 12px; width: 100%; margin-top: 5px; }

        /* 3 botões de mídia em linha */
        .media-row { display: grid; grid-template-columns: repeat(3, 1fr); gap: 10px; }

        /* 2 botões de volume em linha */
        .volume-row { display: grid; grid-template-columns: repeat(2, 1fr); gap: 10px; }

        .btn { background: #262626; border: 1px solid #444; color: #fff; padding: 14px 0; border-radius: 8px; font-weight: bold; font-size: 0.85rem; cursor: pointer; transition: 0.1s; display: flex; align-items: center; justify-content: center; gap: 5px; }
        .btn:active { background: #00e676; color: #000; transform: scale(0.96); box-shadow: 0 0 12px #00e676; border-color: #00e676; }
        .btn-media  { background: #1a385c; border-color: #2979ff; }
        .btn-volume { background: #1a4a2a; border-color: #00e676; }

        .status-bar { font-size: 0.75rem; color: #888; margin-top: 8px; display: flex; justify-content: space-between; width: 100%; }
        .dot { width: 8px; height: 8px; border-radius: 50%; background: #ff5252; display: inline-block; margin-right: 5px; }
        .dot.online { background: #00e676; box-shadow: 0 0 8px #00e676; }
    </style>
</head>
<body>
    <h2>PAINEL FAT - AUDIO</h2>
    <div class="container">
        <div class="card">
            <div class="tabs">
                <button class="tab-btn active" onclick="mudarAba(0)">Equalizador</button>
                <button class="tab-btn" onclick="mudarAba(1)">Waveform</button>
            </div>
            <canvas id="fftCanvas" width="320" height="160"></canvas>
            <div class="status-bar">
                <span><span id="dot" class="dot"></span><span id="statusTxt">Desconectado</span></span>
                <span>FFT: 9 Bandas Reais</span>
            </div>
        </div>

        <div class="card">
            <div class="hardware-panel">
                <div class="media-row">
                    <button class="btn btn-media" onclick="sendBtn(0)">⏮ PREV</button>
                    <button class="btn btn-media" onclick="sendBtn(1)">⏯ PLAY</button>
                    <button class="btn btn-media" onclick="sendBtn(2)">⏭ NEXT</button>
                </div>
                <div class="volume-row">
                    <button class="btn btn-volume" onclick="sendBtn(3)">🔉 VOL -</button>
                    <button class="btn btn-volume" onclick="sendBtn(4)">🔊 VOL +</button>
                </div>
            </div>
        </div>
    </div>

    <script>
        const canvas = document.getElementById('fftCanvas');
        const ctx = canvas.getContext('2d');
        const statusTxt = document.getElementById('statusTxt');
        const dot = document.getElementById('dot');

        let ws = null;
        let abaAtual = 0;
        let faseWaveform = 0;
        let ultimosDadosFFT = new Uint8Array(32);

        const legendasFrequencia = ["60", "120", "240", "480", "960", "2K", "4K", "8K"];
        const ganhoBanda = [14.0, 12.0, 10.0, 8.0, 6.5, 5.0, 3.8, 2.5, 1.2];

        function mudarAba(id) {
            abaAtual = id;
            document.querySelectorAll('.tab-btn').forEach((btn, idx) => {
                btn.classList.toggle('active', idx === id);
            });
        }

        function initWebSocket() {
            ws = new WebSocket(`ws://${location.host}/ws`);
            ws.binaryType = 'arraybuffer';
            ws.onopen = () => {
                statusTxt.innerText = "Conectado (WS)";
                dot.classList.add('online');
            };
            ws.onclose = () => {
                statusTxt.innerText = "Reconectando...";
                dot.classList.remove('online');
                setTimeout(initWebSocket, 2000);
            };
            ws.onmessage = (evt) => {
                if (evt.data instanceof ArrayBuffer) {
                    ultimosDadosFFT = new Uint8Array(evt.data);
                }
            };
        }

        function renderLoop() {
            ctx.clearRect(0, 0, canvas.width, canvas.height);

            if (abaAtual === 0) {
                const larguraBarra = 32;
                const espacamento = 6;
                const xInicial = 10;
                const baseLineY = canvas.height - 25;

                for (let i = 0; i < 8; i++) {
                    let valorFFT = ultimosDadosFFT[i];

                    if (i === 7) {
                        let somaCapped = ultimosDadosFFT[7] + ultimosDadosFFT[8];
                        valorFFT = Math.min(somaCapped, 255);
                    }

                    const alturaBarra = (valorFFT / 255) * (baseLineY - 10);
                    const x = xInicial + (i * (larguraBarra + espacamento));
                    const y = baseLineY - alturaBarra;

                    ctx.fillStyle = '#00e676';
                    ctx.fillRect(x, y, larguraBarra, alturaBarra);

                    ctx.fillStyle = '#888888';
                    ctx.font = 'bold 11px sans-serif';
                    ctx.textAlign = 'center';
                    ctx.fillText(legendasFrequencia[i], x + (larguraBarra / 2), canvas.height - 8);

                    if (i < 7) {
                        const xLinha = x + larguraBarra + (espacamento / 2);
                        ctx.strokeStyle = '#333333';
                        ctx.lineWidth = 1;
                        ctx.beginPath();
                        ctx.moveTo(xLinha, baseLineY);
                        ctx.lineTo(xLinha, canvas.height);
                        ctx.stroke();
                    }
                }
            } else {
                faseWaveform += 0.15;
                if (faseWaveform > Math.PI * 2) faseWaveform -= Math.PI * 2;

                const yCentro = canvas.height / 2;
                const alturaMax = 65;
                const numPontos = 160;

                ctx.strokeStyle = '#00e5ff';
                ctx.lineWidth = 2;
                ctx.beginPath();

                for (let i = 0; i < numPontos; i++) {
                    const xNorm = i / (numPontos - 1);
                    let somaSenoides = 0;

                    for (let b = 0; b < 9; b++) {
                        let f = ultimosDadosFFT[b] / 255.0;
                        let freqHarmonica = 1 << b;
                        let direcaoFase = (b % 2 === 0) ? 1.0 : -1.0;
                        let angulo = (xNorm * Math.PI * 2 * freqHarmonica) + (faseWaveform * direcaoFase * (1.0 + b * 0.2));

                        somaSenoides += Math.sin(angulo) * (f * ganhoBanda[b] * 1.8);
                    }

                    somaSenoides = Math.max(-alturaMax, Math.min(somaSenoides, alturaMax));

                    const x = (i / (numPontos - 1)) * canvas.width;
                    const y = yCentro - somaSenoides;

                    if (i === 0) ctx.moveTo(x, y);
                    else ctx.lineTo(x, y);
                }
                ctx.stroke();
            }

            requestAnimationFrame(renderLoop);
        }

        function sendBtn(id) {
            fetch(`/cmd?btn=${id}`).catch(err => console.error(err));
        }

        window.onload = () => {
            initWebSocket();
            renderLoop();
        };
    </script>
</body>
</html>
)rawliteral";

#endif