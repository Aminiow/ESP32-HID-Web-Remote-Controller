// V2
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <WebSocketsServer.h>
#include <USB.h>
#include <USBHIDMouse.h>
#include <USBHIDKeyboard.h>

USBHIDMouse Mouse;
USBHIDKeyboard Keyboard;
WebServer server(80);
DNSServer dnsServer;
WebSocketsServer webSocket(81);

const char* ssid = "ESP32-Mouse";
const char* password = "12345678";

float sensitivity = 2.0;
int repeatInterval = 100;
bool legacyMode = false;

bool ctrlPressed = false;
bool altPressed = false;
bool shiftPressed = false;
bool winPressed = false;

int16_t clamp(int16_t v, int16_t minv, int16_t maxv) {
  if (v < minv) return minv;
  if (v > maxv) return maxv;
  return v;
}

void applyModifiers() {
  if (ctrlPressed) Keyboard.press(KEY_LEFT_CTRL);
  else Keyboard.release(KEY_LEFT_CTRL);
  if (altPressed) Keyboard.press(KEY_LEFT_ALT);
  else Keyboard.release(KEY_LEFT_ALT);
  if (shiftPressed) Keyboard.press(KEY_LEFT_SHIFT);
  else Keyboard.release(KEY_LEFT_SHIFT);
  if (winPressed) Keyboard.press(KEY_LEFT_GUI);
  else Keyboard.release(KEY_LEFT_GUI);
}

void releaseAllModifiers() {
  ctrlPressed = altPressed = shiftPressed = winPressed = false;
  Keyboard.release(KEY_LEFT_CTRL);
  Keyboard.release(KEY_LEFT_ALT);
  Keyboard.release(KEY_LEFT_SHIFT);
  Keyboard.release(KEY_LEFT_GUI);
}

void toggleModifier(const String& mod) {
  if (mod == "CTRL") ctrlPressed = !ctrlPressed;
  else if (mod == "ALT") altPressed = !altPressed;
  else if (mod == "SHIFT") shiftPressed = !shiftPressed;
  else if (mod == "WIN") winPressed = !winPressed;
  applyModifiers();
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t length) {
  if (type == WStype_TEXT) {
    String msg = "";
    for (size_t i = 0; i < length; i++) msg += (char)payload[i];
    msg.trim();
    int dx = 0, dy = 0;
    if (sscanf(msg.c_str(), "{\"dx\":%d,\"dy\":%d}", &dx, &dy) == 2) {
      dx = clamp(dx, -127, 127);
      dy = clamp(dy, -127, 127);
      Mouse.move(dx, dy, 0);
    }
  }
}

void handleMove() {
  int dx = server.arg("dx").toInt();
  int dy = server.arg("dy").toInt();
  dx = clamp(dx, -127, 127);
  dy = clamp(dy, -127, 127);
  Mouse.move(dx, dy, 0);
  server.send(200, "text/plain", "OK");
}

void handleClick() {
  String btn = server.arg("btn");
  if (btn == "right") Mouse.click(MOUSE_RIGHT);
  else if (btn == "middle") Mouse.click(MOUSE_MIDDLE);
  else Mouse.click(MOUSE_LEFT);
  server.send(200, "text/plain", "OK");
}

void handleDoubleClick() {
  String btn = server.arg("btn");
  if (btn == "right") {
    Mouse.click(MOUSE_RIGHT);
    delay(50);
    Mouse.click(MOUSE_RIGHT);
  } else {
    Mouse.click(MOUSE_LEFT);
    delay(50);
    Mouse.click(MOUSE_LEFT);
  }
  server.send(200, "text/plain", "OK");
}

void handleDown() {
  String btn = server.arg("btn");
  if (btn == "right") Mouse.press(MOUSE_RIGHT);
  else if (btn == "middle") Mouse.press(MOUSE_MIDDLE);
  else Mouse.press(MOUSE_LEFT);
  server.send(200, "text/plain", "OK");
}

void handleUp() {
  String btn = server.arg("btn");
  if (btn == "right") Mouse.release(MOUSE_RIGHT);
  else if (btn == "middle") Mouse.release(MOUSE_MIDDLE);
  else Mouse.release(MOUSE_LEFT);
  server.send(200, "text/plain", "OK");
}

void handleWheel() {
  int delta = server.arg("delta").toInt();
  delta = clamp(delta, -127, 127);
  Mouse.move(0, 0, delta);
  server.send(200, "text/plain", "OK");
}

void handleSetSensitivity() {
  float val = server.arg("value").toFloat();
  if (val < 0.1) val = 0.1;
  if (val > 10.0) val = 10.0;
  sensitivity = val;
  server.send(200, "text/plain", "OK");
}

void handleSetRepeatInterval() {
  int val = server.arg("value").toInt();
  if (val < 20) val = 20;
  if (val > 1000) val = 1000;
  repeatInterval = val;
  server.send(200, "text/plain", "OK");
}

void handleSetLegacyMode() {
  int val = server.arg("value").toInt();
  legacyMode = (val == 1);
  server.send(200, "text/plain", "OK");
}

void sendKeyTap(uint8_t keycode) {
  if (legacyMode) {
    Keyboard.press(keycode);
    delay(40);
    Keyboard.release(keycode);
  } else {
    Keyboard.press(keycode);
    delay(20);
    Keyboard.release(keycode);
  }
}

void handleType() {
  String text = server.arg("text");
  String asciiText = "";
  for (size_t i = 0; i < text.length(); i++) {
    char c = text.charAt(i);
    if (c >= 32 && c <= 126) asciiText += c;
  }
  for (size_t i = 0; i < asciiText.length(); i++) {
    char c = asciiText.charAt(i);
    if (legacyMode) {
      Keyboard.write(c);
      delay(10);
    } else {
      Keyboard.press(c);
      delay(5);
      Keyboard.release(c);
      delay(5);
    }
  }
  server.send(200, "text/plain", "OK");
}

uint8_t keyNameToCode(const String& key) {
  String k = key;
  k.toUpperCase();
  if (k == "ENTER") return KEY_RETURN;
  if (k == "BACKSPACE") return KEY_BACKSPACE;
  if (k == "TAB") return KEY_TAB;
  if (k == "SPACE") return ' ';
  if (k == "ESC") return KEY_ESC;
  if (k == "DELETE") return KEY_DELETE;
  if (k == "CAPSLOCK") return KEY_CAPS_LOCK;
  if (k == "UP") return KEY_UP_ARROW;
  if (k == "DOWN") return KEY_DOWN_ARROW;
  if (k == "LEFT") return KEY_LEFT_ARROW;
  if (k == "RIGHT") return KEY_RIGHT_ARROW;
  if (k == "HOME") return KEY_HOME;
  if (k == "END") return KEY_END;
  if (k == "PAGEUP") return KEY_PAGE_UP;
  if (k == "PAGEDOWN") return KEY_PAGE_DOWN;
  if (k == "INSERT") return KEY_INSERT;
  if (k == "F1") return KEY_F1;
  if (k == "F2") return KEY_F2;
  if (k == "F3") return KEY_F3;
  if (k == "F4") return KEY_F4;
  if (k == "F5") return KEY_F5;
  if (k == "F6") return KEY_F6;
  if (k == "F7") return KEY_F7;
  if (k == "F8") return KEY_F8;
  if (k == "F9") return KEY_F9;
  if (k == "F10") return KEY_F10;
  if (k == "F11") return KEY_F11;
  if (k == "F12") return KEY_F12;
  if (k == "PRTSC") return KEY_PRINT_SCREEN;
  if (k == "SCRLK") return KEY_SCROLL_LOCK;
  if (k == "PAUSE") return KEY_PAUSE;
  if (k == "NUMLOCK") return KEY_NUM_LOCK;
  if (k == "MENU") return KEY_MENU;
  if (k == "WINDOWS") return KEY_LEFT_GUI;
  if (k == "CTRL") return KEY_LEFT_CTRL;
  if (k == "ALT") return KEY_LEFT_ALT;
  if (k == "SHIFT") return KEY_LEFT_SHIFT;
  if (k == "KP_SLASH") return KEY_KP_SLASH;
  if (k == "KP_ASTERISK") return KEY_KP_ASTERISK;
  if (k == "KP_MINUS") return KEY_KP_MINUS;
  if (k == "KP_PLUS") return KEY_KP_PLUS;
  if (k == "KP_ENTER") return KEY_KP_ENTER;
  if (k.length() == 1) return (uint8_t)k.charAt(0);
  return 0;
}

void handleKeyTap() {
  String key = server.arg("key");
  uint8_t code = keyNameToCode(key);
  if (code != 0) sendKeyTap(code);
  server.send(200, "text/plain", "OK");
}

void handleKeyDown() {
  String key = server.arg("key");
  uint8_t code = keyNameToCode(key);
  if (code != 0) Keyboard.press(code);
  server.send(200, "text/plain", "OK");
}

void handleKeyUp() {
  String key = server.arg("key");
  uint8_t code = keyNameToCode(key);
  if (code != 0) Keyboard.release(code);
  server.send(200, "text/plain", "OK");
}

void handleToggleModifier() {
  String mod = server.arg("mod");
  toggleModifier(mod);
  server.send(200, "text/plain", "OK");
}

void handleResetModifiers() {
  releaseAllModifiers();
  server.send(200, "text/plain", "OK");
}

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0, user-scalable=no">
<title>ESP32 HID Controller</title>
<style>
  * { box-sizing: border-box; margin: 0; padding: 0; }
  body {
    font-family: 'Segoe UI', Roboto, sans-serif;
    background: #0b0b0b;
    color: #eee;
    padding: 12px;
    min-height: 100vh;
  }
  .container {
    max-width: 1200px;
    margin: 0 auto;
    display: grid;
    grid-template-columns: 1fr;
    gap: 16px;
  }
  @media (min-width: 780px) {
    .container { grid-template-columns: 1fr 1fr; }
    .full-width { grid-column: 1 / -1; }
  }
  .card {
    background: #1e1e1e;
    border-radius: 16px;
    padding: 18px;
    border: 1px solid #333;
    box-shadow: 0 8px 20px rgba(0,0,0,0.5);
  }
  h2 {
    font-size: 1.6rem;
    color: #5b9aff;
    text-align: center;
    margin-bottom: 10px;
    font-weight: 300;
    letter-spacing: 1px;
  }
  h3 {
    font-size: 1.2rem;
    color: #aaa;
    text-align: center;
    margin-bottom: 14px;
    font-weight: 400;
  }
  .slider-group {
    display: flex;
    flex-wrap: wrap;
    align-items: center;
    justify-content: center;
    gap: 10px 20px;
    margin: 8px 0;
  }
  .slider-group label {
    font-size: 14px;
    color: #ccc;
  }
  input[type=range] {
    flex: 1;
    min-width: 120px;
    height: 4px;
    -webkit-appearance: none;
    appearance: none;
    background: #444;
    border-radius: 2px;
    outline: none;
  }
  input[type=range]::-webkit-slider-thumb {
    -webkit-appearance: none;
    appearance: none;
    width: 16px;
    height: 16px;
    border-radius: 50%;
    background: #5b9aff;
    cursor: pointer;
  }
  .slider-value {
    min-width: 40px;
    text-align: center;
    color: #5b9aff;
    font-weight: 600;
  }
  .btn-group {
    display: flex;
    flex-wrap: wrap;
    gap: 8px;
    justify-content: center;
    margin: 8px 0;
  }
  button {
    padding: 8px 16px;
    background: #2a2a2a;
    color: #eee;
    border: 1px solid #444;
    border-radius: 8px;
    cursor: pointer;
    font-size: 14px;
    transition: 0.15s;
    font-weight: 500;
    box-shadow: 0 2px 4px rgba(0,0,0,0.3);
  }
  button:hover {
    background: #3a3a3a;
    transform: translateY(-1px);
  }
  button:active {
    transform: translateY(0);
    background: #444;
  }
  button.accent {
    background: #2c5f8a;
    border-color: #3a7bbd;
  }
  button.accent:hover { background: #3a7bbd; }
  button.mod-active {
    background: #f39c12;
    color: #000;
    border-color: #f1c40f;
  }
  button.pressed {
    background: #f39c12;
    color: #000;
    border-color: #f1c40f;
  }
  #pad {
    width: 100%;
    height: 200px;
    background: #181818;
    border-radius: 12px;
    border: 2px solid #333;
    touch-action: none;
    cursor: crosshair;
    margin: 10px 0;
    transition: border 0.2s;
  }
  #pad:active { border-color: #5b9aff; }
  .arrow-row {
    display: flex;
    justify-content: center;
    gap: 6px;
    margin: 4px 0;
  }
  .arrow-row button {
    min-width: 48px;
    height: 44px;
    font-size: 18px;
  }

  /* -------- Keyboard -------- */
  .kb-grid {
    display: grid;
    grid-template-columns: repeat(15, 1fr);
    gap: 4px;
    margin: 12px auto 0;
    max-width: 100%;
    width: fit-content;
    justify-content: center;
  }
  .kb-key {
    background: #2a2a2a;
    border: 1px solid #444;
    border-radius: 6px;
    color: #eee;
    display: flex;
    align-items: center;
    justify-content: center;
    font-size: 13px;
    padding: 4px 0;
    cursor: pointer;
    transition: 0.1s;
    user-select: none;
    min-height: 36px;
    min-width: 36px;
  }
  .kb-key:hover { background: #3a3a3a; }
  .kb-key:active { background: #444; }
  .kb-key.special { background: #2c3e50; }
  .kb-key.special:hover { background: #3e5a6f; }
  .kb-key.wide { grid-column: span 2; }
  .kb-key.space { grid-column: span 6; }
  .kb-key.mod-down { background: #5b9aff; color: #000; }
  .kb-key.last-clicked { background: #5b9aff; color: #000; border-color: #7ab7ff; }
  .kb-key.empty { visibility: hidden; pointer-events: none; }

  /* numpad */
  .numpad {
    display: grid;
    grid-template-columns: repeat(4, 1fr);
    gap: 4px;
    max-width: 180px;
    margin: 10px auto 0;
  }
  .numpad .kb-key { min-height: 34px; }
  .numpad .tall { grid-row: span 2; }
  .numpad .zero { grid-column: span 2; }

  .row-label {
    font-size: 12px;
    color: #666;
    text-align: center;
    margin: 6px 0 2px;
  }
  .text-input-area {
    display: flex;
    gap: 10px;
    flex-wrap: wrap;
    justify-content: center;
    margin: 10px 0;
  }
  input[type=text] {
    background: #222;
    border: 1px solid #444;
    border-radius: 8px;
    padding: 8px 14px;
    color: #eee;
    font-size: 16px;
    flex: 1;
    min-width: 160px;
    max-width: 380px;
    outline: none;
  }
  input[type=text]:focus { border-color: #5b9aff; }
  .small {
    font-size: 12px;
    color: #777;
    text-align: center;
    margin-top: 6px;
  }

  .log-panel {
    background: #121212;
    border: 1px solid #333;
    border-radius: 8px;
    padding: 10px;
    max-height: 180px;
    overflow-y: auto;
    margin-top: 10px;
    display: none;
    font-family: monospace;
    font-size: 12px;
  }
  .log-panel.visible { display: block; }
  .log-entry { padding: 2px 0; border-bottom: 1px solid #1a1a1a; }
  .log-info { color: #aaa; }
  .log-warn { color: #f39c12; }
  .log-error { color: #e74c3c; }
  .log-success { color: #2ecc71; }

  @media (max-width: 600px) {
    .kb-grid { font-size: 11px; gap: 3px; }
    .kb-key { min-height: 30px; padding: 2px 0; min-width: 28px; }
  }
</style>
</head>
<body>
<div class="container">
  <div class="card full-width">
    <h2>⚡ ESP32 HID Controller</h2>
    <div class="slider-group">
      <label>Sensitivity</label>
      <input type="range" id="sens" min="0.5" max="5" step="0.1" value="2.0">
      <span class="slider-value" id="sensVal">2.0</span>
      <label>Repeat (ms)</label>
      <input type="range" id="repeatRate" min="20" max="500" step="10" value="100">
      <span class="slider-value" id="repeatVal">100</span>
      <label>Legacy</label>
      <input type="checkbox" id="legacyCheck">
    </div>
    <div class="btn-group">
      <button onclick="testAll()">🧪 Test All</button>
      <button onclick="toggleLogs()">📋 Logs</button>
      <button onclick="clearLogs()">🗑 Clear</button>
    </div>
    <div id="logPanel" class="log-panel"></div>
  </div>

  <div class="card">
    <h3>🖱 Mouse</h3>
    <div id="pad"></div>
    <div class="arrow-row">
      <button data-dx="0" data-dy="-12">▲</button>
    </div>
    <div class="arrow-row">
      <button data-dx="-12" data-dy="0">◀</button>
      <button data-dx="0" data-dy="12">▼</button>
      <button data-dx="12" data-dy="0">▶</button>
    </div>
    <div class="btn-group">
      <button class="accent" onclick="sendHTTP('/click?btn=left')">Left</button>
      <button class="accent" onclick="sendHTTP('/click?btn=right')">Right</button>
      <button class="accent" onclick="sendHTTP('/click?btn=middle')">Middle</button>
      <button class="accent" onclick="sendHTTP('/double?btn=left')">Double</button>
    </div>
    <div class="btn-group">
      <button id="mouseLeftDown" class="mouse-down" data-btn="left" onclick="mouseDown('left')">L⬇</button>
      <button id="mouseLeftUp"   class="mouse-up"   data-btn="left" onclick="mouseUp('left')">L⬆</button>
      <button id="mouseRightDown" class="mouse-down" data-btn="right" onclick="mouseDown('right')">R⬇</button>
      <button id="mouseRightUp"   class="mouse-up"   data-btn="right" onclick="mouseUp('right')">R⬆</button>
      <button onclick="sendHTTP('/wheel?delta=-1')">⬆</button>
      <button onclick="sendHTTP('/wheel?delta=1')">⬇</button>
    </div>
    <div class="small">Drag on pad to move. Tap for left click. Arrows hold‑to‑repeat.</div>
  </div>

  <div class="card">
    <h3>⌨ Keyboard</h3>
    <div class="btn-group" id="modButtons">
      <button data-mod="CTRL">Ctrl</button>
      <button data-mod="ALT">Alt</button>
      <button data-mod="SHIFT">Shift</button>
      <button data-mod="WIN">Win</button>
    </div>

    <div class="text-input-area">
      <input type="text" id="textInput" placeholder="Type text to send...">
      <button class="accent" onclick="sendText()">Send</button>
    </div>
    <div class="text-input-area">
      <input type="text" id="realtimeInput" placeholder="Real‑time typing (backspace works)" autocomplete="off">
    </div>

    <div id="keyboard" class="kb-grid"></div>
    <div class="row-label">Numpad</div>
    <div id="numpad" class="numpad"></div>
    <div class="small">Sticky modifiers toggled via buttons above. Keyboard keys press/release on hold.</div>
  </div>
</div>

<script>
// ========== LOGGING ==========
let logs = [];
const LOG_KEY = 'esp32_logs';
function loadLogs() {
  try { const s = sessionStorage.getItem(LOG_KEY); if (s) logs = JSON.parse(s); } catch(e) {}
}
function saveLogs() { try { sessionStorage.setItem(LOG_KEY, JSON.stringify(logs)); } catch(e) {} }
function addLog(level, msg) {
  logs.push({ ts: new Date().toISOString(), level, msg });
  if (logs.length > 200) logs.shift();
  saveLogs();
  renderLogs();
}
function logInfo(m) { addLog('info', m); }
function logWarn(m) { addLog('warn', m); }
function logError(m) { addLog('error', m); }
function logSuccess(m) { addLog('success', m); }

function renderLogs() {
  const panel = document.getElementById('logPanel');
  if (!panel) return;
  panel.innerHTML = '';
  logs.forEach(e => {
    const div = document.createElement('div');
    div.className = 'log-entry log-' + e.level;
    div.textContent = `[${e.ts}] ${e.level.toUpperCase()}: ${e.msg}`;
    panel.appendChild(div);
  });
  panel.scrollTop = panel.scrollHeight;
}

function toggleLogs() {
  const panel = document.getElementById('logPanel');
  panel.classList.toggle('visible');
  if (panel.classList.contains('visible')) renderLogs();
}
function clearLogs() { logs = []; saveLogs(); renderLogs(); logInfo('Logs cleared'); }

loadLogs(); renderLogs();
logInfo('Page loaded');

// ========== NETWORK ==========
let socket = null;
function connectWS() {
  socket = new WebSocket('ws://' + location.hostname + ':81/');
  socket.onopen = () => { logSuccess('WebSocket connected'); };
  socket.onclose = () => { logWarn('WebSocket closed, reconnecting...'); setTimeout(connectWS, 2000); };
  socket.onerror = () => { logError('WebSocket error'); socket.close(); };
}
connectWS();

function sendHTTP(url) {
  fetch(url).then(res => {
    if (!res.ok) logWarn('HTTP ' + res.status + ' for ' + url);
    else logInfo('HTTP OK: ' + url);
  }).catch(err => logError('Fetch failed: ' + err.message));
}

function sendMove(dx, dy) {
  const realDx = Math.round(dx * sens);
  const realDy = Math.round(dy * sens);
  if (socket && socket.readyState === WebSocket.OPEN) {
    socket.send('{"dx":' + realDx + ',"dy":' + realDy + '}');
  } else {
    sendHTTP('/move?dx=' + realDx + '&dy=' + realDy);
  }
}

// ========== SETTINGS ==========
let sens = 2.0, repeatInterval = 100, legacyMode = false;

document.getElementById('sens').addEventListener('input', function() {
  sens = parseFloat(this.value);
  document.getElementById('sensVal').textContent = sens.toFixed(1);
  sendHTTP('/set_sensitivity?value=' + sens);
  logInfo('Sensitivity = ' + sens);
});
document.getElementById('repeatRate').addEventListener('input', function() {
  repeatInterval = parseInt(this.value);
  document.getElementById('repeatVal').textContent = repeatInterval;
  sendHTTP('/set_repeat?value=' + repeatInterval);
  logInfo('Repeat interval = ' + repeatInterval);
});
document.getElementById('legacyCheck').addEventListener('change', function() {
  legacyMode = this.checked;
  sendHTTP('/set_legacy?value=' + (legacyMode ? 1 : 0));
  logInfo('Legacy mode = ' + legacyMode);
});

// ========== MOUSE PAD ==========
const pad = document.getElementById('pad');
let padDown = false, startX, startY, lastX, lastY, moved, startTime;

pad.addEventListener('pointerdown', (e) => {
  pad.setPointerCapture(e.pointerId);
  padDown = true;
  startX = e.clientX; startY = e.clientY;
  lastX = e.clientX; lastY = e.clientY;
  moved = false; startTime = Date.now();
  e.preventDefault();
});
pad.addEventListener('pointermove', (e) => {
  if (!padDown) return;
  const dx = e.clientX - lastX, dy = e.clientY - lastY;
  lastX = e.clientX; lastY = e.clientY;
  if (Math.abs(e.clientX - startX) > 5 || Math.abs(e.clientY - startY) > 5) moved = true;
  if (moved) sendMove(dx, dy);
  e.preventDefault();
});
pad.addEventListener('pointerup', (e) => {
  if (!padDown) return;
  padDown = false;
  if (!moved && (Date.now() - startTime) < 300) {
    sendHTTP('/click?btn=left');
  }
  e.preventDefault();
});
pad.addEventListener('pointercancel', (e) => { padDown = false; });

// ========== ARROW REPEAT ==========
let repeatTimer = null;
document.querySelectorAll('.arrow-row button[data-dx]').forEach(btn => {
  const dx = parseInt(btn.dataset.dx), dy = parseInt(btn.dataset.dy);
  btn.addEventListener('pointerdown', () => {
    sendMove(dx, dy);
    repeatTimer = setInterval(() => sendMove(dx, dy), repeatInterval);
  });
  btn.addEventListener('pointerup', () => { clearInterval(repeatTimer); repeatTimer = null; });
  btn.addEventListener('pointerleave', () => { clearInterval(repeatTimer); repeatTimer = null; });
});

// ========== MOUSE HOLD STATE (visual toggles) ==========
const mouseState = { left: false, right: false };

function updateMouseUI() {
  document.getElementById('mouseLeftDown').classList.toggle('pressed', mouseState.left);
  document.getElementById('mouseRightDown').classList.toggle('pressed', mouseState.right);
}

function mouseDown(btn) {
  if (btn === 'left') { mouseState.left = true; sendHTTP('/down?btn=left'); }
  else if (btn === 'right') { mouseState.right = true; sendHTTP('/down?btn=right'); }
  updateMouseUI();
}

function mouseUp(btn) {
  if (btn === 'left') { mouseState.left = false; sendHTTP('/up?btn=left'); }
  else if (btn === 'right') { mouseState.right = false; sendHTTP('/up?btn=right'); }
  updateMouseUI();
}

// ========== MODIFIERS (sticky toggles) ==========
const modState = { CTRL: false, ALT: false, SHIFT: false, WIN: false };
document.querySelectorAll('#modButtons button').forEach(btn => {
  btn.addEventListener('click', () => {
    const mod = btn.dataset.mod;
    modState[mod] = !modState[mod];
    btn.classList.toggle('mod-active', modState[mod]);
    sendHTTP('/toggle_modifier?mod=' + mod);
    logInfo('Sticky ' + mod + ' = ' + modState[mod]);
  });
});

// ========== KEYBOARD LAYOUT ==========
const rows = [
  ['Esc','F1','F2','F3','F4','F5','F6','F7','F8','F9','F10','F11','F12','PrtSc','ScrLk','Pause'],
  ['`','1','2','3','4','5','6','7','8','9','0','-','=','Backspace'],
  ['Tab','q','w','e','r','t','y','u','i','o','p','[',']','\\'],
  ['CapsLock','a','s','d','f','g','h','j','k','l',';','\'','Enter'],
  ['Shift','z','x','c','v','b','n','m',',','.','/','Shift'],
  ['Ctrl','Win','Alt','Space','Alt','Win','Menu','Ctrl']
];

const keyMap = {
  'Esc':'ESC','Backspace':'BACKSPACE','Tab':'TAB','CapsLock':'CAPSLOCK','Enter':'ENTER',
  'Shift':'SHIFT','Ctrl':'CTRL','Alt':'ALT','Win':'WINDOWS','Space':'SPACE',
  'Insert':'INSERT','Home':'HOME','PageUp':'PAGEUP','Delete':'DELETE','End':'END','PageDown':'PAGEDOWN',
  'Up':'UP','Down':'DOWN','Left':'LEFT','Right':'RIGHT',
  'PrtSc':'PRTSC','ScrLk':'SCRLK','Pause':'PAUSE','NumLock':'NUMLOCK','Menu':'MENU',
  'Num /':'KP_SLASH','Num *':'KP_ASTERISK','Num -':'KP_MINUS','Num +':'KP_PLUS','Num Enter':'KP_ENTER'
};
for (let i=1; i<=12; i++) keyMap['F'+i] = 'F'+i;

let lastClickedKey = null;

function highlightKey(el) {
  if (lastClickedKey && lastClickedKey !== el) {
    lastClickedKey.classList.remove('last-clicked');
  }
  if (el) {
    el.classList.add('last-clicked');
    lastClickedKey = el;
  } else {
    lastClickedKey = null;
  }
}

function buildKeyboard() {
  const grid = document.getElementById('keyboard');
  grid.innerHTML = '';
  rows.forEach(row => {
    row.forEach(label => {
      const el = document.createElement('div');
      el.className = 'kb-key';
      if (label === '') { el.classList.add('empty'); el.textContent = ''; }
      else {
        el.textContent = label;
        if (['Backspace','Tab','CapsLock','Enter','Shift','Ctrl','Alt','Win','Menu'].includes(label))
          el.classList.add('wide');
        if (label === 'Space') el.classList.add('space');
        if (['Esc','F1','F2','F3','F4','F5','F6','F7','F8','F9','F10','F11','F12','PrtSc','ScrLk','Pause',
             'Insert','Home','PageUp','Delete','End','PageDown','Up','Down','Left','Right'].includes(label))
          el.classList.add('special');

        const isMod = ['Shift','Ctrl','Alt','Win'].includes(label);
        if (isMod) {
          // Modifier keys: press / release on pointer events
          el.addEventListener('pointerdown', (e) => {
            e.preventDefault();
            const code = keyMap[label];
            if (code) {
              sendHTTP('/key_down?key=' + code);
              el.classList.add('mod-down');
            }
          });
          el.addEventListener('pointerup', (e) => {
            e.preventDefault();
            const code = keyMap[label];
            if (code) {
              sendHTTP('/key_up?key=' + code);
              el.classList.remove('mod-down');
            }
          });
          el.addEventListener('pointerleave', () => {
            if (el.classList.contains('mod-down')) {
              const code = keyMap[label];
              if (code) sendHTTP('/key_up?key=' + code);
              el.classList.remove('mod-down');
            }
          });
        } else {
          // Normal key: tap and highlight
          el.addEventListener('click', () => {
            let code = keyMap[label];
            if (code) sendHTTP('/key?key=' + code);
            else if (label.length === 1) sendHTTP('/type?text=' + encodeURIComponent(label));
            else sendHTTP('/key?key=' + label);
            highlightKey(el);
          });
        }
      }
      grid.appendChild(el);
    });
  });
}

// ========== NUMPAD ==========
const numpadLayout = [
  ['NumLock','Num /','Num *','Num -'],
  ['7','8','9','Num +'],
  ['4','5','6'],
  ['1','2','3','Num Enter'],
  ['0','.','Num Enter']
];
function buildNumpad() {
  const container = document.getElementById('numpad');
  container.innerHTML = '';
  numpadLayout.forEach(row => {
    row.forEach(label => {
      const el = document.createElement('div');
      el.className = 'kb-key';
      if (label === 'Num +' || label === 'Num Enter') el.classList.add('tall');
      if (label === '0') el.classList.add('zero');
      el.textContent = label;
      const isMod = ['NumLock'].includes(label);
      if (isMod) {
        el.addEventListener('pointerdown', (e) => {
          e.preventDefault();
          const code = keyMap[label];
          if (code) { sendHTTP('/key_down?key=' + code); el.classList.add('mod-down'); }
        });
        el.addEventListener('pointerup', (e) => {
          e.preventDefault();
          const code = keyMap[label];
          if (code) { sendHTTP('/key_up?key=' + code); el.classList.remove('mod-down'); }
        });
        el.addEventListener('pointerleave', () => {
          if (el.classList.contains('mod-down')) {
            const code = keyMap[label];
            if (code) sendHTTP('/key_up?key=' + code);
            el.classList.remove('mod-down');
          }
        });
      } else {
        el.addEventListener('click', () => {
          let code = keyMap[label];
          if (code) sendHTTP('/key?key=' + code);
          else if (label.length === 1) sendHTTP('/type?text=' + encodeURIComponent(label));
          else sendHTTP('/key?key=' + label);
          highlightKey(el);
        });
      }
      container.appendChild(el);
    });
  });
}

buildKeyboard();
buildNumpad();

// ========== TEXT INPUT ==========
function sendText() {
  const val = document.getElementById('textInput').value;
  if (val) {
    sendHTTP('/type?text=' + encodeURIComponent(val));
    document.getElementById('textInput').value = '';
  }
}

let realtimeOld = '';
const realInput = document.getElementById('realtimeInput');
realInput.addEventListener('focus', () => { realtimeOld = realInput.value; });
realInput.addEventListener('input', function() {
  const newVal = this.value;
  let i = 0;
  while (i < realtimeOld.length && i < newVal.length && realtimeOld[i] === newVal[i]) i++;
  let j = 0;
  while (j < realtimeOld.length - i && j < newVal.length - i &&
         realtimeOld[realtimeOld.length - 1 - j] === newVal[newVal.length - 1 - j]) j++;
  const delCount = realtimeOld.length - i - j;
  for (let k=0; k<delCount; k++) {
    sendHTTP('/key?key=BACKSPACE');
  }
  const inserted = newVal.substring(i, newVal.length - j);
  for (let k=0; k<inserted.length; k++) {
    sendHTTP('/type?text=' + encodeURIComponent(inserted.charAt(k)));
  }
  realtimeOld = newVal;
});

// ========== TEST ==========
function sleep(ms) { return new Promise(r => setTimeout(r, ms)); }
async function testAll() {
  logInfo('=== Starting test ===');
  sendHTTP('/click?btn=left'); await sleep(200);
  sendHTTP('/click?btn=right'); await sleep(200);
  sendHTTP('/move?dx=30&dy=0'); await sleep(200);
  sendHTTP('/move?dx=0&dy=30'); await sleep(200);
  sendHTTP('/wheel?delta=1'); await sleep(200);
  sendHTTP('/type?text=Hello'); await sleep(300);
  sendHTTP('/key?key=ENTER'); await sleep(200);
  sendHTTP('/key?key=BACKSPACE'); await sleep(200);
  sendHTTP('/toggle_modifier?mod=SHIFT'); await sleep(200);
  sendHTTP('/type?text=a'); await sleep(200);
  sendHTTP('/toggle_modifier?mod=SHIFT'); await sleep(200);
  logSuccess('Test complete');
}

sendHTTP('/reset_modifiers');
logInfo('Modifiers reset');
</script>
</body>
</html>
)rawliteral";

void handleRoot() {
  server.send(200, "text/html", index_html);
}

void setup() {
  Serial.begin(115200);
  Mouse.begin();
  Keyboard.begin();
  USB.begin();
  WiFi.softAP(ssid, password);
  IPAddress apIP = WiFi.softAPIP();
  Serial.println("AP IP: " + apIP.toString());
  dnsServer.start(53, "*", apIP);

  server.on("/", handleRoot);
  server.on("/move", handleMove);
  server.on("/click", handleClick);
  server.on("/double", handleDoubleClick);
  server.on("/down", handleDown);
  server.on("/up", handleUp);
  server.on("/wheel", handleWheel);
  server.on("/set_sensitivity", handleSetSensitivity);
  server.on("/set_repeat", handleSetRepeatInterval);
  server.on("/set_legacy", handleSetLegacyMode);
  server.on("/type", handleType);
  server.on("/key", handleKeyTap);
  server.on("/key_down", handleKeyDown);
  server.on("/key_up", handleKeyUp);
  server.on("/toggle_modifier", handleToggleModifier);
  server.on("/reset_modifiers", handleResetModifiers);
  server.onNotFound([]() { server.send(200, "text/html", index_html); });

  server.begin();
  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
  Serial.println("Server ready");
}

void loop() {
  dnsServer.processNextRequest();
  server.handleClient();
  webSocket.loop();
  delay(1);
}
