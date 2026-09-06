// V4
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <WebSocketsServer.h>
#include <USB.h>
#include <USBHIDMouse.h>
#include <USBHIDKeyboard.h>
#include <Preferences.h>
#include <cstdarg>
#include <cstdio>

USBHIDMouse Mouse;
USBHIDKeyboard Keyboard;
WebServer server(80);
DNSServer dnsServer;
WebSocketsServer webSocket(81);

Preferences preferences;

const char* ap_ssid = "ESP32-Mouse";
const char* ap_password = "12345678";

// Persistent settings
float sensitivity = 2.0;
int repeatInterval = 100;
bool legacyMode = false;

bool ctrlPressed = false;
bool altPressed = false;
bool shiftPressed = false;
bool winPressed = false;

// STA status
String sta_ssid = "";
String sta_ip = "";
String sta_status = "Disconnected";
String sta_error = "";
bool scanInProgress = false;
int sta_retry_count = 0;
const int MAX_RETRIES = 3;
const unsigned long CONNECT_TIMEOUT = 10000;  // 10 seconds
unsigned long connectStartTime = 0;
bool connecting = false;

unsigned long lastRetryTime = 0;
const unsigned long RETRY_INTERVAL = 5000;  // 5 seconds between retries
bool retryPending = false;

// ---------- Logging System ----------
#define MAX_LOG_ENTRIES 200

struct LogEntry {
  unsigned long timestamp;
  char level[8];
  char message[256];
};

LogEntry logBuffer[MAX_LOG_ENTRIES];
int logHead = 0;
int logCount = 0;

void addLog(const char* level, const char* format, ...) {
  char msg[256];
  va_list args;
  va_start(args, format);
  vsnprintf(msg, sizeof(msg), format, args);
  va_end(args);

  LogEntry* entry = &logBuffer[logHead];
  entry->timestamp = millis();
  strncpy(entry->level, level, sizeof(entry->level) - 1);
  entry->level[sizeof(entry->level) - 1] = '\0';
  strncpy(entry->message, msg, sizeof(entry->message) - 1);
  entry->message[sizeof(entry->message) - 1] = '\0';

  logHead = (logHead + 1) % MAX_LOG_ENTRIES;
  if (logCount < MAX_LOG_ENTRIES) logCount++;

  // Also print to serial
  Serial.printf("[%lu] [%s] %s\n", millis(), level, msg);
}

#define LOG_INFO(...) addLog("INFO", __VA_ARGS__)
#define LOG_WARN(...) addLog("WARN", __VA_ARGS__)
#define LOG_ERROR(...) addLog("ERROR", __VA_ARGS__)
#define LOG_SUCCESS(...) addLog("SUCCESS", __VA_ARGS__)

// ---------- Helper functions ----------
int16_t clamp(int16_t v, int16_t minv, int16_t maxv) {
  if (v < minv) return minv;
  if (v > maxv) return maxv;
  return v;
}

String jsonEscape(const String& input) {
  String out;
  out.reserve(input.length() + 8);
  for (size_t i = 0; i < input.length(); ++i) {
    char c = input.charAt(i);
    switch (c) {
      case '\\': out += "\\\\"; break;
      case '"':  out += "\\\""; break;
      case '\n': out += "\\n"; break;
      case '\r': out += "\\r"; break;
      case '\t': out += "\\t"; break;
      case '\b': out += "\\b"; break;
      case '\f': out += "\\f"; break;
      default:
        if ((uint8_t)c < 0x20) {
          char buf[7];
          snprintf(buf, sizeof(buf), "\\u%04x", (unsigned char)c);
          out += buf;
        } else {
          out += c;
        }
        break;
    }
  }
  return out;
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
  LOG_INFO("All modifiers released");
}

void toggleModifier(const String& mod) {
  if (mod == "CTRL") ctrlPressed = !ctrlPressed;
  else if (mod == "ALT") altPressed = !altPressed;
  else if (mod == "SHIFT") shiftPressed = !shiftPressed;
  else if (mod == "WIN") winPressed = !winPressed;
  applyModifiers();
  LOG_INFO("Toggled modifier %s -> %d", mod.c_str(), (mod == "CTRL" ? ctrlPressed : (mod == "ALT" ? altPressed : (mod == "SHIFT" ? shiftPressed : winPressed))));
}

// ---------- Persistent settings ----------
void loadSettings() {
  preferences.begin("settings", true);
  sensitivity = preferences.getFloat("sens", 2.0);
  repeatInterval = preferences.getInt("repeat", 100);
  legacyMode = preferences.getBool("legacy", false);
  preferences.end();

  if (sensitivity < 0.1f) sensitivity = 0.1f;
  if (sensitivity > 10.0f) sensitivity = 10.0f;
  if (repeatInterval < 20) repeatInterval = 20;
  if (repeatInterval > 1000) repeatInterval = 1000;

  LOG_INFO("Settings loaded: sens=%.1f, repeat=%d, legacy=%d", sensitivity, repeatInterval, legacyMode);
}

void saveSettings() {
  preferences.begin("settings", false);
  preferences.putFloat("sens", sensitivity);
  preferences.putInt("repeat", repeatInterval);
  preferences.putBool("legacy", legacyMode);
  preferences.end();
  LOG_INFO("Settings saved");
}

// ---------- WiFi STA management ----------
void setSTAErrorFromStatus() {
  wl_status_t status = WiFi.status();
  switch (status) {
    case WL_NO_SSID_AVAIL: sta_error = "SSID not found"; break;
    case WL_CONNECT_FAILED: sta_error = "Connection failed"; break;
#ifdef WL_WRONG_PASSWORD
    case WL_WRONG_PASSWORD: sta_error = "Wrong password"; break;
#endif
    case WL_IDLE_STATUS: sta_error = "Idle"; break;
    case WL_DISCONNECTED: sta_error = "Disconnected"; break;
#ifdef WL_CONNECTION_LOST
    case WL_CONNECTION_LOST: sta_error = "Connection lost"; break;
#endif
    default: sta_error = "Unknown error"; break;
  }
}

void updateSTAStatus(bool logStatus = false) {
  if (WiFi.status() == WL_CONNECTED) {
    bool changed = (sta_status != "Connected" || sta_ip != WiFi.localIP().toString() || sta_ssid != WiFi.SSID());
    sta_status = "Connected";
    sta_ip = WiFi.localIP().toString();
    sta_ssid = WiFi.SSID();
    sta_error = "";
    connecting = false;
    retryPending = false;
    sta_retry_count = 0;
    if (logStatus || changed) {
      LOG_SUCCESS("STA connected to %s, IP %s", sta_ssid.c_str(), sta_ip.c_str());
    }
  } else {
    sta_status = connecting ? "Connecting..." : (retryPending ? "Retrying..." : "Disconnected");
    sta_ip = "";
    sta_ssid = "";
    setSTAErrorFromStatus();
    if (retryPending) {
      sta_error = "Retry scheduled";
    }
    if (connecting && millis() - connectStartTime > CONNECT_TIMEOUT) {
      sta_error = "Connection timeout";
    }
    if (logStatus) {
      LOG_WARN("STA status: %s, error: %s", sta_status.c_str(), sta_error.c_str());
    }
  }
}

void WiFiEvent(WiFiEvent_t event) {
  switch (event) {
    case ARDUINO_EVENT_WIFI_STA_CONNECTED:
      sta_status = "Connecting...";
      sta_error = "Connected, waiting for IP...";
      LOG_INFO("STA connected to AP, waiting for IP");
      break;

    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
      updateSTAStatus(true);
      break;

    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
      if (!retryPending) {
        sta_status = connecting ? "Connecting..." : "Disconnected";
        setSTAErrorFromStatus();
      }
      LOG_WARN("STA disconnected: %s", sta_error.c_str());
      break;

    default:
      break;
  }
}

bool parseBSSID(const String& text, uint8_t out[6]) {
  if (text.length() != 17) return false;
  unsigned int b[6];
  if (sscanf(text.c_str(), "%2x:%2x:%2x:%2x:%2x:%2x",
             &b[0], &b[1], &b[2], &b[3], &b[4], &b[5]) != 6) {
    return false;
  }
  for (int i = 0; i < 6; ++i) out[i] = (uint8_t)b[i];
  return true;
}

void connectSTA(String ssid, String password, bool hidden, String bssid_str, bool resetRetries = true) {
  if (ssid.length() == 0) {
    LOG_ERROR("connectSTA called with empty SSID");
    return;
  }

  if (WiFi.status() == WL_CONNECTED) {
    LOG_WARN("connectSTA aborted: already connected");
    return;
  }

  if (connecting) {
    LOG_WARN("connectSTA aborted: connection already in progress");
    return;
  }

  if (resetRetries) {
    sta_retry_count = 0;
    retryPending = false;
  }

  // Cancel any completed scan results before changing STA state.
  int scanState = WiFi.scanComplete();
  if (scanState >= 0) {
    WiFi.scanDelete();
  }
  scanInProgress = false;

  WiFi.disconnect(true);
  delay(100);
  WiFi.mode(WIFI_AP_STA);

  preferences.begin("wifi", false);
  preferences.putString("ssid", ssid);
  preferences.putString("pass", password);
  preferences.putBool("hidden", hidden);
  preferences.putString("bssid", hidden ? bssid_str : "");
  preferences.end();

  connecting = true;
  retryPending = false;
  connectStartTime = millis();
  lastRetryTime = millis();
  sta_error = "Connecting...";
  sta_status = "Connecting...";

  LOG_INFO("Connecting to STA: %s (hidden=%d, bssid=%s, attempt=%d)",
           ssid.c_str(), hidden, bssid_str.c_str(), sta_retry_count + 1);

  if (hidden && bssid_str.length() > 0) {
    uint8_t bssid[6];
    if (parseBSSID(bssid_str, bssid)) {
      WiFi.begin(ssid.c_str(), password.c_str(), 0, bssid);
    } else {
      LOG_WARN("Invalid BSSID format: %s; connecting without BSSID", bssid_str.c_str());
      WiFi.begin(ssid.c_str(), password.c_str());
    }
  } else {
    WiFi.begin(ssid.c_str(), password.c_str());
  }
}

void loadSTAConfig() {
  preferences.begin("wifi", true);
  String ssid = preferences.getString("ssid", "");
  String pass = preferences.getString("pass", "");
  bool hidden = preferences.getBool("hidden", false);
  String bssid = preferences.getString("bssid", "");
  preferences.end();
  if (ssid.length() > 0) {
    LOG_INFO("Loading saved STA config: %s", ssid.c_str());
    connectSTA(ssid, pass, hidden, bssid, true);
  } else {
    LOG_INFO("No saved STA config found");
  }
}

void disconnectSTA() {
  retryPending = false;
  connecting = false;
  sta_retry_count = 0;
  scanInProgress = false;
  int scanState = WiFi.scanComplete();
  if (scanState >= 0) WiFi.scanDelete();
  WiFi.disconnect();
  WiFi.mode(WIFI_AP);
  sta_status = "Disconnected";
  sta_ip = "";
  sta_ssid = "";
  sta_error = "Disconnected";
  LOG_INFO("STA disconnected manually");
}

void forgetSTA() {
  preferences.begin("wifi", false);
  preferences.clear();
  preferences.end();
  disconnectSTA();
  LOG_INFO("STA credentials forgotten");
}

// ---------- WebSocket event ----------
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
    } else {
      LOG_WARN("WebSocket received unknown message: %s", msg.c_str());
    }
  } else if (type == WStype_CONNECTED) {
    LOG_INFO("WebSocket client connected, id=%u", num);
  } else if (type == WStype_DISCONNECTED) {
    LOG_INFO("WebSocket client disconnected, id=%u", num);
  }
}

// ---------- HTTP endpoints (HID) ----------
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
  if (btn == "right") {
    Mouse.click(MOUSE_RIGHT);
    LOG_INFO("Mouse click right");
  } else if (btn == "middle") {
    Mouse.click(MOUSE_MIDDLE);
    LOG_INFO("Mouse click middle");
  } else {
    Mouse.click(MOUSE_LEFT);
    LOG_INFO("Mouse click left");
  }
  server.send(200, "text/plain", "OK");
}

void handleDoubleClick() {
  String btn = server.arg("btn");
  if (btn == "right") {
    Mouse.click(MOUSE_RIGHT);
    delay(50);
    Mouse.click(MOUSE_RIGHT);
    LOG_INFO("Mouse double click right");
  } else {
    Mouse.click(MOUSE_LEFT);
    delay(50);
    Mouse.click(MOUSE_LEFT);
    LOG_INFO("Mouse double click left");
  }
  server.send(200, "text/plain", "OK");
}

void handleDown() {
  String btn = server.arg("btn");
  if (btn == "right") {
    Mouse.press(MOUSE_RIGHT);
    LOG_INFO("Mouse press right");
  } else if (btn == "middle") {
    Mouse.press(MOUSE_MIDDLE);
    LOG_INFO("Mouse press middle");
  } else {
    Mouse.press(MOUSE_LEFT);
    LOG_INFO("Mouse press left");
  }
  server.send(200, "text/plain", "OK");
}

void handleUp() {
  String btn = server.arg("btn");
  if (btn == "right") {
    Mouse.release(MOUSE_RIGHT);
    LOG_INFO("Mouse release right");
  } else if (btn == "middle") {
    Mouse.release(MOUSE_MIDDLE);
    LOG_INFO("Mouse release middle");
  } else {
    Mouse.release(MOUSE_LEFT);
    LOG_INFO("Mouse release left");
  }
  server.send(200, "text/plain", "OK");
}

void handleWheel() {
  int delta = server.arg("delta").toInt();
  delta = clamp(delta, -127, 127);
  Mouse.move(0, 0, delta);
  LOG_INFO("Mouse wheel delta=%d", delta);
  server.send(200, "text/plain", "OK");
}

void handleSetSensitivity() {
  float val = server.arg("value").toFloat();
  if (val < 0.1) val = 0.1;
  if (val > 10.0) val = 10.0;
  sensitivity = val;
  saveSettings();
  LOG_INFO("Sensitivity set to %.1f", sensitivity);
  server.send(200, "text/plain", "OK");
}

void handleSetRepeatInterval() {
  int val = server.arg("value").toInt();
  if (val < 20) val = 20;
  if (val > 1000) val = 1000;
  repeatInterval = val;
  saveSettings();
  LOG_INFO("Repeat interval set to %d ms", repeatInterval);
  server.send(200, "text/plain", "OK");
}

void handleSetLegacyMode() {
  int val = server.arg("value").toInt();
  legacyMode = (val == 1);
  saveSettings();
  LOG_INFO("Legacy mode set to %d", legacyMode);
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
  LOG_INFO("Key tap: 0x%02X", keycode);
}

void handleType() {
  String text = server.arg("text");
  String asciiText = "";
  for (size_t i = 0; i < text.length(); i++) {
    char c = text.charAt(i);
    if (c >= 32 && c <= 126) asciiText += c;
  }
  LOG_INFO("Typing text: %s", asciiText.c_str());
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
  if (code != 0) {
    sendKeyTap(code);
    LOG_INFO("Key tap: %s (0x%02X)", key.c_str(), code);
  } else {
    LOG_WARN("Unknown key: %s", key.c_str());
  }
  server.send(200, "text/plain", "OK");
}

void handleKeyDown() {
  String key = server.arg("key");
  uint8_t code = keyNameToCode(key);
  if (code != 0) {
    Keyboard.press(code);
    LOG_INFO("Key down: %s (0x%02X)", key.c_str(), code);
  } else {
    LOG_WARN("Unknown key down: %s", key.c_str());
  }
  server.send(200, "text/plain", "OK");
}

void handleKeyUp() {
  String key = server.arg("key");
  uint8_t code = keyNameToCode(key);
  if (code != 0) {
    Keyboard.release(code);
    LOG_INFO("Key up: %s (0x%02X)", key.c_str(), code);
  } else {
    LOG_WARN("Unknown key up: %s", key.c_str());
  }
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

// ---------- STA endpoints ----------
void handleSTAStatus() {
  String json = "{";
  json += "\"connected\":" + String(WiFi.status() == WL_CONNECTED ? "true" : "false");
  json += ",\"ssid\":\"" + jsonEscape(sta_ssid) + "\"";
  json += ",\"ip\":\"" + jsonEscape(sta_ip) + "\"";
  json += ",\"status\":\"" + jsonEscape(sta_status) + "\"";
  json += ",\"error\":\"" + jsonEscape(sta_error) + "\"";
  json += "}";
  server.send(200, "application/json", json);
}

void handleSTAScan() {
  int n = WiFi.scanComplete();

  if (n == WIFI_SCAN_RUNNING) {
    scanInProgress = true;
    server.send(200, "application/json", "{\"scanning\":true}");
    return;
  }

  if (n == WIFI_SCAN_FAILED) {
    if (!scanInProgress) {
      WiFi.mode(WIFI_AP_STA);
      scanInProgress = true;

      LOG_INFO("Starting WiFi scan...");
      int result = WiFi.scanNetworks(true, true);
      if (result == WIFI_SCAN_FAILED) {
        scanInProgress = false;
        LOG_ERROR("Failed to start WiFi scan");
        server.send(503, "application/json", "{\"error\":\"WiFi scan failed to start\"}");
        return;
      }

      server.send(200, "application/json", "{\"scanning\":true}");
      return;
    }

    scanInProgress = false;
    LOG_ERROR("WiFi scan failed");
    server.send(503, "application/json", "{\"error\":\"WiFi scan failed\"}");
    return;
  }

  if (n >= 0) {
    String json = "[";
    json.reserve((size_t)n * 110 + 4);

    for (int i = 0; i < n; ++i) {
      if (i > 0) json += ",";

      String ssid = WiFi.SSID(i);
      String bssid = WiFi.BSSIDstr(i);
      int rssi = WiFi.RSSI(i);
      wifi_auth_mode_t encryption = WiFi.encryptionType(i);

      String encType;
      switch (encryption) {
        case WIFI_AUTH_OPEN: encType = "Open"; break;
        case WIFI_AUTH_WEP: encType = "WEP"; break;
        case WIFI_AUTH_WPA_PSK: encType = "WPA"; break;
        case WIFI_AUTH_WPA2_PSK: encType = "WPA2"; break;
        case WIFI_AUTH_WPA_WPA2_PSK: encType = "WPA/WPA2"; break;
        case WIFI_AUTH_WPA2_ENTERPRISE: encType = "WPA2-Enterprise"; break;
#ifdef WIFI_AUTH_WPA3_PSK
        case WIFI_AUTH_WPA3_PSK: encType = "WPA3"; break;
#endif
#ifdef WIFI_AUTH_WPA2_WPA3_PSK
        case WIFI_AUTH_WPA2_WPA3_PSK: encType = "WPA2/WPA3"; break;
#endif
        default: encType = "Unknown"; break;
      }

      json += "{\"ssid\":\"" + jsonEscape(ssid) + "\",";
      json += "\"rssi\":" + String(rssi) + ",";
      json += "\"encryption\":" + String((int)encryption) + ",";
      json += "\"bssid\":\"" + jsonEscape(bssid) + "\",";
      json += "\"encryption_str\":\"" + jsonEscape(encType) + "\"}";
    }

    json += "]";

    LOG_INFO("WiFi scan completed, %d networks found", n);
    scanInProgress = false;
    WiFi.scanDelete();

    server.send(200, "application/json", json);
    return;
  }

  scanInProgress = false;
  LOG_ERROR("Unexpected WiFi scan state: %d", n);
  server.send(500, "application/json", "{\"error\":\"Unexpected scan state\"}");
}

void handleSTAConnect() {
  String ssid = server.arg("ssid");
  String password = server.arg("pass");
  String hiddenStr = server.arg("hidden");
  String bssid = server.arg("bssid");
  bool hidden = (hiddenStr == "1" || hiddenStr == "true");

  if (ssid.length() == 0) {
    LOG_ERROR("STA connect called with empty SSID");
    server.send(400, "text/plain", "SSID required");
    return;
  }
  LOG_INFO("STA connect request: ssid=%s, hidden=%d, bssid=%s", ssid.c_str(), hidden, bssid.c_str());
  if (connecting || WiFi.status() == WL_CONNECTED) {
    server.send(409, "text/plain", "STA already connected or connecting");
    return;
  }
  connectSTA(ssid, password, hidden, bssid, true);
  server.send(200, "text/plain", "OK");
}

void handleSTADisconnect() {
  LOG_INFO("STA disconnect request");
  disconnectSTA();
  server.send(200, "text/plain", "OK");
}

void handleSTAForget() {
  LOG_INFO("STA forget request");
  forgetSTA();
  server.send(200, "text/plain", "OK");
}

// ---------- Logs endpoint ----------
void handleLogs() {
  String json = "[";
  int start = (logHead - logCount + MAX_LOG_ENTRIES) % MAX_LOG_ENTRIES;
  for (int i = 0; i < logCount; i++) {
    int idx = (start + i) % MAX_LOG_ENTRIES;
    if (i) json += ",";
    json += "{";
    json += "\"timestamp\":" + String(logBuffer[idx].timestamp);
    json += ",\"level\":\"" + jsonEscape(String(logBuffer[idx].level)) + "\"";
    json += ",\"message\":\"" + jsonEscape(String(logBuffer[idx].message)) + "\"";
    json += "}";
  }
  json += "]";
  server.send(200, "application/json", json);
}

// ---------- Web pages ----------
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0, user-scalable=no">
<title>ESP32 HID Controller</title>
<style>
  * { box-sizing: border-box; margin: 0; padding: 0; }
  body { font-family: 'Segoe UI', Roboto, sans-serif; background: #0b0b0b; color: #eee; padding: 12px; min-height: 100vh; }
  .container { max-width: 1200px; margin: 0 auto; display: grid; grid-template-columns: 1fr; gap: 16px; }
  @media (min-width: 780px) { .container { grid-template-columns: 1fr 1fr; } .full-width { grid-column: 1 / -1; } }
  .card { background: #1e1e1e; border-radius: 16px; padding: 18px; border: 1px solid #333; box-shadow: 0 8px 20px rgba(0,0,0,0.5); }
  h2 { font-size: 1.6rem; color: #5b9aff; text-align: center; margin-bottom: 10px; font-weight: 300; letter-spacing: 1px; }
  h3 { font-size: 1.2rem; color: #aaa; text-align: center; margin-bottom: 14px; font-weight: 400; }
  .slider-group { display: flex; flex-wrap: wrap; align-items: center; justify-content: center; gap: 10px 20px; margin: 8px 0; }
  .slider-group label { font-size: 14px; color: #ccc; }
  input[type=range] { flex: 1; min-width: 120px; height: 4px; -webkit-appearance: none; appearance: none; background: #444; border-radius: 2px; outline: none; }
  input[type=range]::-webkit-slider-thumb { -webkit-appearance: none; appearance: none; width: 16px; height: 16px; border-radius: 50%; background: #5b9aff; cursor: pointer; }
  .slider-value { min-width: 40px; text-align: center; color: #5b9aff; font-weight: 600; }
  .btn-group { display: flex; flex-wrap: wrap; gap: 8px; justify-content: center; margin: 8px 0; }
  button { padding: 8px 16px; background: #2a2a2a; color: #eee; border: 1px solid #444; border-radius: 8px; cursor: pointer; font-size: 14px; transition: 0.15s; font-weight: 500; box-shadow: 0 2px 4px rgba(0,0,0,0.3); }
  button:hover { background: #3a3a3a; transform: translateY(-1px); }
  button:active { transform: translateY(0); background: #444; }
  button.accent { background: #2c5f8a; border-color: #3a7bbd; }
  button.accent:hover { background: #3a7bbd; }
  button.mod-active { background: #f39c12; color: #000; border-color: #f1c40f; }
  button.pressed { background: #f39c12; color: #000; border-color: #f1c40f; }
  #pad { width: 100%; height: 200px; background: #181818; border-radius: 12px; border: 2px solid #333; touch-action: none; cursor: crosshair; margin: 10px 0; transition: border 0.2s; }
  #pad:active { border-color: #5b9aff; }
  .arrow-row { display: flex; justify-content: center; gap: 6px; margin: 4px 0; }
  .arrow-row button { min-width: 48px; height: 44px; font-size: 18px; }

  .kb-grid {
    display: flex;
    flex-direction: column;
    gap: 5px;
    width: 100%;
    max-width: 900px;
    margin: 12px auto 0;
    overflow-x: auto;
  }
  .kb-row {
    display: flex;
    gap: 5px;
    width: max-content;
    min-width: 100%;
  }
  .kb-key {
    flex: 0 0 44px;
    height: 42px;
    background: #2a2a2a;
    border: 1px solid #444;
    border-radius: 6px;
    color: #eee;
    display: flex;
    align-items: center;
    justify-content: center;
    font-size: 13px;
    cursor: pointer;
    user-select: none;
    transition: 0.1s;
  }
  .kb-key:hover {
    background: #3a3a3a;
  }
  .kb-key:active {
    background: #444;
  }
  .kb-key.special {
    background: #2c3e50;
  }
  .kb-key.special:hover { background: #3e5a6f; }
  .kb-key.mod-down {
    background: #5b9aff;
    color: #000;
  }
  .kb-key.last-clicked {
    background: #5b9aff;
    color: #000;
    border-color: #7ab7ff;
  }

  .numpad {
    display: grid;
    grid-template-columns: repeat(4, 48px);
    grid-template-rows: repeat(5, 42px);
    gap: 5px;
    width: max-content;
    margin: 12px auto 0;
  }
  .numpad .kb-key {
    width: 48px;
    height: 42px;
    min-height: 42px;
    flex: none;
  }
  .numpad .tall {
    grid-row: span 2;
  }
  .numpad .zero {
    grid-column: span 2;
  }

  .row-label { font-size: 12px; color: #666; text-align: center; margin: 6px 0 2px; }
  .text-input-area { display: flex; gap: 10px; flex-wrap: wrap; justify-content: center; margin: 10px 0; }
  input[type=text] { background: #222; border: 1px solid #444; border-radius: 8px; padding: 8px 14px; color: #eee; font-size: 16px; flex: 1; min-width: 160px; max-width: 380px; outline: none; }
  input[type=text]:focus { border-color: #5b9aff; }
  .small { font-size: 12px; color: #777; text-align: center; margin-top: 6px; }

  .log-panel { background: #121212; border: 1px solid #333; border-radius: 8px; padding: 10px; max-height: 180px; overflow-y: auto; margin-top: 10px; display: none; font-family: monospace; font-size: 12px; }
  .log-panel.visible { display: block; }
  .log-entry { padding: 2px 0; border-bottom: 1px solid #1a1a1a; }
  .log-info { color: #aaa; }
  .log-warn { color: #f39c12; }
  .log-error { color: #e74c3c; }
  .log-success { color: #2ecc71; }

  .sta-status { background: #1a1a1a; border-radius: 8px; padding: 8px 16px; margin-bottom: 10px; display: flex; flex-wrap: wrap; align-items: center; justify-content: space-between; border-left: 4px solid #555; }
  .sta-status .label { font-weight: 500; color: #aaa; }
  .sta-status .status-text { color: #eee; }
  .sta-status .connected { color: #2ecc71; }
  .sta-status .disconnected { color: #e74c3c; }
  .sta-status .ip { color: #5b9aff; }

  /* Different real keyboard key sizes */
  .kb-key.w75  { flex-basis: 75px; }
  .kb-key.w85  { flex-basis: 85px; }
  .kb-key.w95  { flex-basis: 95px; }
  .kb-key.w110 { flex-basis: 110px; }
  .kb-key.w125 { flex-basis: 125px; }
  .kb-key.space { flex-basis: 220px; }

  @media (max-width: 600px) {
    .kb-grid {
      width: 100%;
      overflow-x: hidden;
    }

    .kb-key {
      flex-basis: 34px;
      height: 38px;
      font-size: 10px;
    }

    .kb-key.w75  { flex-basis: 48px; }
    .kb-key.w95  { flex-basis: 60px; }
    .kb-key.w110 { flex-basis: 70px; }
    .kb-key.w125 { flex-basis: 82px; }
    .kb-key.space { flex-basis: 150px; }
  }
</style>
</head>
<body>
<div class="container">
  <div class="card full-width">
    <div style="display:flex; justify-content:space-between; align-items:center;">
      <h2 style="margin:0;">⚡ ESP32 HID Controller</h2>
      <a href="/sta" style="color:#5b9aff; font-size:20px; text-decoration:none;">📶</a>
    </div>
    <div class="sta-status" id="staStatus">
      <span class="label">Wi‑Fi:</span>
      <span class="status-text" id="staStatusText">Loading...</span>
      <span style="margin-left:auto;">
        <button onclick="window.location.href='/sta'" style="background:#333; padding:4px 12px; font-size:12px;">Settings</button>
      </span>
    </div>

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
      <button onclick="window.location.href='/logs'" style="background:#333;">📄 Raw Logs</button>
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
      <button id="mouseLeftDown" class="mouse-down" data-btn="left">L⬇</button>
      <button id="mouseLeftUp"   class="mouse-up"   data-btn="left" onclick="mouseUp('left')">L⬆</button>
      <button id="mouseRightDown" class="mouse-down" data-btn="right">R⬇</button>
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
  fetch(url, {cache: 'no-store'}).then(res => {
    if (!res.ok) logWarn('HTTP ' + res.status + ' for ' + url);
    else logInfo('HTTP OK: ' + url);
    return res;
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
let sensSaveTimer = null;
let repeatSaveTimer = null;

document.getElementById('sens').addEventListener('input', function() {
  sens = parseFloat(this.value);
  document.getElementById('sensVal').textContent = sens.toFixed(1);
  clearTimeout(sensSaveTimer);
  sensSaveTimer = setTimeout(() => {
    sendHTTP('/set_sensitivity?value=' + encodeURIComponent(sens));
  }, 400);
  logInfo('Sensitivity = ' + sens);
});
document.getElementById('repeatRate').addEventListener('input', function() {
  repeatInterval = parseInt(this.value);
  document.getElementById('repeatVal').textContent = repeatInterval;
  clearTimeout(repeatSaveTimer);
  repeatSaveTimer = setTimeout(() => {
    sendHTTP('/set_repeat?value=' + encodeURIComponent(repeatInterval));
  }, 400);
  logInfo('Repeat interval = ' + repeatInterval);
});
document.getElementById('legacyCheck').addEventListener('change', function() {
  legacyMode = this.checked;
  sendHTTP('/set_legacy?value=' + (legacyMode ? 1 : 0));
  logInfo('Legacy mode = ' + legacyMode);
});

// ========== STA STATUS UPDATE ==========
function updateSTAStatus() {
  fetch('/sta/status')
    .then(res => res.json())
    .then(data => {
      const statusText = document.getElementById('staStatusText');
      if (data.connected) {
        statusText.innerHTML = `<span class="connected">Connected</span> to <strong>${data.ssid}</strong> (IP: <span class="ip">${data.ip}</span>)`;
      } else {
        let err = data.error || 'Not connected';
        statusText.innerHTML = `<span class="disconnected">Disconnected</span> – ${err}`;
      }
    })
    .catch(() => {});
}
setInterval(updateSTAStatus, 3000);
updateSTAStatus();

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
pad.addEventListener('lostpointercapture', () => { padDown = false; });

// ========== ARROW REPEAT ==========
let repeatTimer = null;
function stopArrowRepeat() {
  clearInterval(repeatTimer);
  repeatTimer = null;
}
document.querySelectorAll('.arrow-row button[data-dx]').forEach(btn => {
  const dx = parseInt(btn.dataset.dx), dy = parseInt(btn.dataset.dy);
  btn.addEventListener('pointerdown', (e) => {
    e.preventDefault();
    clearInterval(repeatTimer);
    sendMove(dx, dy);
    repeatTimer = setInterval(() => sendMove(dx, dy), repeatInterval);
  });
  btn.addEventListener('pointerup', stopArrowRepeat);
  btn.addEventListener('pointercancel', stopArrowRepeat);
  btn.addEventListener('pointerleave', stopArrowRepeat);
});

// ========== MOUSE HOLD STATE ==========
const mouseState = { left: false, right: false };

function updateMouseUI() {
  document.getElementById('mouseLeftDown').classList.toggle('pressed', mouseState.left);
  document.getElementById('mouseRightDown').classList.toggle('pressed', mouseState.right);
}

function mouseDown(btn) {
  if (btn === 'left' && !mouseState.left) { mouseState.left = true; sendHTTP('/down?btn=left'); }
  else if (btn === 'right' && !mouseState.right) { mouseState.right = true; sendHTTP('/down?btn=right'); }
  updateMouseUI();
}

function mouseUp(btn) {
  if (btn === 'left' && mouseState.left) { mouseState.left = false; sendHTTP('/up?btn=left'); }
  else if (btn === 'right' && mouseState.right) { mouseState.right = false; sendHTTP('/up?btn=right'); }
  updateMouseUI();
}

function releaseMouseButtons() {
  if (mouseState.left) sendHTTP('/up?btn=left');
  if (mouseState.right) sendHTTP('/up?btn=right');
  mouseState.left = false;
  mouseState.right = false;
  updateMouseUI();
}

document.getElementById('mouseLeftDown').addEventListener('pointerdown', e => {
  e.preventDefault();
  e.currentTarget.setPointerCapture?.(e.pointerId);
  mouseDown('left');
});
document.getElementById('mouseLeftDown').addEventListener('pointerup', e => { e.preventDefault(); mouseUp('left'); });
document.getElementById('mouseLeftDown').addEventListener('pointercancel', () => mouseUp('left'));
document.getElementById('mouseRightDown').addEventListener('pointerdown', e => {
  e.preventDefault();
  e.currentTarget.setPointerCapture?.(e.pointerId);
  mouseDown('right');
});
document.getElementById('mouseRightDown').addEventListener('pointerup', e => { e.preventDefault(); mouseUp('right'); });
document.getElementById('mouseRightDown').addEventListener('pointercancel', () => mouseUp('right'));

// ========== MODIFIERS ==========
const modState = { CTRL: false, ALT: false, SHIFT: false, WIN: false };
const heldKeyCounts = new Map();

document.querySelectorAll('#modButtons button').forEach(btn => {
  btn.addEventListener('click', () => {
    const mod = btn.dataset.mod;
    modState[mod] = !modState[mod];
    btn.classList.toggle('mod-active', modState[mod]);
    sendHTTP('/toggle_modifier?mod=' + encodeURIComponent(mod));
    logInfo('Sticky ' + mod + ' = ' + modState[mod]);
  });
});

function holdKey(code) {
  const count = heldKeyCounts.get(code) || 0;
  heldKeyCounts.set(code, count + 1);
}

function releaseKey(code) {
  const count = heldKeyCounts.get(code) || 0;
  if (count <= 1) {
    heldKeyCounts.delete(code);
    sendHTTP('/key_up?key=' + encodeURIComponent(code));
  } else {
    heldKeyCounts.set(code, count - 1);
  }
}

function releaseHeldKeys() {
  heldKeyCounts.forEach((count, code) => {
    if (count > 0) sendHTTP('/key_up?key=' + encodeURIComponent(code));
  });
  heldKeyCounts.clear();
  document.querySelectorAll('.mod-down').forEach(el => el.classList.remove('mod-down'));
  sendHTTP('/reset_modifiers');
  Object.keys(modState).forEach(k => modState[k] = false);
  document.querySelectorAll('#modButtons button').forEach(btn => btn.classList.remove('mod-active'));
  logInfo('All held keys/modifiers released');
}

window.addEventListener('blur', () => {
  stopArrowRepeat();
  releaseMouseButtons();
  releaseHeldKeys();
});
document.addEventListener('visibilitychange', () => {
  if (document.hidden) {
    stopArrowRepeat();
    releaseMouseButtons();
    releaseHeldKeys();
  }
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

  const keyboardRows = [
    [
      ['Esc','w75'],
      ['F1',''], ['F2',''], ['F3',''], ['F4',''],
      ['F5',''], ['F6',''], ['F7',''], ['F8',''],
      ['F9',''], ['F10',''], ['F11',''], ['F12',''],
      ['PrtSc','w75'], ['ScrLk','w75'], ['Pause','w75']
    ],

    [
      ['`',''], ['1',''], ['2',''], ['3',''], ['4',''],
      ['5',''], ['6',''], ['7',''], ['8',''], ['9',''],
      ['0',''], ['-',''], ['=',''], ['Backspace','w110']
    ],

    [
      ['Tab','w75'],
      ['q',''], ['w',''], ['e',''], ['r',''], ['t',''],
      ['y',''], ['u',''], ['i',''], ['o',''], ['p',''],
      ['[',''], [']',''], ['\\','w75']
    ],

    [
      ['CapsLock','w95'],
      ['a',''], ['s',''], ['d',''], ['f',''], ['g',''],
      ['h',''], ['j',''], ['k',''], ['l',''], [';',''],
      ["'",''],
      ['Enter','w95']
    ],

    [
      ['Shift','w125'],
      ['z',''], ['x',''], ['c',''], ['v',''], ['b',''],
      ['n',''], ['m',''], [',',''], ['.',''], ['/',''],
      ['Shift','w125']
    ],

    [
      ['Ctrl','w75'],
      ['Win','w75'],
      ['Alt','w75'],
      ['Space','space'],
      ['Alt','w75'],
      ['Win','w75'],
      ['Menu','w75'],
      ['Ctrl','w75']
    ]
  ];

  keyboardRows.forEach(row => {
    const rowEl = document.createElement('div');
    rowEl.className = 'kb-row';

    row.forEach(([label, size]) => {
      const el = document.createElement('div');
      el.className = 'kb-key';

      if (size) {
        el.classList.add(size);
      }

      el.textContent = label;

      if ([
        'Esc','F1','F2','F3','F4','F5','F6','F7','F8',
        'F9','F10','F11','F12','PrtSc','ScrLk','Pause'
      ].includes(label)) {
        el.classList.add('special');
      }

      const isMod = ['Shift','Ctrl','Alt','Win'].includes(label);

      if (isMod) {
        el.addEventListener('pointerdown', e => {
          e.preventDefault();

          const code = keyMap[label];

          if (code) {
            sendHTTP('/key_down?key=' + encodeURIComponent(code));
            holdKey(code);
            el.classList.add('mod-down');
          }
        });

        el.addEventListener('pointerup', e => {
          e.preventDefault();

          const code = keyMap[label];

          if (code) {
            sendHTTP('/key_up?key=' + encodeURIComponent(code));
            releaseKey(code);
            el.classList.remove('mod-down');
          }
        });

        el.addEventListener('pointerleave', () => {
          if (el.classList.contains('mod-down')) {
            const code = keyMap[label];

            if (code) {
              sendHTTP('/key_up?key=' + encodeURIComponent(code));
              releaseKey(code);
            }

            el.classList.remove('mod-down');
          }
        });

      } else {

        el.addEventListener('click', () => {
          let code = keyMap[label];

          if (code) {
            sendHTTP('/key?key=' + code);
          }
          else if (label.length === 1) {
            sendHTTP('/type?text=' + encodeURIComponent(label));
          }
          else {
            sendHTTP('/key?key=' + label);
          }

          highlightKey(el);
        });
      }

      rowEl.appendChild(el);
    });

    grid.appendChild(rowEl);
  });
}

// ========== NUMPAD ==========
const numpadLayout = [
  ['NumLock', 'Num /', 'Num *', 'Num -'],
  ['7',       '8',     '9',     'Num +'],
  ['4',       '5',     '6',     'Num +'],
  ['1',       '2',     '3',     'Num Enter'],
  ['0',       '0',     '.',     'Num Enter']
];

function buildNumpad() {
  const container = document.getElementById('numpad');
  container.innerHTML = '';

  const keys = [
    ['NumLock', 1, 1],
    ['Num /',   1, 1],
    ['Num *',   1, 1],
    ['Num -',   1, 1],

    ['7', 1, 1],
    ['8', 1, 1],
    ['9', 1, 1],

    ['4', 1, 1],
    ['5', 1, 1],
    ['6', 1, 1],

    ['1', 1, 1],
    ['2', 1, 1],
    ['3', 1, 1],

    ['0', 2, 1],
    ['.', 1, 1]
  ];

  keys.forEach(([label, colSpan, rowSpan]) => {
    const el = document.createElement('div');
    el.className = 'kb-key';

    if (colSpan === 2) {
      el.style.gridColumn = 'span 2';
    }

    el.textContent = label;

    const isNumLock = label === 'NumLock';

    if (isNumLock) {
      el.addEventListener('pointerdown', e => {
        e.preventDefault();

        const code = keyMap[label];

        if (code) {
          sendHTTP('/key_down?key=' + encodeURIComponent(code));
          holdKey(code);
          el.classList.add('mod-down');
        }
      });

      el.addEventListener('pointerup', e => {
        e.preventDefault();

        const code = keyMap[label];

        if (code) {
          sendHTTP('/key_up?key=' + encodeURIComponent(code));
          releaseKey(code);
          el.classList.remove('mod-down');
        }
      });

      el.addEventListener('pointerleave', () => {
        if (el.classList.contains('mod-down')) {
          const code = keyMap[label];

          if (code) {
            sendHTTP('/key_up?key=' + encodeURIComponent(code));
            releaseKey(code);
          }

          el.classList.remove('mod-down');
        }
      });

    } else {
      el.addEventListener('click', () => {
        const code = keyMap[label];

        if (code) {
          sendHTTP('/key?key=' + code);
        } else if (label.length === 1) {
          sendHTTP('/type?text=' + encodeURIComponent(label));
        } else {
          sendHTTP('/key?key=' + label);
        }

        highlightKey(el);
      });
    }

    container.appendChild(el);
  });

  const plus = document.createElement('div');
  plus.className = 'kb-key';
  plus.textContent = '+';
  plus.style.gridColumn = '4';
  plus.style.gridRow = '2 / span 2';

  plus.addEventListener('click', () => {
    sendHTTP('/key?key=KP_PLUS');
    highlightKey(plus);
  });

  container.appendChild(plus);

  const enter = document.createElement('div');
  enter.className = 'kb-key';
  enter.textContent = 'Enter';
  enter.style.gridColumn = '4';
  enter.style.gridRow = '4 / span 2';

  enter.addEventListener('click', () => {
    sendHTTP('/key?key=KP_ENTER');
    highlightKey(enter);
  });

  container.appendChild(enter);
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
  releaseHeldKeys();
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

// STA configuration page (separate) – now auto-scans on load
// ---- Updated STA page HTML (sta_html) ----
const char sta_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>WiFi Settings</title>
<style>
  body { font-family: 'Segoe UI', Roboto, sans-serif; background: #0b0b0b; color: #eee; padding: 20px; }
  .container { max-width: 500px; margin: 0 auto; background: #1e1e1e; border-radius: 16px; padding: 24px; border: 1px solid #333; }
  h2 { color: #5b9aff; text-align: center; }
  label { display: block; margin: 12px 0 4px; color: #aaa; }
  input[type=text], input[type=password] { width: 100%; padding: 8px; background: #222; border: 1px solid #444; border-radius: 6px; color: #eee; }
  input[type=checkbox] { margin-right: 8px; }
  button { padding: 10px 20px; background: #5b9aff; border: none; border-radius: 8px; color: #fff; font-weight: bold; cursor: pointer; margin-top: 12px; }
  button:hover { background: #3a7bbd; }
  button.secondary { background: #444; }
  button.secondary:hover { background: #555; }
  .status-box { background: #111; padding: 10px; border-radius: 8px; margin: 12px 0; }
  .connected { color: #2ecc71; }
  .disconnected { color: #e74c3c; }
  .info { color: #aaa; font-size: 14px; }
  .network-list { max-height: 200px; overflow-y: auto; background: #111; border-radius: 6px; padding: 4px; margin-top: 6px; }
  .network-item { padding: 6px 8px; cursor: pointer; border-bottom: 1px solid #222; display: flex; justify-content: space-between; align-items: center; }
  .network-item:hover { background: #2a2a2a; }
  .network-item .bssid { color: #888; font-size: 12px; }
  .network-item .rssi { color: #666; font-size: 12px; }
  .network-item .enc { color: #5b9aff; font-size: 12px; }
  .hidden-note { background: #2a2a2a; padding: 8px; border-radius: 6px; margin: 8px 0; font-size: 14px; border-left: 3px solid #f39c12; }
  .scanning-msg { text-align: center; color: #aaa; padding: 10px; }
  .error-msg { color: #e74c3c; }
</style>
</head>
<body>
<div class="container">
  <h2>🔧 WiFi Settings</h2>
  <div id="statusBox" class="status-box">Loading...</div>

  <div class="hidden-note">
    ⚠️ <strong>Hidden networks</strong> do not appear in scans. If your network is hidden,
    manually enter its SSID and BSSID (MAC) below, then tick the “Hidden network” checkbox.
  </div>

  <label>SSID</label>
  <input type="text" id="ssid" placeholder="Network name">

  <!-- No Scan button – scanning happens automatically on page load -->
  <div id="networkList" class="network-list">
    <div class="scanning-msg">Scanning for networks...</div>
  </div>

  <label>Password</label>
  <input type="password" id="pass" placeholder="Password">

  <label>BSSID (MAC) <span class="info">(optional, useful for hidden networks)</span></label>
  <input type="text" id="bssid" placeholder="xx:xx:xx:xx:xx:xx">

  <label>
    <input type="checkbox" id="hiddenCheck"> Hidden network
  </label>

  <button onclick="connect()">Connect</button>
  <button onclick="disconnect()" class="secondary">Disconnect</button>
  <button onclick="forget()" class="secondary" style="background:#722;">Forget</button>
  <br>
  <button onclick="window.location.href='/'" style="background:#333;">← Back to HID</button>
</div>

<script>
function updateStatus() {
  fetch('/sta/status')
    .then(r => r.json())
    .then(data => {
      const box = document.getElementById('statusBox');
      if (data.connected) {
        box.innerHTML = `<span class="connected">Connected</span> to <strong>${data.ssid}</strong><br>IP: ${data.ip}`;
      } else {
        box.innerHTML = `<span class="disconnected">Disconnected</span> – ${data.error || 'Idle'}`;
      }
    })
    .catch(() => {});
}
setInterval(updateStatus, 2000);
updateStatus();

let scanAttempts = 0;
const MAX_SCAN_ATTEMPTS = 10;

function scanNetworks() {
  const listDiv = document.getElementById('networkList');
  listDiv.innerHTML = '<div class="scanning-msg">Scanning...</div>';

  fetch('/sta/scan', {cache: 'no-store'})
    .then(r => r.json())
    .then(data => {
      if (data && data.scanning) {
        listDiv.innerHTML = '<div class="scanning-msg">Scanning, please wait...</div>';
        if (++scanAttempts < MAX_SCAN_ATTEMPTS) {
          setTimeout(scanNetworks, 2000);
        } else {
          listDiv.innerHTML = '<div class="scanning-msg error-msg">Scan timed out. <a href="#" id="retryScan">Retry</a></div>';
          document.getElementById('retryScan')?.addEventListener('click', e => { e.preventDefault(); scanAttempts = 0; scanNetworks(); });
        }
        return;
      }

      if (!Array.isArray(data)) {
        throw new Error((data && data.error) || 'Invalid scan response');
      }

      if (data.length === 0) {
        listDiv.innerHTML = '<div class="scanning-msg">No networks found. Make sure you are in range.</div>';
        scanAttempts = 0;
        return;
      }

      listDiv.innerHTML = '';
      data.forEach(net => {
        const item = document.createElement('div');
        item.className = 'network-item';

        const left = document.createElement('span');
        const name = document.createElement('strong');
        name.textContent = net.ssid || '(hidden)';
        left.appendChild(name);

        const bssid = document.createElement('span');
        bssid.className = 'bssid';
        bssid.textContent = ' (' + (net.bssid || '') + ')';
        left.appendChild(bssid);

        const right = document.createElement('span');
        const enc = document.createElement('span');
        enc.className = 'enc';
        enc.textContent = net.encryption_str || 'Unknown';
        right.appendChild(enc);

        const rssi = document.createElement('span');
        rssi.className = 'rssi';
        rssi.textContent = ' RSSI: ' + net.rssi;
        right.appendChild(rssi);

        item.appendChild(left);
        item.appendChild(right);
        item.addEventListener('click', () => selectNetwork(net.ssid || '', net.bssid || ''));
        listDiv.appendChild(item);
      });
      scanAttempts = 0;
    })
    .catch(err => {
      listDiv.innerHTML = '<div class="scanning-msg error-msg">Error scanning. <a href="#" id="retryScan">Retry</a></div>';
      document.getElementById('retryScan')?.addEventListener('click', e => { e.preventDefault(); scanAttempts = 0; scanNetworks(); });
      console.error(err);
    });
}

function selectNetwork(ssid, bssid) {
  document.getElementById('ssid').value = ssid;
  document.getElementById('bssid').value = bssid;
}

function connect() {
  const ssid = document.getElementById('ssid').value.trim();
  const pass = document.getElementById('pass').value;
  const bssid = document.getElementById('bssid').value.trim();
  const hidden = document.getElementById('hiddenCheck').checked;

  if (!ssid) { alert('SSID required'); return; }
  const params = new URLSearchParams();
  params.append('ssid', ssid);
  params.append('pass', pass);
  params.append('hidden', hidden ? '1' : '0');
  if (bssid) params.append('bssid', bssid);

  fetch('/sta/connect?' + params.toString())
    .then(res => {
      if (res.ok) {
        alert('Connecting... check status.');
        setTimeout(updateStatus, 1000);
      } else {
        alert('Failed to send connect request.');
      }
    })
    .catch(() => alert('Network error.'));
}

function disconnect() {
  fetch('/sta/disconnect').then(() => { updateStatus(); });
}

function forget() {
  if (confirm('Forget saved WiFi credentials?')) {
    fetch('/sta/forget').then(() => { updateStatus(); });
  }
}

// Auto-scan when page loads, with a small delay to let the ESP settle
window.onload = function() {
  setTimeout(scanNetworks, 500);
};
</script>
</body>
</html>
)rawliteral";

// ---------- Web server handlers ----------
void handleRoot() {
  server.send(200, "text/html", index_html);
}

void handleSTA() {
  server.send(200, "text/html", sta_html);
}

// ---------- Setup ----------
void setup() {
  Serial.begin(115200);
  LOG_INFO("ESP32 HID Controller starting...");

  // USB HID – correct order: USB first, then HID devices
  USB.begin();
  delay(100);               // Let the host detect the device
  Mouse.begin();
  Keyboard.begin();
  LOG_INFO("USB HID initialized");

  // Load persistent settings
  loadSettings();

  // AP mode
  WiFi.mode(WIFI_AP);
  WiFi.softAP(ap_ssid, ap_password);
  IPAddress apIP = WiFi.softAPIP();
  LOG_INFO("AP mode started, IP: %s", apIP.toString().c_str());

  // DNS
  dnsServer.start(53, "*", apIP);
  LOG_INFO("DNS server started");

  // STA: load saved config and connect
  WiFi.onEvent(WiFiEvent);
  loadSTAConfig();

  // Web server
  server.on("/", handleRoot);
  server.on("/sta", handleSTA);
  server.on("/sta/status", handleSTAStatus);
  server.on("/sta/scan", handleSTAScan);
  server.on("/sta/connect", handleSTAConnect);
  server.on("/sta/disconnect", handleSTADisconnect);
  server.on("/sta/forget", handleSTAForget);
  server.on("/logs", handleLogs);

  // HID endpoints
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
  server.onNotFound([]() {
    server.send(200, "text/html", index_html);
  });

  server.begin();
  webSocket.begin();
  webSocket.onEvent(webSocketEvent);

  LOG_INFO("HTTP server and WebSocket started");
  LOG_INFO("Setup complete.");
}

void loop() {
  dnsServer.processNextRequest();
  server.handleClient();
  webSocket.loop();

  // STA timeout + scheduled retry logic.
  if (connecting && WiFi.status() != WL_CONNECTED) {
    if (millis() - connectStartTime > CONNECT_TIMEOUT) {
      connecting = false;

      if (sta_retry_count < MAX_RETRIES) {
        sta_retry_count++;
        retryPending = true;
        lastRetryTime = millis();
        sta_status = "Retrying...";
        sta_error = "Retry " + String(sta_retry_count) + "/" + String(MAX_RETRIES) + " in " + String(RETRY_INTERVAL / 1000) + "s";
        LOG_WARN("STA connection timeout; retry %d/%d scheduled in %lu ms",
                 sta_retry_count, MAX_RETRIES, RETRY_INTERVAL);
      } else {
        retryPending = false;
        sta_status = "Disconnected";
        sta_error = "Max retries exceeded";
        LOG_ERROR("STA max retries exceeded");
      }
    }
  }

  if (retryPending && !connecting && WiFi.status() != WL_CONNECTED) {
    if (millis() - lastRetryTime >= RETRY_INTERVAL) {
      preferences.begin("wifi", true);
      String ssid = preferences.getString("ssid", "");
      String pass = preferences.getString("pass", "");
      bool hidden = preferences.getBool("hidden", false);
      String bssid = preferences.getString("bssid", "");
      preferences.end();

      if (ssid.length() > 0) {
        retryPending = false;
        connectSTA(ssid, pass, hidden, bssid, false);
      } else {
        retryPending = false;
        sta_status = "Disconnected";
        sta_error = "No saved credentials";
        LOG_ERROR("No saved credentials for retry");
      }
    }
  }

  delay(1);
}
