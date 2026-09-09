// v6
// Default SSID: ESP32-MOUSE
// Default Password: 12345678
// Default Setting:
// - USB Mode: USB-OTG (TinyUSB)
// - Upload Mode: UART0 / Hardware CDC
// - USB CDC On Boot: Disabled
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
#include <cstring>
#include <HTTPClient.h>
#include <Update.h>
#include <WiFiClientSecure.h>
#include <mbedtls/sha256.h>

USBHIDMouse Mouse;
USBHIDKeyboard Keyboard;
WebServer server(80);
DNSServer dnsServer;
WebSocketsServer webSocket(81);
Preferences preferences;

const char* ap_ssid = "ESP32-Mouse";
const char* ap_password = "12345678";

float sensitivity = 2.0f;
int repeatInterval = 100;
bool legacyMode = false;

bool ctrlSticky = false;
bool altSticky = false;
bool shiftSticky = false;
bool winSticky = false;
uint8_t ctrlHeld = 0;
uint8_t altHeld = 0;
uint8_t shiftHeld = 0;
uint8_t winHeld = 0;

String sta_ssid = "";
String sta_ip = "";
String sta_status = "Disconnected";
String sta_error = "";
bool scanInProgress = false;
int sta_retry_count = 0;
const int MAX_RETRIES = 3;
const unsigned long CONNECT_TIMEOUT = 10000UL;
const unsigned long RETRY_INTERVAL = 5000UL;
unsigned long connectStartTime = 0;
unsigned long lastRetryTime = 0;
bool connecting = false;
bool retryPending = false;
bool staStarted = false;

// Firmware update globals
#define FW_VERSION_MAJOR 5
#define FW_VERSION_MINOR 0
#define FW_VERSION_PATCH 0
#define FW_VERSION_STR "5.0.0"
String updateVersionUrl = "";
String updateBinUrl = "";
bool updateInProgress = false;

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
  Serial.printf("[%lu] [%s] %s\n", millis(), level, msg);
}
#define LOG_INFO(...) addLog("INFO", __VA_ARGS__)
#define LOG_WARN(...) addLog("WARN", __VA_ARGS__)
#define LOG_ERROR(...) addLog("ERROR", __VA_ARGS__)
#define LOG_SUCCESS(...) addLog("SUCCESS", __VA_ARGS__)

int16_t clamp(int16_t v, int16_t minv, int16_t maxv) {
  if (v < minv) return minv;
  if (v > maxv) return maxv;
  return v;
}

String jsonEscape(const String& input) {
  String out; out.reserve(input.length() + 8);
  for (size_t i = 0; i < input.length(); ++i) {
    char c = input.charAt(i);
    switch (c) {
      case '\\': out += "\\\\"; break;
      case '"': out += "\\\""; break;
      case '\n': out += "\\n"; break;
      case '\r': out += "\\r"; break;
      case '\t': out += "\\t"; break;
      case '\b': out += "\\b"; break;
      case '\f': out += "\\f"; break;
      default:
        if ((uint8_t)c < 0x20) { char buf[7]; snprintf(buf, sizeof(buf), "\\u%04x", (unsigned char)c); out += buf; }
        else out += c;
        break;
    }
  }
  return out;
}

bool effectiveCtrl() { return ctrlSticky || ctrlHeld > 0; }
bool effectiveAlt() { return altSticky || altHeld > 0; }
bool effectiveShift() { return shiftSticky || shiftHeld > 0; }
bool effectiveWin() { return winSticky || winHeld > 0; }

void applyModifiers() {
  if (effectiveCtrl()) Keyboard.press(KEY_LEFT_CTRL); else Keyboard.release(KEY_LEFT_CTRL);
  if (effectiveAlt()) Keyboard.press(KEY_LEFT_ALT); else Keyboard.release(KEY_LEFT_ALT);
  if (effectiveShift()) Keyboard.press(KEY_LEFT_SHIFT); else Keyboard.release(KEY_LEFT_SHIFT);
  if (effectiveWin()) Keyboard.press(KEY_LEFT_GUI); else Keyboard.release(KEY_LEFT_GUI);
}

void releaseAllModifiers() {
  ctrlSticky = false; altSticky = false; shiftSticky = false; winSticky = false;
  ctrlHeld = 0; altHeld = 0; shiftHeld = 0; winHeld = 0;
  Keyboard.release(KEY_LEFT_CTRL); Keyboard.release(KEY_LEFT_ALT); Keyboard.release(KEY_LEFT_SHIFT); Keyboard.release(KEY_LEFT_GUI);
  LOG_INFO("All modifiers released");
}

bool isModifierCode(uint8_t code) { return code == KEY_LEFT_CTRL || code == KEY_LEFT_ALT || code == KEY_LEFT_SHIFT || code == KEY_LEFT_GUI; }

void modifierHeldDown(uint8_t code) {
  if (code == KEY_LEFT_CTRL) { if (ctrlHeld < 255) ctrlHeld++; }
  else if (code == KEY_LEFT_ALT) { if (altHeld < 255) altHeld++; }
  else if (code == KEY_LEFT_SHIFT) { if (shiftHeld < 255) shiftHeld++; }
  else if (code == KEY_LEFT_GUI) { if (winHeld < 255) winHeld++; }
  applyModifiers();
}

void modifierHeldUp(uint8_t code) {
  if (code == KEY_LEFT_CTRL) { if (ctrlHeld > 0) ctrlHeld--; }
  else if (code == KEY_LEFT_ALT) { if (altHeld > 0) altHeld--; }
  else if (code == KEY_LEFT_SHIFT) { if (shiftHeld > 0) shiftHeld--; }
  else if (code == KEY_LEFT_GUI) { if (winHeld > 0) winHeld--; }
  applyModifiers();
}

bool toggleModifier(const String& mod) {
  if (mod == "CTRL") ctrlSticky = !ctrlSticky;
  else if (mod == "ALT") altSticky = !altSticky;
  else if (mod == "SHIFT") shiftSticky = !shiftSticky;
  else if (mod == "WIN") winSticky = !winSticky;
  else { LOG_WARN("Invalid modifier: %s", mod.c_str()); return false; }
  applyModifiers();
  bool state = false;
  if (mod == "CTRL") state = ctrlSticky;
  else if (mod == "ALT") state = altSticky;
  else if (mod == "SHIFT") state = shiftSticky;
  else if (mod == "WIN") state = winSticky;
  LOG_INFO("Sticky modifier %s -> %d", mod.c_str(), state);
  return true;
}

void loadSettings() {
  preferences.begin("settings", true);
  sensitivity = preferences.getFloat("sens", 2.0f);
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

// ------------------- Update URL storage -------------------
void loadUpdateUrls() {
  preferences.begin("updates", true);
  updateVersionUrl = preferences.getString("verUrl", "https://example.com/version.txt");
  updateBinUrl = preferences.getString("binUrl", "https://example.com/firmware.bin");
  preferences.end();
}

void saveUpdateUrls(const String& verUrl, const String& binUrl) {
  preferences.begin("updates", false);
  preferences.putString("verUrl", verUrl);
  preferences.putString("binUrl", binUrl);
  preferences.end();
  updateVersionUrl = verUrl;
  updateBinUrl = binUrl;
  LOG_INFO("Update URLs saved");
}

// ------------------- Firmware Update Functions -------------------
bool fetchVersionInfo(const String& url, String& version, String& hash) {
  HTTPClient http;
  http.begin(url);
  http.setTimeout(5000);
  int code = http.GET();
  if (code != HTTP_CODE_OK) { http.end(); return false; }
  String payload = http.getString();
  http.end();
  int nl = payload.indexOf('\n');
  if (nl == -1) { version = payload; hash = ""; }
  else { version = payload.substring(0, nl); hash = payload.substring(nl + 1); }
  version.trim(); hash.trim();
  return true;
}

bool downloadAndVerify(const String& url, const String& expectedHash, int maxRetries = 3) {
  for (int attempt = 1; attempt <= maxRetries; attempt++) {
    LOG_INFO("Download attempt %d/%d", attempt, maxRetries);
    WiFiClientSecure client;
    client.setInsecure(); // For testing; in production use proper certificate validation
    HTTPClient http;
    http.begin(client, url);
    http.setTimeout(30000);
    int code = http.GET();
    if (code != HTTP_CODE_OK) {
      LOG_WARN("HTTP GET failed, code=%d", code);
      http.end();
      delay(1000 * attempt);
      continue;
    }
    int len = http.getSize();
    if (len <= 0) {
      LOG_WARN("Invalid content length: %d", len);
      http.end();
      continue;
    }
    if (!Update.begin(len)) {
      LOG_ERROR("Update.begin() failed: %s", Update.errorString());
      http.end();
      return false;
    }
    WiFiClient* stream = http.getStreamPtr();
    size_t written = 0;
    uint8_t buf[1024];
    mbedtls_sha256_context sha256_ctx;
    mbedtls_sha256_init(&sha256_ctx);
    mbedtls_sha256_starts(&sha256_ctx, 0);

    bool success = true;
    while (http.connected() && written < len) {
      size_t avail = stream->available();
      if (avail) {
        size_t toRead = min(avail, (size_t)1024);
        size_t bytes = stream->readBytes(buf, toRead);
        if (bytes == 0) { delay(10); continue; }
        if (Update.write(buf, bytes) != bytes) {
          LOG_ERROR("Update.write failed");
          success = false;
          break;
        }
        mbedtls_sha256_update(&sha256_ctx, buf, bytes);
        written += bytes;
      }
      yield();
    }
    http.end();

    if (!success || written != len) {
      Update.abort();
      LOG_WARN("Download incomplete, attempt %d", attempt);
      delay(1000 * attempt);
      continue;
    }

    uint8_t hash[32];
    mbedtls_sha256_finish(&sha256_ctx, hash);
    mbedtls_sha256_free(&sha256_ctx);
    char hex[65];
    for (int i = 0; i < 32; i++) sprintf(hex + i*2, "%02x", hash[i]);
    String computedHash = String(hex);

    if (!expectedHash.isEmpty() && computedHash != expectedHash) {
      LOG_ERROR("Hash mismatch! Expected: %s, got: %s", expectedHash.c_str(), computedHash.c_str());
      Update.abort();
      return false;
    }

    if (!Update.end()) {
      LOG_ERROR("Update.end() failed: %s", Update.errorString());
      return false;
    }

    LOG_SUCCESS("Firmware downloaded and verified.");
    return true;
  }
  return false;
}

void checkAndUpdate() {
  if (updateInProgress || WiFi.status() != WL_CONNECTED) return;
  loadUpdateUrls();
  String remoteVersion, remoteHash;
  if (!fetchVersionInfo(updateVersionUrl, remoteVersion, remoteHash)) {
    LOG_WARN("Could not fetch version info");
    return;
  }
  if (remoteVersion == FW_VERSION_STR) {
    LOG_INFO("Firmware up to date (%s)", FW_VERSION_STR);
    return;
  }
  LOG_INFO("New version %s available, updating...", remoteVersion.c_str());
  updateInProgress = true;
  if (downloadAndVerify(updateBinUrl, remoteHash, 3)) {
    LOG_SUCCESS("Update successful, rebooting...");
    delay(1000);
    ESP.restart();
  } else {
    LOG_ERROR("Update failed after retries.");
    updateInProgress = false;
  }
}

// ------------------- Web Handlers (new) -------------------
void handleSetUpdateUrls() {
  String ver = server.arg("ver");
  String bin = server.arg("bin");
  if (ver.isEmpty() || bin.isEmpty()) {
    server.send(400, "text/plain", "Missing parameters");
    return;
  }
  saveUpdateUrls(ver, bin);
  server.send(200, "text/plain", "OK");
}

void handleCheckUpdate() {
  server.send(200, "text/plain", "Check started");
  checkAndUpdate();
}

void handleUpload() {
  static size_t total = 0;
  HTTPUpload& upload = server.upload();
  if (upload.status == UPLOAD_FILE_START) {
    total = 0;
    if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
      LOG_ERROR("Update.begin failed");
      server.send(500, "text/plain", "Update begin failed");
    }
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
      LOG_ERROR("Update.write failed");
    }
    total += upload.currentSize;
  } else if (upload.status == UPLOAD_FILE_END) {
    if (Update.end()) {
      LOG_SUCCESS("Uploaded %u bytes, rebooting...", total);
      server.send(200, "text/plain", "Update success, rebooting...");
      delay(1000);
      ESP.restart();
    } else {
      LOG_ERROR("Update.end failed: %s", Update.errorString());
      server.send(500, "text/plain", "Update failed");
    }
  }
}

// ------------------- Existing Handlers (unchanged) -------------------
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
  if (btn == "right") { Mouse.click(MOUSE_RIGHT); LOG_INFO("Mouse click right"); }
  else if (btn == "middle") { Mouse.click(MOUSE_MIDDLE); LOG_INFO("Mouse click middle"); }
  else { Mouse.click(MOUSE_LEFT); LOG_INFO("Mouse click left"); }
  server.send(200, "text/plain", "OK");
}

void handleDoubleClick() {
  String btn = server.arg("btn");
  if (btn == "right") { Mouse.click(MOUSE_RIGHT); delay(50); Mouse.click(MOUSE_RIGHT); LOG_INFO("Mouse double click right"); }
  else { Mouse.click(MOUSE_LEFT); delay(50); Mouse.click(MOUSE_LEFT); LOG_INFO("Mouse double click left"); }
  server.send(200, "text/plain", "OK");
}

void handleDown() {
  String btn = server.arg("btn");
  if (btn == "right") { Mouse.press(MOUSE_RIGHT); LOG_INFO("Mouse press right"); }
  else if (btn == "middle") { Mouse.press(MOUSE_MIDDLE); LOG_INFO("Mouse press middle"); }
  else { Mouse.press(MOUSE_LEFT); LOG_INFO("Mouse press left"); }
  server.send(200, "text/plain", "OK");
}

void handleUp() {
  String btn = server.arg("btn");
  if (btn == "right") { Mouse.release(MOUSE_RIGHT); LOG_INFO("Mouse release right"); }
  else if (btn == "middle") { Mouse.release(MOUSE_MIDDLE); LOG_INFO("Mouse release middle"); }
  else { Mouse.release(MOUSE_LEFT); LOG_INFO("Mouse release left"); }
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
  if (val < 0.1f) val = 0.1f;
  if (val > 10.0f) val = 10.0f;
  sensitivity = val; saveSettings();
  LOG_INFO("Sensitivity set to %.1f", sensitivity);
  server.send(200, "text/plain", "OK");
}

void handleSetRepeatInterval() {
  int val = server.arg("value").toInt();
  if (val < 20) val = 20;
  if (val > 1000) val = 1000;
  repeatInterval = val; saveSettings();
  LOG_INFO("Repeat interval set to %d ms", repeatInterval);
  server.send(200, "text/plain", "OK");
}

void handleSetLegacyMode() {
  int val = server.arg("value").toInt();
  legacyMode = (val == 1); saveSettings();
  LOG_INFO("Legacy mode set to %d", legacyMode);
  server.send(200, "text/plain", "OK");
}

void handleType() {
  String text = server.arg("text");
  String asciiText; asciiText.reserve(text.length());
  for (size_t i = 0; i < text.length(); i++) { char c = text.charAt(i); if (c >= 32 && c <= 126) asciiText += c; }
  LOG_INFO("Typing text: %s", asciiText.c_str());
  applyModifiers();
  for (size_t i = 0; i < asciiText.length(); i++) {
    char c = asciiText.charAt(i);
    Keyboard.press((uint8_t)c);
    delay(legacyMode ? 10 : 5);
    Keyboard.release((uint8_t)c);
    if (!legacyMode) delay(5);
  }
  server.send(200, "text/plain", "OK");
}

void handleKeyTap() {
  String key = server.arg("key");
  uint8_t code = keyNameToCode(key);
  if (code != 0) { sendKeyTap(code); LOG_INFO("Key tap request: %s (0x%02X)", key.c_str(), code); }
  else LOG_WARN("Unknown key: %s", key.c_str());
  server.send(200, "text/plain", "OK");
}

void handleKeyDown() {
  String key = server.arg("key");
  uint8_t code = keyNameToCode(key);
  if (code == 0) { LOG_WARN("Unknown key down: %s", key.c_str()); server.send(200, "text/plain", "OK"); return; }
  if (isModifierCode(code)) modifierHeldDown(code);
  else { applyModifiers(); Keyboard.press(code); }
  LOG_INFO("Key down: %s (0x%02X)", key.c_str(), code);
  server.send(200, "text/plain", "OK");
}

void handleKeyUp() {
  String key = server.arg("key");
  uint8_t code = keyNameToCode(key);
  if (code == 0) { LOG_WARN("Unknown key up: %s", key.c_str()); server.send(200, "text/plain", "OK"); return; }
  if (isModifierCode(code)) modifierHeldUp(code);
  else { Keyboard.release(code); applyModifiers(); }
  LOG_INFO("Key up: %s (0x%02X)", key.c_str(), code);
  server.send(200, "text/plain", "OK");
}

void handleToggleModifier() {
  String mod = server.arg("mod");
  if (!toggleModifier(mod)) { server.send(400, "text/plain", "Invalid modifier"); return; }
  server.send(200, "text/plain", "OK");
}

void handleResetModifiers() { releaseAllModifiers(); server.send(200, "text/plain", "OK"); }

// ------------------- STA functions (unchanged) -------------------
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
    String currentIP = WiFi.localIP().toString();
    String currentSSID = WiFi.SSID();
    bool changed = sta_status != "Connected" || sta_ip != currentIP || sta_ssid != currentSSID;
    sta_status = "Connected"; sta_ip = currentIP; sta_ssid = currentSSID; sta_error = ""; connecting = false; retryPending = false; sta_retry_count = 0;
    if (logStatus || changed) LOG_SUCCESS("STA connected to %s, IP %s", sta_ssid.c_str(), sta_ip.c_str());
    return;
  }
  sta_ip = ""; sta_ssid = "";
  if (connecting) { sta_status = "Connecting..."; setSTAErrorFromStatus(); if (millis() - connectStartTime > CONNECT_TIMEOUT) sta_error = "Connection timeout"; }
  else if (retryPending) { sta_status = "Retrying..."; sta_error = "Retry scheduled"; }
  else { sta_status = "Disconnected"; setSTAErrorFromStatus(); }
  if (logStatus) LOG_WARN("STA status: %s, error: %s", sta_status.c_str(), sta_error.c_str());
}

void WiFiEvent(WiFiEvent_t event) {
  switch (event) {
    case ARDUINO_EVENT_WIFI_STA_CONNECTED:
      if (connecting) { sta_status = "Connecting..."; sta_error = "Connected, waiting for IP..."; }
      LOG_INFO("STA connected to AP, waiting for IP");
      break;
    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
      updateSTAStatus(true);
      break;
    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
      if (!retryPending) { if (connecting) sta_status = "Connecting..."; else sta_status = "Disconnected"; setSTAErrorFromStatus(); }
      LOG_WARN("STA disconnected: %s", sta_error.c_str());
      break;
    default: break;
  }
}

bool parseBSSID(const String& text, uint8_t out[6]) {
  if (text.length() != 17) return false;
  unsigned int b[6];
  int result = sscanf(text.c_str(), "%2x:%2x:%2x:%2x:%2x:%2x", &b[0], &b[1], &b[2], &b[3], &b[4], &b[5]);
  if (result != 6) return false;
  for (int i = 0; i < 6; i++) { if (b[i] > 255) return false; out[i] = (uint8_t)b[i]; }
  return true;
}

void connectSTA(String ssid, String password, bool hidden, String bssid_str, bool resetRetries = true) {
  if (ssid.length() == 0) { LOG_ERROR("connectSTA called with empty SSID"); return; }
  if (WiFi.status() == WL_CONNECTED) { LOG_WARN("connectSTA aborted: already connected"); return; }
  if (connecting) { LOG_WARN("connectSTA aborted: connection already in progress"); return; }
  if (resetRetries) { sta_retry_count = 0; retryPending = false; }
  if (WiFi.status() != WL_IDLE_STATUS && WiFi.status() != WL_DISCONNECTED) { WiFi.disconnect(); delay(50); }
  int scanState = WiFi.scanComplete();
  if (scanState >= 0) WiFi.scanDelete();
  scanInProgress = false;
  WiFi.disconnect(); delay(50);
  WiFi.mode(WIFI_AP_STA);
  WiFi.setAutoReconnect(false);
  preferences.begin("wifi", false);
  preferences.putString("ssid", ssid);
  preferences.putString("pass", password);
  preferences.putBool("hidden", hidden);
  if (hidden) preferences.putString("bssid", bssid_str);
  else preferences.putString("bssid", "");
  preferences.end();
  connecting = true; retryPending = false; connectStartTime = millis(); lastRetryTime = millis();
  sta_error = "Connecting..."; sta_status = "Connecting..."; sta_ip = ""; sta_ssid = "";
  LOG_INFO("Connecting to STA: %s (hidden=%d, bssid=%s, attempt=%d)", ssid.c_str(), hidden, bssid_str.c_str(), sta_retry_count + 1);
  if (hidden && bssid_str.length() > 0) {
    uint8_t bssid[6];
    if (parseBSSID(bssid_str, bssid)) WiFi.begin(ssid.c_str(), password.c_str(), 0, bssid);
    else { LOG_WARN("Invalid BSSID format: %s; connecting without BSSID", bssid_str.c_str()); WiFi.begin(ssid.c_str(), password.c_str()); }
  } else WiFi.begin(ssid.c_str(), password.c_str());
}

void loadSTAConfig() {
  preferences.begin("wifi", true);
  String ssid = preferences.getString("ssid", "");
  String pass = preferences.getString("pass", "");
  bool hidden = preferences.getBool("hidden", false);
  String bssid = preferences.getString("bssid", "");
  preferences.end();
  if (ssid.length() > 0) { LOG_INFO("Loading saved STA config: %s", ssid.c_str()); connectSTA(ssid, pass, hidden, bssid, true); }
  else LOG_INFO("No saved STA config found");
}

void disconnectSTA() {
  retryPending = false; connecting = false; sta_retry_count = 0; scanInProgress = false;
  int scanState = WiFi.scanComplete();
  if (scanState >= 0) WiFi.scanDelete();
  WiFi.disconnect(); WiFi.mode(WIFI_AP);
  sta_status = "Disconnected"; sta_ip = ""; sta_ssid = ""; sta_error = "Disconnected";
  LOG_INFO("STA disconnected manually");
}

void forgetSTA() {
  preferences.begin("wifi", false); preferences.clear(); preferences.end();
  disconnectSTA();
  LOG_INFO("STA credentials forgotten");
}

void handleSTAStatus() {
  updateSTAStatus(false);
  String json = "{";
  json += "\"connected\":"; json += (WiFi.status() == WL_CONNECTED ? "true" : "false");
  json += ",\"ssid\":\""; json += jsonEscape(sta_ssid); json += "\"";
  json += ",\"ip\":\""; json += jsonEscape(sta_ip); json += "\"";
  json += ",\"status\":\""; json += jsonEscape(sta_status); json += "\"";
  json += ",\"error\":\""; json += jsonEscape(sta_error); json += "\"";
  json += "}";
  server.send(200, "application/json", json);
}

void handleSTAScan() {
  if (connecting) { server.send(409, "application/json", "{\"error\":\"Cannot scan while connecting\"}"); return; }
  int n = WiFi.scanComplete();
  if (n == WIFI_SCAN_RUNNING) { scanInProgress = true; server.send(200, "application/json", "{\"scanning\":true}"); return; }
  if (n == WIFI_SCAN_FAILED) {
    if (scanInProgress) { scanInProgress = false; LOG_ERROR("WiFi scan failed"); server.send(503, "application/json", "{\"error\":\"WiFi scan failed\"}"); return; }
    WiFi.mode(WIFI_AP_STA); LOG_INFO("Starting WiFi scan..."); scanInProgress = true;
    int result = WiFi.scanNetworks(true, true);
    if (result == WIFI_SCAN_FAILED) { scanInProgress = false; LOG_ERROR("Failed to start WiFi scan"); server.send(503, "application/json", "{\"error\":\"WiFi scan failed to start\"}"); return; }
    server.send(200, "application/json", "{\"scanning\":true}"); return;
  }
  if (n >= 0) {
    String json = "["; json.reserve((size_t)n * 130 + 4);
    for (int i = 0; i < n; i++) {
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
      json += "{\"ssid\":\""; json += jsonEscape(ssid);
      json += "\",\"rssi\":"; json += String(rssi);
      json += ",\"encryption\":"; json += String((int)encryption);
      json += ",\"bssid\":\""; json += jsonEscape(bssid);
      json += "\",\"encryption_str\":\""; json += jsonEscape(encType); json += "\"}";
    }
    json += "]";
    LOG_INFO("WiFi scan completed, %d networks found", n);
    scanInProgress = false; WiFi.scanDelete();
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
  bool hidden = hiddenStr == "1" || hiddenStr == "true";
  ssid.trim(); bssid.trim();
  if (ssid.length() == 0) { LOG_ERROR("STA connect called with empty SSID"); server.send(400, "text/plain", "SSID required"); return; }
  if (connecting || WiFi.status() == WL_CONNECTED) { server.send(409, "text/plain", "STA already connected or connecting"); return; }
  if (hidden && bssid.length() > 0) { uint8_t temp[6]; if (!parseBSSID(bssid, temp)) { server.send(400, "text/plain", "Invalid BSSID"); return; } }
  LOG_INFO("STA connect request: ssid=%s, hidden=%d, bssid=%s", ssid.c_str(), hidden, bssid.c_str());
  connectSTA(ssid, password, hidden, bssid, true);
  server.send(200, "text/plain", "OK");
}

void handleSTADisconnect() { LOG_INFO("STA disconnect request"); disconnectSTA(); server.send(200, "text/plain", "OK"); }
void handleSTAForget() { LOG_INFO("STA forget request"); forgetSTA(); server.send(200, "text/plain", "OK"); }

void handleLogs() {
  String json = "[";
  int start = (logHead - logCount + MAX_LOG_ENTRIES) % MAX_LOG_ENTRIES;
  for (int i = 0; i < logCount; i++) {
    int idx = (start + i) % MAX_LOG_ENTRIES;
    if (i) json += ",";
    json += "{";
    json += "\"timestamp\":"; json += String(logBuffer[idx].timestamp);
    json += ",\"level\":\""; json += jsonEscape(String(logBuffer[idx].level)); json += "\"";
    json += ",\"message\":\""; json += jsonEscape(String(logBuffer[idx].message)); json += "\"";
    json += "}";
  }
  json += "]";
  server.send(200, "application/json", json);
}

// ------------------- HID key helpers (unchanged) -------------------
const uint8_t HID_KP_NUMLOCK = 0x53;
const uint8_t HID_KP_SLASH = 0x54;
const uint8_t HID_KP_STAR = 0x55;
const uint8_t HID_KP_MINUS = 0x56;
const uint8_t HID_KP_PLUS = 0x57;
const uint8_t HID_KP_ENTER = 0x58;
const uint8_t HID_KP_1 = 0x59;
const uint8_t HID_KP_2 = 0x5A;
const uint8_t HID_KP_3 = 0x5B;
const uint8_t HID_KP_4 = 0x5C;
const uint8_t HID_KP_5 = 0x5D;
const uint8_t HID_KP_6 = 0x5E;
const uint8_t HID_KP_7 = 0x5F;
const uint8_t HID_KP_8 = 0x60;
const uint8_t HID_KP_9 = 0x61;
const uint8_t HID_KP_0 = 0x62;
const uint8_t HID_KP_DOT = 0x63;

uint8_t keyNameToCode(const String& key) {
  String k = key; k.trim(); k.toUpperCase();
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
  if (k == "NUMLOCK") return HID_KP_NUMLOCK;
  if (k == "MENU") return KEY_MENU;
  if (k == "WINDOWS") return KEY_LEFT_GUI;
  if (k == "CTRL") return KEY_LEFT_CTRL;
  if (k == "ALT") return KEY_LEFT_ALT;
  if (k == "SHIFT") return KEY_LEFT_SHIFT;
  if (k == "KP_SLASH") return HID_KP_SLASH;
  if (k == "KP_ASTERISK") return HID_KP_STAR;
  if (k == "KP_MINUS") return HID_KP_MINUS;
  if (k == "KP_PLUS") return HID_KP_PLUS;
  if (k == "KP_ENTER") return HID_KP_ENTER;
  if (k == "KP_0") return HID_KP_0;
  if (k == "KP_1") return HID_KP_1;
  if (k == "KP_2") return HID_KP_2;
  if (k == "KP_3") return HID_KP_3;
  if (k == "KP_4") return HID_KP_4;
  if (k == "KP_5") return HID_KP_5;
  if (k == "KP_6") return HID_KP_6;
  if (k == "KP_7") return HID_KP_7;
  if (k == "KP_8") return HID_KP_8;
  if (k == "KP_9") return HID_KP_9;
  if (k == "KP_DOT") return HID_KP_DOT;
  if (k.length() == 1) return (uint8_t)k.charAt(0);
  return 0;
}

void sendKeyTap(uint8_t keycode) {
  if (isModifierCode(keycode)) { modifierHeldDown(keycode); delay(legacyMode ? 40 : 20); modifierHeldUp(keycode); }
  else { applyModifiers(); Keyboard.press(keycode); delay(legacyMode ? 40 : 20); Keyboard.release(keycode); }
  LOG_INFO("Key tap: 0x%02X", keycode);
}

// ------------------- WebSocket -------------------
void webSocketEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t length) {
  if (type == WStype_TEXT) {
    String msg; msg.reserve(length + 1);
    for (size_t i = 0; i < length; i++) msg += (char)payload[i];
    msg.trim();
    int dx = 0; int dy = 0;
    if (sscanf(msg.c_str(), "{\"dx\":%d,\"dy\":%d}", &dx, &dy) == 2) {
      dx = clamp(dx, -127, 127);
      dy = clamp(dy, -127, 127);
      Mouse.move(dx, dy, 0);
    } else LOG_WARN("WebSocket unknown message: %s", msg.c_str());
  } else if (type == WStype_CONNECTED) LOG_INFO("WebSocket client connected, id=%u", num);
  else if (type == WStype_DISCONNECTED) LOG_INFO("WebSocket client disconnected, id=%u", num);
}

// ------------------- AP channel selection -------------------
int selectBestChannel() {
  int n = WiFi.scanNetworks(true);
  int attempts = 0;
  while (WiFi.scanComplete() == WIFI_SCAN_RUNNING && attempts++ < 30) delay(100);
  n = WiFi.scanComplete();
  if (n <= 0) { WiFi.scanDelete(); return 1; }
  int channelCount[12] = { 0 };
  for (int i = 0; i < n; i++) { int ch = WiFi.channel(i); if (ch >= 1 && ch <= 11) channelCount[ch]++; }
  WiFi.scanDelete();
  int best = 1; int minCount = channelCount[1];
  for (int ch = 2; ch <= 11; ch++) { if (channelCount[ch] < minCount) { minCount = channelCount[ch]; best = ch; } }
  return best;
}

// ------------------- HTML pages -------------------
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0, user-scalable=no">
<title>ESP32 HID Controller v6</title>
<style>
*{box-sizing:border-box;margin:0;padding:0}
body{font-family:'Segoe UI',Roboto,sans-serif;background:#0b0b0b;color:#eee;padding:12px;min-height:100vh}
.container{max-width:1200px;margin:0 auto;display:grid;grid-template-columns:1fr;gap:16px}
@media(min-width:780px){.container{grid-template-columns:1fr 1fr}.full-width{grid-column:1/-1}}
.card{background:#1e1e1e;border-radius:16px;padding:18px;border:1px solid #333;box-shadow:0 8px 20px rgba(0,0,0,0.5)}
h2{font-size:1.6rem;color:#5b9aff;text-align:center;margin-bottom:10px;font-weight:300;letter-spacing:1px}
h3{font-size:1.2rem;color:#aaa;text-align:center;margin-bottom:14px;font-weight:400}
.slider-group{display:flex;flex-wrap:wrap;align-items:center;justify-content:center;gap:10px 20px;margin:8px 0}
.slider-group label{font-size:14px;color:#ccc}
input[type=range]{flex:1;min-width:120px;height:4px;appearance:none;background:#444;border-radius:2px;outline:none}
input[type=range]::-webkit-slider-thumb{appearance:none;width:16px;height:16px;border-radius:50%;background:#5b9aff;cursor:pointer}
.slider-value{min-width:40px;text-align:center;color:#5b9aff;font-weight:600}
.btn-group{display:flex;flex-wrap:wrap;gap:8px;justify-content:center;margin:8px 0}
button{padding:8px 16px;background:#2a2a2a;color:#eee;border:1px solid #444;border-radius:8px;cursor:pointer;font-size:14px;transition:0.15s;font-weight:500;box-shadow:0 2px 4px rgba(0,0,0,0.3);user-select:none;-webkit-user-select:none}
button:hover{background:#3a3a3a;transform:translateY(-1px)}
button:active{transform:translateY(0);background:#444}
button.accent{background:#2c5f8a;border-color:#3a7bbd}
button.accent:hover{background:#3a7bbd}
button.mod-active{background:#f39c12;color:#000;border-color:#f1c40f}
button.pressed{background:#f39c12;color:#000;border-color:#f1c40f}
#pad{width:100%;height:200px;background:#181818;border-radius:12px;border:2px solid #333;touch-action:none;cursor:crosshair;margin:10px 0}
#pad:active{border-color:#5b9aff}
.arrow-row{display:flex;justify-content:center;gap:6px;margin:4px 0}
.arrow-row button{min-width:48px;height:44px;font-size:18px}
.kb-grid{display:flex;flex-direction:column;gap:5px;width:100%;max-width:900px;margin:12px auto 0;overflow-x:auto}
.kb-row{display:flex;gap:5px;width:max-content;min-width:100%}
.kb-key{flex:0 0 44px;height:42px;background:#2a2a2a;border:1px solid #444;border-radius:6px;color:#eee;display:flex;align-items:center;justify-content:center;font-size:13px;cursor:pointer;user-select:none;-webkit-user-select:none;touch-action:none;transition:0.1s}
.kb-key:hover{background:#3a3a3a}
.kb-key:active{background:#444}
.kb-key.special{background:#2c3e50}
.kb-key.special:hover{background:#3e5a6f}
.kb-key.mod-down{background:#5b9aff;color:#000}
.kb-key.last-clicked{background:#5b9aff;color:#000;border-color:#7ab7ff}
.numpad{display:grid;grid-template-columns:repeat(4,48px);grid-template-rows:repeat(5,42px);gap:5px;width:max-content;margin:12px auto 0}
.numpad .kb-key{width:48px;height:42px;min-height:42px;flex:none}
.row-label{font-size:12px;color:#666;text-align:center;margin:6px 0 2px}
.text-input-area{display:flex;gap:10px;flex-wrap:wrap;justify-content:center;margin:10px 0}
input[type=text],input[type=password]{background:#222;border:1px solid #444;border-radius:8px;padding:8px 14px;color:#eee;font-size:16px;flex:1;min-width:160px;max-width:380px;outline:none}
input[type=text]:focus,input[type=password]:focus{border-color:#5b9aff}
.small{font-size:12px;color:#777;text-align:center;margin-top:6px}
.log-panel{background:#121212;border:1px solid #333;border-radius:8px;padding:10px;max-height:180px;overflow-y:auto;margin-top:10px;display:none;font-family:monospace;font-size:12px}
.log-panel.visible{display:block}
.log-entry{padding:2px 0;border-bottom:1px solid #1a1a1a}
.log-info{color:#aaa}
.log-warn{color:#f39c12}
.log-error{color:#e74c3c}
.log-success{color:#2ecc71}
.sta-status{background:#1a1a1a;border-radius:8px;padding:8px 16px;margin-bottom:10px;display:flex;flex-wrap:wrap;align-items:center;justify-content:space-between;border-left:4px solid #555}
.sta-status .label{font-weight:500;color:#aaa}
.sta-status .status-text{color:#eee}
.sta-status .connected{color:#2ecc71}
.sta-status .disconnected{color:#e74c3c}
.sta-status .ip{color:#5b9aff}
.kb-key.w75{flex-basis:75px}
.kb-key.w95{flex-basis:95px}
.kb-key.w110{flex-basis:110px}
.kb-key.w125{flex-basis:125px}
.kb-key.space{flex-basis:220px}
@media(max-width:600px){.kb-grid{width:100%;overflow-x:hidden}.kb-key{flex-basis:34px;height:38px;font-size:10px}.kb-key.w75{flex-basis:48px}.kb-key.w95{flex-basis:60px}.kb-key.w110{flex-basis:70px}.kb-key.w125{flex-basis:82px}.kb-key.space{flex-basis:150px}}
</style>
</head>
<body>
<div class="container">
<div class="card full-width">
<div style="display:flex;justify-content:space-between;align-items:center;">
<h2 style="margin:0;">⚡ ESP32 HID Controller v6</h2>
<a href="/sta" style="color:#5b9aff;font-size:20px;text-decoration:none;">📶</a>
</div>
<div class="sta-status" id="staStatus">
<span class="label">Wi-Fi:</span>
<span class="status-text" id="staStatusText">Loading...</span>
<span style="margin-left:auto;"><button onclick="window.location.href='/sta'" style="background:#333;padding:4px 12px;font-size:12px;">Settings</button></span>
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
<!-- Mouse card -->
<div class="card">
<h3>🖱 Mouse</h3>
<div id="pad"></div>
<div class="arrow-row"><button data-dx="0" data-dy="-12">▲</button></div>
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
<button id="mouseLeftDown" data-btn="left">L⬇</button>
<button id="mouseLeftUp" data-btn="left" onclick="mouseUp('left')">L⬆</button>
<button id="mouseRightDown" data-btn="right">R⬇</button>
<button id="mouseRightUp" data-btn="right" onclick="mouseUp('right')">R⬆</button>
<button onclick="sendHTTP('/wheel?delta=-1')">⬆</button>
<button onclick="sendHTTP('/wheel?delta=1')">⬇</button>
</div>
<div class="small">Drag on pad to move. Tap for left click. Arrows hold-to-repeat.</div>
</div>
<!-- Keyboard card -->
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
<input type="text" id="realtimeInput" placeholder="Real-time typing" autocomplete="off">
</div>
<div id="keyboard" class="kb-grid"></div>
<div class="row-label">Numpad</div>
<div id="numpad" class="numpad"></div>
<div class="small">Sticky modifiers toggled via buttons above. Keyboard keys press/release on hold.</div>
</div>
<!-- Firmware Update card (full width) -->
<div class="card full-width">
<h3>⚙️ Firmware Update</h3>
<div style="display:flex;flex-wrap:wrap;gap:10px;justify-content:center;align-items:center;">
<input type="text" id="verUrl" placeholder="Version URL" style="flex:2;min-width:200px;">
<input type="text" id="binUrl" placeholder="Firmware URL" style="flex:2;min-width:200px;">
<button onclick="saveUrls()">Save URLs</button>
<button onclick="checkUpdate()">Check for Update</button>
</div>
<div style="margin-top:10px;">
<form id="uploadForm" enctype="multipart/form-data" method="POST" action="/upload" style="display:flex;flex-wrap:wrap;gap:10px;justify-content:center;align-items:center;">
<input type="file" name="firmware" accept=".bin" style="background:#222;border:1px solid #444;border-radius:8px;padding:6px;color:#eee;">
<button type="submit">Upload & Update</button>
</form>
</div>
<div class="small">Current version: 5.0.0</div>
</div>
</div>
<script>
let logs = [];
const LOG_KEY = "esp32_logs";
function loadLogs() {
  try {
    const s = sessionStorage.getItem(LOG_KEY);
    if (s) logs = JSON.parse(s);
  } catch(e) { logs = []; }
}
function saveLogs() {
  try { sessionStorage.setItem(LOG_KEY, JSON.stringify(logs)); } catch(e) {}
}
function addLog(level, msg) {
  logs.push({ ts: new Date().toISOString(), level: level, msg: msg });
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
    div.textContent = '[' + e.ts + '] ' + e.level.toUpperCase() + ': ' + e.msg;
    panel.appendChild(div);
  });
  panel.scrollTop = panel.scrollHeight;
}
function toggleLogs() {
  const panel = document.getElementById('logPanel');
  panel.classList.toggle('visible');
  if (panel.classList.contains('visible')) renderLogs();
}
function clearLogs() {
  logs = [];
  saveLogs();
  renderLogs();
  logInfo('Logs cleared');
}
loadLogs(); renderLogs(); logInfo('Page loaded');

let socket = null;
function connectWS() {
  try {
    socket = new WebSocket('ws://' + location.hostname + ':81/');
    socket.onopen = () => { logSuccess('WebSocket connected'); };
    socket.onclose = () => { logWarn('WebSocket closed, reconnecting...'); setTimeout(connectWS, 2000); };
    socket.onerror = () => { logError('WebSocket error'); try { socket.close(); } catch(e) {} };
  } catch(e) {
    logError('WebSocket creation failed');
    setTimeout(connectWS, 2000);
  }
}
connectWS();

function sendHTTP(url) {
  return fetch(url, { cache: 'no-store' })
    .then(res => {
      if (!res.ok) logWarn('HTTP ' + res.status + ' for ' + url);
      else logInfo('HTTP OK: ' + url);
      return res;
    })
    .catch(err => {
      logError('Fetch failed: ' + err.message);
      throw err;
    });
}

function sendMove(dx, dy) {
  const realDx = Math.max(-127, Math.min(127, Math.round(dx * sens)));
  const realDy = Math.max(-127, Math.min(127, Math.round(dy * sens)));
  if (socket && socket.readyState === WebSocket.OPEN) {
    socket.send('{"dx":' + realDx + ',"dy":' + realDy + '}');
  } else {
    sendHTTP('/move?dx=' + encodeURIComponent(realDx) + '&dy=' + encodeURIComponent(realDy));
  }
}

let sens = 2.0;
let repeatInterval = 100;
let legacyMode = false;
let sensSaveTimer = null, repeatSaveTimer = null;

document.getElementById('sens').addEventListener('input', function() {
  sens = parseFloat(this.value);
  document.getElementById('sensVal').textContent = sens.toFixed(1);
  clearTimeout(sensSaveTimer);
  sensSaveTimer = setTimeout(() => {
    sendHTTP('/set_sensitivity?value=' + encodeURIComponent(sens)).catch(() => {});
  }, 400);
  logInfo('Sensitivity = ' + sens);
});

document.getElementById('repeatRate').addEventListener('input', function() {
  repeatInterval = parseInt(this.value);
  document.getElementById('repeatVal').textContent = repeatInterval;
  clearTimeout(repeatSaveTimer);
  repeatSaveTimer = setTimeout(() => {
    sendHTTP('/set_repeat?value=' + encodeURIComponent(repeatInterval)).catch(() => {});
  }, 400);
  logInfo('Repeat interval = ' + repeatInterval);
});

document.getElementById('legacyCheck').addEventListener('change', function() {
  legacyMode = this.checked;
  sendHTTP('/set_legacy?value=' + (legacyMode ? 1 : 0)).catch(() => {});
  logInfo('Legacy mode = ' + legacyMode);
});

function updateSTAStatus() {
  fetch('/sta/status', { cache: 'no-store' })
    .then(res => res.json())
    .then(data => {
      const statusText = document.getElementById('staStatusText');
      statusText.textContent = '';
      if (data.connected) {
        const connected = document.createElement('span');
        connected.className = 'connected';
        connected.textContent = 'Connected';
        statusText.appendChild(connected);
        statusText.appendChild(document.createTextNode(' to '));
        const strong = document.createElement('strong');
        strong.textContent = data.ssid || '';
        statusText.appendChild(strong);
        statusText.appendChild(document.createTextNode(' (IP: '));
        const ip = document.createElement('span');
        ip.className = 'ip';
        ip.textContent = data.ip || '';
        statusText.appendChild(ip);
        statusText.appendChild(document.createTextNode(')'));
      } else {
        const disconnected = document.createElement('span');
        disconnected.className = 'disconnected';
        disconnected.textContent = 'Disconnected';
        statusText.appendChild(disconnected);
        statusText.appendChild(document.createTextNode(' – ' + (data.error || 'Not connected')));
      }
    })
    .catch(() => {});
}
setInterval(updateSTAStatus, 3000);
updateSTAStatus();

// ---------- Mouse pad ----------
const pad = document.getElementById('pad');
let padDown = false, startX = 0, startY = 0, lastX = 0, lastY = 0, moved = false, startTime = 0;
pad.addEventListener('pointerdown', e => {
  pad.setPointerCapture(e.pointerId);
  padDown = true; startX = e.clientX; startY = e.clientY; lastX = e.clientX; lastY = e.clientY; moved = false; startTime = Date.now();
  e.preventDefault();
});
pad.addEventListener('pointermove', e => {
  if (!padDown) return;
  const dx = e.clientX - lastX;
  const dy = e.clientY - lastY;
  lastX = e.clientX; lastY = e.clientY;
  if (Math.abs(e.clientX - startX) > 5 || Math.abs(e.clientY - startY) > 5) moved = true;
  if (moved) sendMove(dx, dy);
  e.preventDefault();
});
pad.addEventListener('pointerup', e => {
  if (!padDown) return;
  padDown = false;
  if (!moved && Date.now() - startTime < 300) sendHTTP('/click?btn=left').catch(() => {});
  e.preventDefault();
});
pad.addEventListener('pointercancel', () => { padDown = false; });
pad.addEventListener('lostpointercapture', () => { padDown = false; });

// ---------- Arrow repeat ----------
let repeatTimer = null;
function stopArrowRepeat() { if (repeatTimer !== null) { clearInterval(repeatTimer); repeatTimer = null; } }
document.querySelectorAll('.arrow-row button[data-dx]').forEach(btn => {
  const dx = parseInt(btn.dataset.dx);
  const dy = parseInt(btn.dataset.dy);
  btn.addEventListener('pointerdown', e => {
    e.preventDefault();
    try { btn.setPointerCapture(e.pointerId); } catch(_) {}
    stopArrowRepeat();
    sendMove(dx, dy);
    repeatTimer = setInterval(() => { sendMove(dx, dy); }, repeatInterval);
  });
  btn.addEventListener('pointerup', stopArrowRepeat);
  btn.addEventListener('pointercancel', stopArrowRepeat);
  btn.addEventListener('lostpointercapture', stopArrowRepeat);
});

// ---------- Mouse hold ----------
const mouseState = { left: false, right: false };
function updateMouseUI() {
  document.getElementById('mouseLeftDown').classList.toggle('pressed', mouseState.left);
  document.getElementById('mouseRightDown').classList.toggle('pressed', mouseState.right);
}
function mouseDown(btn) {
  if (btn === 'left' && !mouseState.left) { mouseState.left = true; sendHTTP('/down?btn=left').catch(() => {}); }
  else if (btn === 'right' && !mouseState.right) { mouseState.right = true; sendHTTP('/down?btn=right').catch(() => {}); }
  updateMouseUI();
}
function mouseUp(btn) {
  if (btn === 'left' && mouseState.left) { mouseState.left = false; sendHTTP('/up?btn=left').catch(() => {}); }
  else if (btn === 'right' && mouseState.right) { mouseState.right = false; sendHTTP('/up?btn=right').catch(() => {}); }
  updateMouseUI();
}
function releaseMouseButtons() {
  if (mouseState.left) sendHTTP('/up?btn=left').catch(() => {});
  if (mouseState.right) sendHTTP('/up?btn=right').catch(() => {});
  mouseState.left = false; mouseState.right = false; updateMouseUI();
}
function setupMouseHold(id, btn) {
  const el = document.getElementById(id);
  el.addEventListener('pointerdown', e => { e.preventDefault(); try { el.setPointerCapture(e.pointerId); } catch(_) {} mouseDown(btn); });
  el.addEventListener('pointerup', e => { e.preventDefault(); mouseUp(btn); });
  el.addEventListener('pointercancel', () => { mouseUp(btn); });
  el.addEventListener('lostpointercapture', () => { mouseUp(btn); });
}
setupMouseHold('mouseLeftDown', 'left');
setupMouseHold('mouseRightDown', 'right');

// ---------- Modifiers ----------
const modState = { CTRL: false, ALT: false, SHIFT: false, WIN: false };
document.querySelectorAll('#modButtons button').forEach(btn => {
  btn.addEventListener('click', () => {
    const mod = btn.dataset.mod;
    modState[mod] = !modState[mod];
    btn.classList.toggle('mod-active', modState[mod]);
    sendHTTP('/toggle_modifier?mod=' + encodeURIComponent(mod)).catch(() => {});
    logInfo('Sticky ' + mod + ' = ' + modState[mod]);
  });
});

// ---------- Keyboard hold ----------
const heldKeyCounts = new Map();
function holdKey(code) { const count = heldKeyCounts.get(code) || 0; heldKeyCounts.set(code, count + 1); }
function releaseKey(code) { const count = heldKeyCounts.get(code) || 0; if (count <= 1) heldKeyCounts.delete(code); else heldKeyCounts.set(code, count - 1); }
function releaseHeldKeys() {
  heldKeyCounts.forEach((count, code) => { if (count > 0) sendHTTP('/key_up?key=' + encodeURIComponent(code)).catch(() => {}); });
  heldKeyCounts.clear();
  document.querySelectorAll('.mod-down').forEach(el => el.classList.remove('mod-down'));
  sendHTTP('/reset_modifiers').catch(() => {});
  Object.keys(modState).forEach(k => { modState[k] = false; });
  document.querySelectorAll('#modButtons button').forEach(btn => btn.classList.remove('mod-active'));
  logInfo('All held keys/modifiers released');
}
window.addEventListener('blur', () => { stopArrowRepeat(); releaseMouseButtons(); releaseHeldKeys(); });
document.addEventListener('visibilitychange', () => { if (document.hidden) { stopArrowRepeat(); releaseMouseButtons(); releaseHeldKeys(); } });

// ---------- Keyboard layout ----------
const keyMap = {
  'Esc':'ESC', 'Backspace':'BACKSPACE', 'Tab':'TAB', 'CapsLock':'CAPSLOCK', 'Enter':'ENTER',
  'Shift':'SHIFT', 'Ctrl':'CTRL', 'Alt':'ALT', 'Win':'WINDOWS', 'Space':'SPACE',
  'Insert':'INSERT', 'Home':'HOME', 'PageUp':'PAGEUP', 'Delete':'DELETE', 'End':'END', 'PageDown':'PAGEDOWN',
  'Up':'UP', 'Down':'DOWN', 'Left':'LEFT', 'Right':'RIGHT',
  'PrtSc':'PRTSC', 'ScrLk':'SCRLK', 'Pause':'PAUSE', 'NumLock':'NUMLOCK', 'Menu':'MENU',
  'Num /':'KP_SLASH', 'Num *':'KP_ASTERISK', 'Num -':'KP_MINUS', 'Num +':'KP_PLUS', 'Num Enter':'KP_ENTER',
  '0':'KP_0','1':'KP_1','2':'KP_2','3':'KP_3','4':'KP_4','5':'KP_5','6':'KP_6','7':'KP_7','8':'KP_8','9':'KP_9','.':'KP_DOT'
};
for (let i = 1; i <= 12; i++) keyMap['F' + i] = 'F' + i;

let lastClickedKey = null;
function highlightKey(el) {
  if (lastClickedKey && lastClickedKey !== el) lastClickedKey.classList.remove('last-clicked');
  if (el) { el.classList.add('last-clicked'); lastClickedKey = el; }
  else lastClickedKey = null;
}

function buildKeyboard() {
  const grid = document.getElementById('keyboard');
  grid.innerHTML = '';
  const keyboardRows = [
    [['Esc','w75'],['F1',''],['F2',''],['F3',''],['F4',''],['F5',''],['F6',''],['F7',''],['F8',''],['F9',''],['F10',''],['F11',''],['F12',''],['PrtSc','w75'],['ScrLk','w75'],['Pause','w75']],
    [['`',''],['1',''],['2',''],['3',''],['4',''],['5',''],['6',''],['7',''],['8',''],['9',''],['0',''],['-',''],['=',''],['Backspace','w110']],
    [['Tab','w75'],['q',''],['w',''],['e',''],['r',''],['t',''],['y',''],['u',''],['i',''],['o',''],['p',''],['[',''],[']',''],['\\','w75']],
    [['CapsLock','w95'],['a',''],['s',''],['d',''],['f',''],['g',''],['h',''],['j',''],['k',''],['l',''],[';',''],["'",''],['Enter','w95']],
    [['Shift','w125'],['z',''],['x',''],['c',''],['v',''],['b',''],['n',''],['m',''],[' ',''],['.',''],['/',''],['Shift','w125']],
    [['Ctrl','w75'],['Win','w75'],['Alt','w75'],['Space','space'],['Alt','w75'],['Win','w75'],['Menu','w75'],['Ctrl','w75']]
  ];
  keyboardRows.forEach(row => {
    const rowEl = document.createElement('div');
    rowEl.className = 'kb-row';
    row.forEach(([label, size]) => {
      const el = document.createElement('div');
      el.className = 'kb-key';
      if (size) el.classList.add(size);
      el.textContent = label;
      if (['Esc','F1','F2','F3','F4','F5','F6','F7','F8','F9','F10','F11','F12','PrtSc','ScrLk','Pause'].includes(label)) el.classList.add('special');
      const isMod = ['Shift','Ctrl','Alt','Win'].includes(label);
      if (isMod) {
        el.addEventListener('pointerdown', e => {
          e.preventDefault();
          try { el.setPointerCapture(e.pointerId); } catch(_) {}
          const code = keyMap[label];
          if (!code) return;
          sendHTTP('/key_down?key=' + encodeURIComponent(code)).catch(() => {});
          holdKey(code);
          el.classList.add('mod-down');
        });
        el.addEventListener('pointerup', e => {
          e.preventDefault();
          const code = keyMap[label];
          if (!code) return;
          sendHTTP('/key_up?key=' + encodeURIComponent(code)).catch(() => {});
          releaseKey(code);
          el.classList.remove('mod-down');
        });
        el.addEventListener('pointercancel', () => {
          const code = keyMap[label];
          if (!code) return;
          sendHTTP('/key_up?key=' + encodeURIComponent(code)).catch(() => {});
          releaseKey(code);
          el.classList.remove('mod-down');
        });
      } else {
        el.addEventListener('click', () => {
          let code = keyMap[label];
          if (code) { sendHTTP('/key?key=' + encodeURIComponent(code)).catch(() => {}); }
          else if (label.length === 1) { sendHTTP('/type?text=' + encodeURIComponent(label)).catch(() => {}); }
          else { sendHTTP('/key?key=' + encodeURIComponent(label)).catch(() => {}); }
          highlightKey(el);
        });
      }
      rowEl.appendChild(el);
    });
    grid.appendChild(rowEl);
  });
}

function buildNumpad() {
  const container = document.getElementById('numpad');
  container.innerHTML = '';
  const keys = [['NumLock',1],['Num /',1],['Num *',1],['Num -',1],['KP_7',1],['KP_8',1],['KP_9',1],['KP_4',1],['KP_5',1],['KP_6',1],['KP_1',1],['KP_2',1],['KP_3',1],['KP_0',2],['KP_DOT',1]];
  keys.forEach(([label, colSpan]) => {
    const el = document.createElement('div');
    el.className = 'kb-key';
    if (colSpan === 2) el.style.gridColumn = 'span 2';
    const displayName = { 'KP_0':'0','KP_1':'1','KP_2':'2','KP_3':'3','KP_4':'4','KP_5':'5','KP_6':'6','KP_7':'7','KP_8':'8','KP_9':'9','KP_DOT':'.' }[label] || label;
    el.textContent = displayName;
    if (label === 'NumLock') {
      el.addEventListener('click', () => { const code = keyMap[label]; if (!code) return; sendHTTP('/key?key=' + encodeURIComponent(code)).catch(() => {}); highlightKey(el); });
    } else {
      el.addEventListener('click', () => { const code = keyMap[label]; if (code) sendHTTP('/key?key=' + encodeURIComponent(code)).catch(() => {}); highlightKey(el); });
    }
    container.appendChild(el);
  });
  const plus = document.createElement('div');
  plus.className = 'kb-key';
  plus.textContent = '+';
  plus.style.gridColumn = '4';
  plus.style.gridRow = '2 / span 2';
  plus.addEventListener('click', () => { sendHTTP('/key?key=KP_PLUS').catch(() => {}); highlightKey(plus); });
  container.appendChild(plus);
  const enter = document.createElement('div');
  enter.className = 'kb-key';
  enter.textContent = 'Enter';
  enter.style.gridColumn = '4';
  enter.style.gridRow = '4 / span 2';
  enter.addEventListener('click', () => { sendHTTP('/key?key=KP_ENTER').catch(() => {}); highlightKey(enter); });
  container.appendChild(enter);
}

buildKeyboard();
buildNumpad();

function sendText() {
  const input = document.getElementById('textInput');
  const val = input.value;
  if (!val) return;
  sendHTTP('/type?text=' + encodeURIComponent(val)).catch(() => {});
  input.value = '';
}

let realtimeOld = '';
const realInput = document.getElementById('realtimeInput');
realInput.addEventListener('focus', () => { realtimeOld = realInput.value; });
realInput.addEventListener('input', function() {
  const newVal = this.value;
  if (newVal.startsWith(realtimeOld) && newVal.length > realtimeOld.length) {
    const inserted = newVal.substring(realtimeOld.length);
    sendHTTP('/type?text=' + encodeURIComponent(inserted)).catch(() => {});
    realtimeOld = newVal;
    return;
  }
  if (realtimeOld.startsWith(newVal) && newVal.length < realtimeOld.length) {
    const delCount = realtimeOld.length - newVal.length;
    for (let i = 0; i < delCount; i++) sendHTTP('/key?key=BACKSPACE').catch(() => {});
    realtimeOld = newVal;
    return;
  }
  if (newVal !== realtimeOld) {
    sendHTTP('/key?key=CTRL')
      .then(() => sendHTTP('/key_down?key=CTRL'))
      .then(() => sendHTTP('/key?key=A'))
      .then(() => sendHTTP('/key_up?key=CTRL'))
      .then(() => sendHTTP('/type?text=' + encodeURIComponent(newVal)))
      .catch(() => {});
    realtimeOld = newVal;
  }
});

function sleep(ms) { return new Promise(resolve => setTimeout(resolve, ms)); }

async function testAll() {
  releaseHeldKeys();
  logInfo('=== Starting test ===');
  try {
    await sendHTTP('/click?btn=left'); await sleep(200);
    await sendHTTP('/click?btn=right'); await sleep(200);
    await sendHTTP('/move?dx=30&dy=0'); await sleep(200);
    await sendHTTP('/move?dx=0&dy=30'); await sleep(200);
    await sendHTTP('/wheel?delta=1'); await sleep(200);
    await sendHTTP('/type?text=Hello'); await sleep(300);
    await sendHTTP('/key?key=ENTER'); await sleep(200);
    await sendHTTP('/key?key=BACKSPACE'); await sleep(200);
    await sendHTTP('/toggle_modifier?mod=SHIFT'); await sleep(200);
    await sendHTTP('/type?text=a'); await sleep(200);
    await sendHTTP('/toggle_modifier?mod=SHIFT'); await sleep(200);
    logSuccess('Test complete');
  } catch(e) { logError('Test failed: ' + e.message); }
}

sendHTTP('/reset_modifiers').catch(() => {});
logInfo('Modifiers reset');

// ---------- Update functions ----------
function saveUrls() {
  const ver = document.getElementById('verUrl').value.trim();
  const bin = document.getElementById('binUrl').value.trim();
  if (!ver || !bin) { alert('Both URLs required'); return; }
  sendHTTP('/set_urls?ver=' + encodeURIComponent(ver) + '&bin=' + encodeURIComponent(bin))
    .then(() => { alert('URLs saved'); })
    .catch(() => { alert('Failed to save URLs'); });
}

function checkUpdate() {
  sendHTTP('/check_update')
    .then(() => { alert('Update check started, check logs.'); })
    .catch(() => { alert('Failed to start update check.'); });
}

// Load current URLs into fields
function loadCurrentUrls() {
  // We could fetch from /get_urls, but we'll just rely on defaults.
  // We'll fill with placeholders.
  document.getElementById('verUrl').value = 'https://example.com/version.txt';
  document.getElementById('binUrl').value = 'https://example.com/firmware.bin';
}
loadCurrentUrls();

</script>
</body>
</html>
)rawliteral";

const char sta_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>WiFi Settings</title>
<style>
body{font-family:'Segoe UI',Roboto,sans-serif;background:#0b0b0b;color:#eee;padding:20px}
.container{max-width:500px;margin:0 auto;background:#1e1e1e;border-radius:16px;padding:24px;border:1px solid #333}
h2{color:#5b9aff;text-align:center}
label{display:block;margin:12px 0 4px;color:#aaa}
input[type=text],input[type=password]{width:100%;padding:8px;background:#222;border:1px solid #444;border-radius:6px;color:#eee}
input[type=checkbox]{margin-right:8px}
button{padding:10px 20px;background:#5b9aff;border:none;border-radius:8px;color:#fff;font-weight:bold;cursor:pointer;margin-top:12px}
button:hover{background:#3a7bbd}
button.secondary{background:#444}
.status-box{background:#111;padding:10px;border-radius:8px;margin:12px 0}
.connected{color:#2ecc71}
.disconnected{color:#e74c3c}
.info{color:#aaa;font-size:14px}
.network-list{max-height:200px;overflow-y:auto;background:#111;border-radius:6px;padding:4px;margin-top:6px}
.network-item{padding:6px 8px;cursor:pointer;border-bottom:1px solid #222;display:flex;justify-content:space-between;align-items:center;gap:10px}
.network-item:hover{background:#2a2a2a}
.network-item .bssid{color:#888;font-size:12px}
.network-item .rssi{color:#666;font-size:12px}
.network-item .enc{color:#5b9aff;font-size:12px}
.hidden-note{background:#2a2a2a;padding:8px;border-radius:6px;margin:8px 0;font-size:14px;border-left:3px solid #f39c12}
.scanning-msg{text-align:center;color:#aaa;padding:10px}
.error-msg{color:#e74c3c}
</style>
</head>
<body>
<div class="container">
<h2>🔧 WiFi Settings</h2>
<div id="statusBox" class="status-box">Loading...</div>
<div class="hidden-note">⚠️ <strong>Hidden networks</strong> are included in scans. If your network does not broadcast its SSID, you can also manually enter the SSID and BSSID.</div>
<label>SSID</label>
<input type="text" id="ssid" placeholder="Network name">
<div id="networkList" class="network-list"><div class="scanning-msg">Scanning for networks...</div></div>
<label>Password</label>
<input type="password" id="pass" placeholder="Password">
<label>BSSID (MAC) <span class="info">(optional)</span></label>
<input type="text" id="bssid" placeholder="xx:xx:xx:xx:xx:xx">
<label><input type="checkbox" id="hiddenCheck"> Hidden network</label>
<button onclick="connect()">Connect</button>
<button onclick="disconnect()" class="secondary">Disconnect</button>
<button onclick="forget()" class="secondary" style="background:#722;">Forget</button>
<br>
<button onclick="window.location.href='/'" style="background:#333;">← Back to HID</button>
</div>
<script>
function updateStatus() {
  fetch('/sta/status', { cache: 'no-store' })
    .then(r => r.json())
    .then(data => {
      const box = document.getElementById('statusBox');
      box.textContent = '';
      if (data.connected) {
        const status = document.createElement('span');
        status.className = 'connected';
        status.textContent = 'Connected';
        box.appendChild(status);
        box.appendChild(document.createTextNode(' to '));
        const ssid = document.createElement('strong');
        ssid.textContent = data.ssid || '';
        box.appendChild(ssid);
        box.appendChild(document.createElement('br'));
        box.appendChild(document.createTextNode('IP: ' + (data.ip || '')));
      } else {
        const status = document.createElement('span');
        status.className = 'disconnected';
        status.textContent = 'Disconnected';
        box.appendChild(status);
        box.appendChild(document.createTextNode(' – ' + (data.error || 'Idle')));
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
  fetch('/sta/scan', { cache: 'no-store' })
    .then(r => r.json())
    .then(data => {
      if (data && data.scanning) {
        listDiv.innerHTML = '<div class="scanning-msg">Scanning, please wait...</div>';
        scanAttempts++;
        if (scanAttempts < MAX_SCAN_ATTEMPTS) setTimeout(scanNetworks, 2000);
        else { listDiv.innerHTML = '<div class="scanning-msg error-msg">Scan timed out.</div>'; scanAttempts = 0; }
        return;
      }
      if (!Array.isArray(data)) throw new Error(data.error || 'Invalid scan response');
      if (data.length === 0) { listDiv.innerHTML = '<div class="scanning-msg">No networks found.</div>'; scanAttempts = 0; return; }
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
        item.addEventListener('click', () => { selectNetwork(net.ssid || '', net.bssid || ''); });
        listDiv.appendChild(item);
      });
      scanAttempts = 0;
    })
    .catch(err => {
      listDiv.innerHTML = '<div class="scanning-msg error-msg">Error scanning. Refresh to retry.</div>';
      scanAttempts = 0;
      console.error(err);
    });
}

function selectNetwork(ssid, bssid) {
  document.getElementById('ssid').value = ssid;
  document.getElementById('bssid').value = bssid;
  document.getElementById('hiddenCheck').checked = !ssid;
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
        setTimeout(updateStatus, 500);
      } else return res.text().then(text => { alert('Failed: ' + text); });
    })
    .catch(() => { alert('Network error.'); });
}

function disconnect() { fetch('/sta/disconnect').then(() => updateStatus()).catch(() => {}); }
function forget() { if (!confirm('Forget saved WiFi credentials?')) return; fetch('/sta/forget').then(() => updateStatus()).catch(() => {}); }

window.onload = function() { setTimeout(scanNetworks, 500); };
</script>
</body>
</html>
)rawliteral";

void handleRoot() { server.send(200, "text/html", index_html); }
void handleSTA() { server.send(200, "text/html", sta_html); }

// ------------------- Setup & Loop -------------------
void setup() {
  Serial.begin(115200);
  delay(100);
  LOG_INFO("ESP32 HID Controller v6 starting...");
  USB.begin();
  Keyboard.begin();
  Mouse.begin();
  LOG_INFO("USB HID initialised");
  loadSettings();
  loadUpdateUrls();
  WiFi.mode(WIFI_AP);
  int bestChannel = selectBestChannel();
  LOG_INFO("Selected Wi‑Fi channel: %d", bestChannel);
  WiFi.softAP(ap_ssid, ap_password, bestChannel, true);
  IPAddress apIP = WiFi.softAPIP();
  LOG_INFO("AP mode started, IP: %s", apIP.toString().c_str());
  dnsServer.start(53, "*", apIP);
  WiFi.onEvent(WiFiEvent);

  // --- Web routes ---
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
  server.on("/sta", handleSTA);
  server.on("/sta/status", handleSTAStatus);
  server.on("/sta/scan", handleSTAScan);
  server.on("/sta/connect", handleSTAConnect);
  server.on("/sta/disconnect", handleSTADisconnect);
  server.on("/sta/forget", handleSTAForget);
  server.on("/logs", handleLogs);
  // New update routes
  server.on("/set_urls", handleSetUpdateUrls);
  server.on("/check_update", handleCheckUpdate);
  server.on("/upload", HTTP_POST, []() {
    server.send(200, "text/plain", "Update " + (Update.hasError() ? "failed" : "success"));
  }, handleUpload);
  server.onNotFound([apIP]() {
    server.sendHeader("Location", "http://" + apIP.toString() + "/", true);
    server.send(302, "text/plain", "");
  });
  server.begin();
  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
  LOG_INFO("Setup complete.");
}

void loop() {
  dnsServer.processNextRequest();
  server.handleClient();
  webSocket.loop();

  if (!staStarted && millis() > 5000UL) {
    staStarted = true;
    LOG_INFO("Delayed STA connection starting...");
    loadSTAConfig();
  }

  if (connecting && WiFi.status() != WL_CONNECTED) {
    if (millis() - connectStartTime >= CONNECT_TIMEOUT) {
      connecting = false;
      if (sta_retry_count < MAX_RETRIES) {
        sta_retry_count++;
        retryPending = true;
        lastRetryTime = millis();
        sta_status = "Retrying...";
        sta_error = "Retry " + String(sta_retry_count) + "/" + String(MAX_RETRIES) + " in " + String(RETRY_INTERVAL / 1000) + "s";
        WiFi.disconnect();
        LOG_WARN("STA timeout; retry %d/%d scheduled in %lu ms", sta_retry_count, MAX_RETRIES, RETRY_INTERVAL);
      } else {
        retryPending = false;
        sta_status = "Disconnected";
        sta_error = "Max retries exceeded";
        WiFi.disconnect();
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
        connecting = false;
        sta_status = "Disconnected";
        sta_error = "No saved credentials";
        LOG_ERROR("No saved credentials for retry");
      }
    }
  }

  // Optional periodic update check (e.g., once per day)
  static unsigned long lastUpdateCheck = 0;
  if (WiFi.status() == WL_CONNECTED && millis() - lastUpdateCheck > 86400000UL) {
    checkAndUpdate();
    lastUpdateCheck = millis();
  }

  delay(1);
}
