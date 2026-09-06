// V1
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

void toggleModifier(const String& mod) {
  if (mod == "CTRL") ctrlPressed = !ctrlPressed;
  else if (mod == "ALT") altPressed = !altPressed;
  else if (mod == "SHIFT") shiftPressed = !shiftPressed;
  else if (mod == "WIN") winPressed = !winPressed;
  applyModifiers();
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t length) {
  if (type == WStype_TEXT) {
    String msg = String((char*)payload);
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

void handleType() {
  String text = server.arg("text");
  String asciiText = "";
  for (size_t i = 0; i < text.length(); i++) {
    char c = text.charAt(i);
    if (c >= 32 && c <= 126) asciiText += c;
  }

  for (size_t i = 0; i < asciiText.length(); i++) {
    char c = asciiText.charAt(i);
    Keyboard.press(c);
    delay(5);
    Keyboard.release(c);
    delay(5);
  }
  server.send(200, "text/plain", "OK");
}

void handleKeyTap() {
  String key = server.arg("key");
  key.toUpperCase();
  uint8_t keycode = 0;

  if (key == "ENTER") keycode = KEY_RETURN;
  else if (key == "BACKSPACE") keycode = KEY_BACKSPACE;
  else if (key == "TAB") keycode = KEY_TAB;
  else if (key == "SPACE") keycode = ' ';
  else if (key == "ESC") keycode = KEY_ESC;
  else if (key == "DELETE") keycode = KEY_DELETE;
  else if (key == "CAPSLOCK") keycode = KEY_CAPS_LOCK;
  else if (key == "UP") keycode = KEY_UP_ARROW;
  else if (key == "DOWN") keycode = KEY_DOWN_ARROW;
  else if (key == "LEFT") keycode = KEY_LEFT_ARROW;
  else if (key == "RIGHT") keycode = KEY_RIGHT_ARROW;
  else if (key == "HOME") keycode = KEY_HOME;
  else if (key == "END") keycode = KEY_END;
  else if (key == "PAGEUP") keycode = KEY_PAGE_UP;
  else if (key == "PAGEDOWN") keycode = KEY_PAGE_DOWN;
  else if (key == "INSERT") keycode = KEY_INSERT;
  else if (key == "F1") keycode = KEY_F1;
  else if (key == "F2") keycode = KEY_F2;
  else if (key == "F3") keycode = KEY_F3;
  else if (key == "F4") keycode = KEY_F4;
  else if (key == "F5") keycode = KEY_F5;
  else if (key == "F6") keycode = KEY_F6;
  else if (key == "F7") keycode = KEY_F7;
  else if (key == "F8") keycode = KEY_F8;
  else if (key == "F9") keycode = KEY_F9;
  else if (key == "F10") keycode = KEY_F10;
  else if (key == "F11") keycode = KEY_F11;
  else if (key == "F12") keycode = KEY_F12;
  else if (key == "PRTSC") keycode = KEY_PRINT_SCREEN;
  else if (key == "SCRLK") keycode = KEY_SCROLL_LOCK;
  else if (key == "PAUSE") keycode = KEY_PAUSE;
  else if (key == "NUMLOCK") keycode = KEY_NUM_LOCK;
  else if (key == "MENU") keycode = KEY_MENU;
  else if (key == "WINDOWS") keycode = KEY_LEFT_GUI;
  else if (key == "CTRL") keycode = KEY_LEFT_CTRL;
  else if (key == "ALT") keycode = KEY_LEFT_ALT;
  else if (key.length() == 1) {
    keycode = key.charAt(0);
  }

  if (keycode != 0) {
    Keyboard.press(keycode);
    delay(20);
    Keyboard.release(keycode);
  }
  server.send(200, "text/plain", "OK");
}

void handleToggleModifier() {
  String mod = server.arg("mod");
  toggleModifier(mod);
  server.send(200, "text/plain", "OK");
}

// ------------------------------------------------------------
// HTML page with improved keyboard layout
// ------------------------------------------------------------
const char index_html[] = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0, user-scalable=no">
<title>ESP32 Mouse + Keyboard</title>
<style>
  :root {
    --bg: #121212;
    --surface: #1e1e1e;
    --card: #2a2a2a;
    --card-border: #3a3a3a;
    --text: #ffffff;
    --text-secondary: #aaaaaa;
    --accent: #4a90e2;
    --accent-hover: #357abd;
    --key-bg: #3a3a3a;
    --key-bg-hover: #555555;
    --key-text: #ffffff;
    --special-key: #2c3e50;
    --mod-active: #f39c12;
    --shadow: 0 4px 6px rgba(0,0,0,0.3);
    --radius: 12px;
    --transition: all 0.2s ease;
  }
  * { box-sizing: border-box; margin: 0; padding: 0; }
  html, body {
    height: 100%;
    overflow: hidden;
    overscroll-behavior: none;
    font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, Oxygen, Ubuntu, Cantarell, sans-serif;
    background: var(--bg);
    color: var(--text);
  }
  body {
    display: flex;
    justify-content: center;
    align-items: flex-start;
    padding: 10px;
    -webkit-user-select: none;
    user-select: none;
    touch-action: manipulation;
  }
  .app-container {
    max-width: 1200px;
    width: 100%;
    max-height: 100vh;
    overflow-y: auto;
    padding: 10px;
    display: grid;
    grid-template-columns: 1fr;
    gap: 15px;
  }
  @media (min-width: 768px) {
    .app-container {
      grid-template-columns: 1fr 1fr;
      align-items: start;
    }
    .full-width {
      grid-column: 1 / -1;
    }
  }
  .card {
    background: var(--card);
    border: 1px solid var(--card-border);
    border-radius: var(--radius);
    padding: 16px;
    box-shadow: var(--shadow);
    transition: var(--transition);
  }
  .card:hover {
    border-color: #555;
  }
  h2 {
    font-size: 1.5rem;
    margin-bottom: 10px;
    color: var(--accent);
    text-align: center;
  }
  h3 {
    font-size: 1.2rem;
    margin-bottom: 12px;
    text-align: center;
    color: var(--text-secondary);
  }
  .slider-container {
    display: flex;
    align-items: center;
    gap: 10px;
    margin: 8px 0;
    flex-wrap: wrap;
    justify-content: center;
  }
  .slider-container label {
    min-width: 70px;
    font-size: 14px;
  }
  input[type=range] {
    flex: 1;
    min-width: 150px;
    accent-color: var(--accent);
  }
  .btn-group {
    display: flex;
    flex-wrap: wrap;
    gap: 8px;
    justify-content: center;
    margin: 8px 0;
  }
  button {
    padding: 10px 16px;
    font-size: 14px;
    border: none;
    border-radius: 8px;
    background: var(--accent);
    color: white;
    cursor: pointer;
    transition: var(--transition);
    min-width: 40px;
    box-shadow: 0 2px 4px rgba(0,0,0,0.2);
    font-weight: 500;
  }
  button:hover {
    background: var(--accent-hover);
    transform: translateY(-1px);
    box-shadow: 0 4px 8px rgba(0,0,0,0.3);
  }
  button:active {
    background: var(--accent-hover);
    transform: translateY(0);
    box-shadow: none;
  }
  button.mod-active {
    background: var(--mod-active);
    color: black;
  }
  #pad {
    width: 100%;
    height: 220px;
    background: #222;
    border-radius: 10px;
    margin: 10px 0;
    touch-action: none;
    cursor: crosshair;
    border: 2px solid #444;
    position: relative;
    overscroll-behavior: none;
    transition: border-color 0.2s;
  }
  #pad:active {
    border-color: var(--accent);
  }
  .arrow-row {
    display: flex;
    justify-content: center;
    gap: 5px;
    margin: 5px 0;
  }
  .keyboard-grid {
    display: flex;
    flex-direction: column;
    gap: 5px;
    margin-top: 10px;
    width: 100%;
    overflow-x: auto;
    touch-action: manipulation;
  }

  .keyboard-section {
    display: flex;
    flex-direction: column;
    gap: 5px;
    width: max-content;
    min-width: 100%;
  }
  .keyboard-section.main {
    flex: 2 1 400px;
    min-width: 300px;
  }
  .keyboard-section.middle {
    flex: 1 1 150px;
    min-width: 120px;
  }
  .keyboard-section.numpad {
    flex: 1 1 150px;
    min-width: 120px;
  }
  .kb-row {
    display: flex;
    gap: 5px;
    width: max-content;
    min-width: 100%;
    justify-content: flex-start;
  }

  .kb-key {
    flex: 0 0 42px;
    width: 42px;
    height: 42px;
    min-width: 42px;

    padding: 0;
    margin: 0;

    display: flex;
    align-items: center;
    justify-content: center;

    background: var(--key-bg);
    border: 1px solid #555;
    border-radius: 6px;
    color: var(--key-text);

    font-size: 13px;
    font-weight: normal;

    cursor: pointer;
    user-select: none;
    -webkit-user-select: none;
    touch-action: manipulation;
    -webkit-tap-highlight-color: transparent;
  }
  .kb-key:hover {
    background: var(--key-bg-hover);
    transform: translateY(-1px);
  }
  .kb-key:active {
    background: var(--key-bg-hover);
    transform: translateY(0);
  }
  .kb-key.wide {
    flex-basis: 65px;
    width: 65px;
  }

  .kb-key.space {
    flex-basis: 210px;
    width: 210px;
  }
  .kb-key.special {
    background: var(--special-key);
  }
  .kb-key.special:hover { background: #34495e; }
  .kb-key.mod-active {
    background: var(--mod-active);
    color: black;
  }

  .key-spacer {
    visibility: hidden;
    pointer-events: none;
  }
  .text-input-area {
    display: flex;
    gap: 10px;
    margin: 10px 0;
    align-items: center;
    justify-content: center;
    flex-wrap: wrap;
  }
  input[type=text] {
    padding: 10px 14px;
    font-size: 16px;
    border-radius: 8px;
    border: 1px solid #555;
    background: #222;
    color: white;
    flex: 1;
    min-width: 200px;
    max-width: 400px;
    outline: none;
    transition: border-color 0.2s;
  }
  input[type=text]:focus {
    border-color: var(--accent);
  }
  .small {
    font-size: 12px;
    color: var(--text-secondary);
    text-align: center;
    margin-top: 5px;
  }
  .app-container::-webkit-scrollbar {
    width: 8px;
  }
  .app-container::-webkit-scrollbar-track {
    background: #1a1a1a;
  }
  .app-container::-webkit-scrollbar-thumb {
    background: #444;
    border-radius: 4px;
  }
  .app-container::-webkit-scrollbar-thumb:hover {
    background: #666;
  }
</style>
</head>
<body>
<div class="app-container">
  <div class="card full-width">
    <h2>ESP32 Mouse + Keyboard</h2>
    <div class="slider-container">
      <label for="sens">Sensitivity:</label>
      <input type="range" id="sens" min="0.1" max="5" step="0.1" value="2.0" oninput="updateSens(this.value)">
      <span id="sensVal">2.0</span>
    </div>
    <div class="slider-container">
      <label for="repeatRate">Repeat (ms):</label>
      <input type="range" id="repeatRate" min="20" max="1000" step="10" value="100" oninput="updateRepeatRate(this.value)">
      <span id="repeatVal">100 ms</span>
    </div>
  </div>

  <div class="card">
    <h3>Mouse Control</h3>
    <div id="pad"></div>
    <div class="arrow-row">
      <button onpointerdown="startRepeat(0,-10)" onpointerup="stopRepeat()" onpointerleave="stopRepeat()">▲</button>
    </div>
    <div class="arrow-row">
      <button onpointerdown="startRepeat(-10,0)" onpointerup="stopRepeat()" onpointerleave="stopRepeat()">◀</button>
      <button onpointerdown="startRepeat(0,10)" onpointerup="stopRepeat()" onpointerleave="stopRepeat()">▼</button>
      <button onpointerdown="startRepeat(10,0)" onpointerup="stopRepeat()" onpointerleave="stopRepeat()">▶</button>
    </div>
    <div class="btn-group">
      <button onclick="sendHTTP('/click?btn=left')">Left Click</button>
      <button onclick="sendHTTP('/click?btn=right')">Right Click</button>
      <button onclick="sendHTTP('/click?btn=middle')">Middle Click</button>
      <button onclick="sendHTTP('/double?btn=left')">Double Click</button>
    </div>
    <div class="btn-group">
      <button onclick="sendHTTP('/down?btn=left')">L Down</button>
      <button onclick="sendHTTP('/up?btn=left')">L Up</button>
      <button onclick="sendHTTP('/down?btn=right')">R Down</button>
      <button onclick="sendHTTP('/up?btn=right')">R Up</button>
    </div>
    <div class="btn-group">
      <button onclick="sendHTTP('/wheel?delta=-1')">Wheel Up</button>
      <button onclick="sendHTTP('/wheel?delta=1')">Wheel Down</button>
    </div>
    <div class="small">Drag on pad to move. Tap for left click. Arrows support hold-to-repeat.</div>
  </div>

  <div class="card">
    <h3>Keyboard</h3>
    <div class="btn-group">
      <button id="modCtrl" onclick="toggleMod('CTRL')">Ctrl</button>
      <button id="modAlt" onclick="toggleMod('ALT')">Alt</button>
      <button id="modShift" onclick="toggleMod('SHIFT')">Shift</button>
      <button id="modWin" onclick="toggleMod('WIN')">Win</button>
    </div>
    <div class="text-input-area">
      <input type="text" id="textInput" placeholder="Type text here...">
      <button onclick="sendText()">Send Text</button>
    </div>
    <div class="small">ASCII only. Non-ASCII characters are filtered out.</div>
    <div class="text-input-area">
      <input type="text" id="realtimeInput" placeholder="Real-time typing..." autocomplete="off">
    </div>
    <div class="small">Type here and it will be sent live. Backspace works.</div>
    <div id="keyboard" class="keyboard-grid"></div>
  </div>
</div>

<script>
let sens = 2.0;
let repeatInterval = 100;
let socket = null;
let modifiers = { CTRL: false, ALT: false, SHIFT: false, WIN: false };
let repeatTimer = null;

function updateSens(val) {
  sens = parseFloat(val);
  document.getElementById('sensVal').textContent = sens.toFixed(1);
  fetch('/set_sensitivity?value=' + sens).catch(e=>console.error(e));
}

function updateRepeatRate(val) {
  repeatInterval = parseInt(val);
  document.getElementById('repeatVal').textContent = repeatInterval + ' ms';
  fetch('/set_repeat?value=' + repeatInterval).catch(e=>console.error(e));
  if (repeatTimer) {
    stopRepeat();
  }
}

function sendHTTP(url) {
  fetch(url).catch(e => console.error(e));
}

function connectWebSocket() {
  socket = new WebSocket('ws://' + location.hostname + ':81/');
  socket.onopen = function() { console.log('WS connected'); };
  socket.onclose = function() { setTimeout(connectWebSocket, 2000); };
  socket.onerror = function() { socket.close(); };
}

function sendMove(dx, dy) {
  let realDx = Math.round(dx * sens);
  let realDy = Math.round(dy * sens);
  if (socket && socket.readyState === WebSocket.OPEN) {
    socket.send('{"dx":' + realDx + ',"dy":' + realDy + '}');
  } else {
    sendHTTP('/move?dx=' + realDx + '&dy=' + realDy);
  }
}

function startRepeat(dx, dy) {
  sendMove(dx, dy);
  repeatTimer = setInterval(() => {
    sendMove(dx, dy);
  }, repeatInterval);
}

function stopRepeat() {
  if (repeatTimer) {
    clearInterval(repeatTimer);
    repeatTimer = null;
  }
}

const pad = document.getElementById('pad');
let lastX = null, lastY = null;
let down = false;
let startX = null, startY = null, startTime = 0;
let moved = false;

pad.addEventListener('touchmove', function(e) {
  e.preventDefault();
}, { passive: false });

pad.addEventListener('pointerdown', (e) => {
  pad.setPointerCapture(e.pointerId);
  down = true;
  startX = e.clientX;
  startY = e.clientY;
  startTime = Date.now();
  lastX = e.clientX;
  lastY = e.clientY;
  moved = false;
  e.preventDefault();
});

pad.addEventListener('pointermove', (e) => {
  if (!down) return;
  let dx = e.clientX - lastX;
  let dy = e.clientY - lastY;
  lastX = e.clientX;
  lastY = e.clientY;
  if (Math.abs(e.clientX - startX) > 5 || Math.abs(e.clientY - startY) > 5) moved = true;
  if (moved) {
    sendMove(dx, dy);
  }
  e.preventDefault();
});

function pointerUp(e) {
  if (!down) return;
  down = false;
  let elapsed = Date.now() - startTime;
  if (!moved && elapsed < 300) {
    sendHTTP('/click?btn=left');
  }
  lastX = lastY = null;
  e.preventDefault();
}

pad.addEventListener('pointerup', pointerUp);
pad.addEventListener('pointercancel', pointerUp);

function sendText() {
  let txt = document.getElementById('textInput').value;
  if (txt) {
    fetch('/type?text=' + encodeURIComponent(txt)).catch(e=>console.error(e));
    document.getElementById('textInput').value = '';
  }
}

// Real-time typing
let lastRealtimeValue = '';

document.getElementById('realtimeInput').addEventListener('input', function(e) {
  const currentValue = e.target.value;
  const oldValue = lastRealtimeValue;

  // Find common prefix length
  let prefixLen = 0;
  while (prefixLen < oldValue.length && prefixLen < currentValue.length && oldValue[prefixLen] === currentValue[prefixLen]) {
    prefixLen++;
  }

  // Find common suffix length (starting from the end, but not overlapping prefix)
  let suffixLen = 0;
  const maxSuffix = Math.min(oldValue.length - prefixLen, currentValue.length - prefixLen);
  while (suffixLen < maxSuffix && oldValue[oldValue.length - 1 - suffixLen] === currentValue[currentValue.length - 1 - suffixLen]) {
    suffixLen++;
  }

  // Deleted characters = oldValue.length - prefixLen - suffixLen
  const deletedCount = oldValue.length - prefixLen - suffixLen;
  for (let i = 0; i < deletedCount; i++) {
    sendHTTP('/key?key=BACKSPACE');
  }

  // Inserted characters = currentValue.length - prefixLen - suffixLen
  const insertedText = currentValue.substring(prefixLen, currentValue.length - suffixLen);
  if (insertedText.length > 0) {
    // Send each character individually (preserves modifiers if any are active)
    for (let i = 0; i < insertedText.length; i++) {
      sendHTTP('/type?text=' + encodeURIComponent(insertedText.charAt(i)));
    }
  }

  lastRealtimeValue = currentValue;
});

// Initialize last value
document.getElementById('realtimeInput').addEventListener('focus', function() {
  lastRealtimeValue = this.value;
});

function toggleMod(mod) {
  modifiers[mod] = !modifiers[mod];
  let btn = document.getElementById('mod' + mod);
  if (modifiers[mod]) btn.classList.add('mod-active');
  else btn.classList.remove('mod-active');
  fetch('/toggle_modifier?mod=' + mod).catch(e=>console.error(e));
}

// -------- Keyboard Layout Data --------
const mainRows = [
  ['Esc','F1','F2','F3','F4','F5','F6','F7','F8','F9','F10','F11','F12','PrtSc','ScrLk','Pause'],
  ['`','1','2','3','4','5','6','7','8','9','0','-','=','Backspace'],
  ['Tab','q','w','e','r','t','y','u','i','o','p','[',']','\\'],
  ['CapsLock','a','s','d','f','g','h','j','k','l',';','\'','Enter'],
  ['Shift','z','x','c','v','b','n','m',',','.','/','Shift'],
  ['Ctrl','Win','Alt','Space','Alt','Win','Menu','Ctrl']
];

const navRows = [
  ['Insert','Home','PageUp'],
  ['Delete','End','PageDown']
];

const arrowRows = [
  ['', 'Up', ''],
  ['Left', 'Down', 'Right']
];

const numpadRows = [
  ['NumLock','/','*','-'],
  ['7','8','9','+'],
  ['4','5','6','+'],
  ['1','2','3','Enter'],
  ['0','.','Enter']
];

const specialKeys = {
  'Esc': 'ESC',
  'Backspace': 'BACKSPACE',
  'Tab': 'TAB',
  'CapsLock': 'CAPSLOCK',
  'Enter': 'ENTER',
  'Shift': 'SHIFT',
  'Ctrl': 'CTRL',
  'Alt': 'ALT',
  'Win': 'WINDOWS',
  'Space': 'SPACE',
  'Insert': 'INSERT',
  'Home': 'HOME',
  'PageUp': 'PAGEUP',
  'Delete': 'DELETE',
  'End': 'END',
  'PageDown': 'PAGEDOWN',
  'Up': 'UP',
  'Down': 'DOWN',
  'Left': 'LEFT',
  'Right': 'RIGHT',
  'PrtSc': 'PRTSC',
  'ScrLk': 'SCRLK',
  'Pause': 'PAUSE',
  'NumLock': 'NUMLOCK',
  'Menu': 'MENU'
};

for (let i = 1; i <= 12; i++) {
  specialKeys['F' + i] = 'F' + i;
}

let shiftActive = false;

function createKeyButton(key) {
  const btn = document.createElement('button');
  btn.className = 'kb-key';
  btn.textContent = key;

  if (specialKeys[key]) {
    btn.classList.add('special');
    if (['Backspace','Tab','CapsLock','Enter','Shift','Ctrl','Alt','Win','Space'].includes(key)) {
      btn.classList.add('wide');
      if (key === 'Space') btn.classList.add('space');
    }
  }
  if (key === '') {
    btn.classList.add('key-spacer');
  }

  // For Ctrl, Alt, Win: support both tap and long‑press toggle
  if (['Ctrl','Alt','Win'].includes(key)) {
    let pressTimer = null;
    let longPressTriggered = false;

    btn.addEventListener('pointerdown', (e) => {
      longPressTriggered = false;
      pressTimer = setTimeout(() => {
        longPressTriggered = true;
        let modString = key.toUpperCase(); // Fix: map to uppercase
        toggleMod(modString);
      }, 400);
    });

    btn.addEventListener('pointerup', (e) => {
      clearTimeout(pressTimer);
      if (!longPressTriggered) {
        handleKeyClick(key); // short press = tap
      }
    });

    btn.addEventListener('pointerleave', (e) => {
      clearTimeout(pressTimer);
      longPressTriggered = false; // reset just in case
    });

    btn.addEventListener('click', (e) => {
      e.preventDefault();
      e.stopPropagation();
    });
  } else {
    btn.addEventListener('pointerup', (e) => {
      e.preventDefault();
      handleKeyClick(key);
    });
  }

  return btn;
}

function generateKeyboard() {
  const kbDiv = document.getElementById('keyboard');
  kbDiv.innerHTML = '';

  // Main keyboard
  mainRows.forEach(row => {
    const rowDiv = document.createElement('div');
    rowDiv.className = 'kb-row';

    row.forEach(key => {
      const btn = createKeyButton(key);

      if (key === 'Shift' && shiftActive) {
        btn.classList.add('mod-active');
      }

      rowDiv.appendChild(btn);
    });

    kbDiv.appendChild(rowDiv);
  });

  // Bottom navigation area
  const bottom = document.createElement('div');
  bottom.style.display = 'flex';
  bottom.style.gap = '25px';
  bottom.style.marginTop = '10px';
  bottom.style.width = 'max-content';

  // Navigation
  const nav = document.createElement('div');
  nav.className = 'keyboard-section';

  navRows.forEach(row => {
    const rowDiv = document.createElement('div');
    rowDiv.className = 'kb-row';

    row.forEach(key => {
      rowDiv.appendChild(createKeyButton(key));
    });

    nav.appendChild(rowDiv);
  });

  // Arrows
  const arrows = document.createElement('div');
  arrows.className = 'keyboard-section';

  arrowRows.forEach(row => {
    const rowDiv = document.createElement('div');
    rowDiv.className = 'kb-row';

    row.forEach(key => {
      const btn = createKeyButton(key);

      if (key === '') {
        btn.classList.add('key-spacer');
      }

      rowDiv.appendChild(btn);
    });

    arrows.appendChild(rowDiv);
  });

  // Numpad
  const numpad = document.createElement('div');
  numpad.className = 'keyboard-section';

  numpadRows.forEach(row => {
    const rowDiv = document.createElement('div');
    rowDiv.className = 'kb-row';

    row.forEach(key => {
      rowDiv.appendChild(createKeyButton(key));
    });

    numpad.appendChild(rowDiv);
  });

  bottom.appendChild(nav);
  bottom.appendChild(arrows);
  bottom.appendChild(numpad);

  kbDiv.appendChild(bottom);
}

function handleKeyClick(key) {
  if (key === '') return;

  // Shift still toggles (for uppercase)
  if (key === 'Shift') {
    shiftActive = !shiftActive;
    if (shiftActive) {
      modifiers.SHIFT = true;
      document.getElementById('modShift').classList.add('mod-active');
    } else {
      modifiers.SHIFT = false;
      document.getElementById('modShift').classList.remove('mod-active');
    }
    fetch('/toggle_modifier?mod=SHIFT').catch(e=>console.error(e));
    generateKeyboard();
    return;
  }

  // For Ctrl, Alt, Win: we are here because of a short press, so send a tap
  if (['Ctrl','Alt','Win'].includes(key)) {
    fetch('/key?key=' + specialKeys[key]).catch(e=>console.error(e));
    return;
  }

  // Special keys (non-modifier)
  if (specialKeys[key]) {
    fetch('/key?key=' + specialKeys[key]).catch(e=>console.error(e));
    return;
  }

  // Regular character
  let charToSend = key;
  if (modifiers.SHIFT || shiftActive) {
    charToSend = key.toUpperCase();
  }
  fetch('/type?text=' + encodeURIComponent(charToSend)).catch(e=>console.error(e));
}

// Initialize
connectWebSocket();
generateKeyboard();
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
  server.on("/type", handleType);
  server.on("/key", handleKeyTap);
  server.on("/toggle_modifier", handleToggleModifier);

  server.onNotFound([]() {
    server.send(200, "text/html", index_html);
  });

  server.begin();
  webSocket.begin();
  webSocket.onEvent(webSocketEvent);

  Serial.println("Web server started");
}

void loop() {
  dnsServer.processNextRequest();
  server.handleClient();
  webSocket.loop();
  delay(1);
}

.keyboard-grid {
  display: flex;
  flex-direction: column;
  gap: 5px;
  margin-top: 10px;
  width: 100%;
  overflow-x: hidden;
  touch-action: manipulation;
}

.keyboard-section {
  display: flex;
  flex-direction: column;
  gap: 5px;
  width: 100%;
  min-width: 0;
}

.keyboard-section.main {
  flex: none;
  min-width: 0;
}

.keyboard-section.middle {
  flex: none;
  min-width: 0;
}

.keyboard-section.numpad {
  flex: none;
  min-width: 0;
}

.kb-row {
  display: flex;
  gap: 5px;
  width: 100%;
  min-width: 0;
  justify-content: flex-start;
}

.kb-key {
  flex: 1 1 0;
  width: 0;
  min-width: 0;
  height: clamp(30px, 7vw, 42px);

  padding: 0;
  margin: 0;

  display: flex;
  align-items: center;
  justify-content: center;

  background: var(--key-bg);
  border: 1px solid #555;
  border-radius: 6px;
  color: var(--key-text);

  font-size: clamp(8px, 1.8vw, 13px);
  font-weight: normal;

  cursor: pointer;
  user-select: none;
  -webkit-user-select: none;
  touch-action: manipulation;
  -webkit-tap-highlight-color: transparent;
}

.kb-key:hover {
  background: var(--key-bg-hover);
  transform: translateY(-1px);
}

.kb-key:active {
  background: var(--key-bg-hover);
  transform: translateY(0);
}

.kb-key.wide {
  flex: 1.5 1 0;
  width: 0;
}

.kb-key.space {
  flex: 5 1 0;
  width: 0;
}

.kb-key.special {
  background: var(--special-key);
}

.kb-key.special:hover {
  background: #34495e;
}

.kb-key.mod-active {
  background: var(--mod-active);
  color: black;
}

.key-spacer {
  visibility: hidden;
  pointer-events: none;
}
