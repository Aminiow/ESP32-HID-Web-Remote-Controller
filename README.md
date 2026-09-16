# ESP32-HID-Web-Remote-Controller — V10.0.0

**ESP32‑S3 Wi‑Fi → USB HID bridge with web‑based mouse/keyboard control, captive portal, STA/AP mode, auto‑channel selection, hidden SSID, secure over‑the‑air (OTA) firmware updates with three‑tier TLS verification, consumer controls (media keys), gyro mouse support, mDNS, Wi‑Fi power management, idle sleep, and SHA‑256 verified firmware uploads.**

Control your computer or TV wirelessly from your phone or tablet — settings survive power cycles.

[![GitHub release](https://img.shields.io/github/v/release/Aminiow/ESP32-HID-Web-Remote-Controller)](https://github.com/Aminiow/ESP32-HID-Web-Remote-Controller/releases)
[![PlatformIO](https://img.shields.io/badge/PlatformIO-ESP32S3-orange)](https://platformio.org/)
[![Arduino Core](https://img.shields.io/badge/Arduino%20Core-2.x%20%7C%203.x-blue)](https://github.com/espressif/arduino-esp32)
[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)
[![Flash: 4MB+](https://img.shields.io/badge/Flash-4MB%20Min%20%7C%208MB%20Recommended-informational)]()
[![Author](https://img.shields.io/badge/Author-Human-blue?style=flat-square)]()
[![Written by](https://img.shields.io/badge/Written%20by-Human%20%2B%20AI-blueviolet?style=flat-square)]()
[![Tested by](https://img.shields.io/badge/Tested%20by-Human-success?style=flat-square)]()

---

## 🌟 Features

- 🖱 **Mouse** – move, click (left/right/middle), double‑click, press/release, scroll wheel.
- ⌨ **Keyboard** – type text, send key taps (including special keys), press & hold modifiers (Ctrl, Alt, Shift, Win).
- 🔧 **Sticky modifiers** – tap to toggle Ctrl, Alt, Shift, Win (visual feedback on web UI).
- 💾 **Persistent settings** – sensitivity, repeat interval, legacy mode, boot protocol, gyro enable, TX power, power save, NTP servers, and mDNS hostname are stored in NVS and restored after power‑cycle.
- 📶 **Wi‑Fi Access Point** – creates its own network with **auto‑channel selection** (scans for the least crowded channel).
- 🔒 **Hidden SSID** – the AP name (`ESP32-HID`) is hidden by default for privacy.
- 🔗 **STA (Client) Mode** – can simultaneously connect to an existing Wi‑Fi network. Credentials are saved and auto‑reconnect with **exponential backoff** (5s → 10s → 20s → 30s).
- 📡 **Wi‑Fi scanning** – scan for networks and connect via the web interface (supports WPA3 and hidden networks).
- 🔁 **Automatic retry with backoff** – if STA connection fails, retries up to 3 times with an exponentially increasing delay.
- 🔗 **Captive Portal** – any DNS request resolves to the ESP32; any HTTP request redirects to the web UI (HTTP 302).
- 📱 **Responsive Web Interface** – works on phones, tablets, and desktops; touch‑friendly with a mouse pad and deadzone protection.
- 📊 **On‑board logging** – ring buffer of 200 entries; view logs in the web UI or fetch raw JSON at `/logs`.
- 🎛 **Adjustable Settings** – sensitivity, repeat interval, legacy mode, boot protocol, TX power, power save, NTP servers, mDNS name.
- ⚡ **USB HID** – uses TinyUSB to emulate a standard USB mouse, keyboard, and consumer control device.
- 🔄 **Robust USB enumeration** – `USB.begin()` is called first for reliable detection; custom descriptors identify the device.
- 🛡 **JSON escaping** – all API responses are properly JSON‑escaped to prevent injection.
- 🚀 **Over‑the‑Air (OTA) Firmware Updates** – three‑tier security: **Root CA** → **certificate fingerprint** → **insecure** (user‑confirmed).
- 🔐 **SHA‑256 Verified Uploads** – manual uploads can be verified against a user‑supplied hash (from `version.txt`).
- 📡 **Live progress via SSE** – Server‑Sent Events stream OTA logs and a real‑time progress bar to the browser.
- 🎛️ **Consumer Controls (Media Keys)** – volume, mute, channel, power, input menu/select, AV List, Back, Exit, Home.
- 📱 **Gyro Mouse Support** – use phone orientation sensors to move the cursor (with proper iOS permission handling).
- 🌐 **mDNS** – access the web interface at a configurable `<name>.local` address.
- 🔋 **Wi‑Fi Power Management** – adjustable TX power (0–20 dBm) and modem sleep.
- 😴 **Idle Sleep** – reduces CPU to 80 MHz and enables max modem sleep when idle for 60 seconds.
- 📋 **Dynamic Version Display** – web UI shows current firmware version fetched from `/update_status`.
- 🔒 **WebSocket hardening** – frame size limit (256 bytes), per‑client rate limiting (20 frames per 100 ms), ASCII validation.
- 🧹 **Comprehensive cleanup** – all held keys and mouse buttons are released on WiFi disconnect, page unload, or blur.

---

## 🆕 What's New in v10

### 🛠 Critical bug fixes

- **Compile error fixed** — the original v10 draft had a nested duplicate `void handleEvents() {` inside itself and a duplicate `#define MAX_RETRIES` clashing with `const int MAX_RETRIES`. Both removed; file compiles clean.
- **Firmware update hash bypass fixed** — the previous `handleUpload` implementation could skip SHA‑256 verification if the remote version file was unreachable. v10 requires an explicit hash from the user (form field) or logs a clear warning and proceeds knowingly.
- **RTC sanity check rewritten** — replaced the meaningless `now > 8 * 3600 * 2` (16 hours after 1970) with `isRtcSynced()`, which checks the time is between 2020 and 2100. Uses `long long` comparison to avoid 32‑bit `time_t` overflow. Applied consistently to `fetchVersionInfo`, `tryDownloadWithFingerprint`, and `sendTimeUpdate`.
- **JSON escape buffer overflow fixed** — `jsonEscape()` now reserves `length * 6 + 16` bytes (worst case: 6‑char `\uXXXX` sequences) instead of `length + 8`.
- **Log buffer truncation detected** — `addLog()` now checks the `vsnprintf` return value and logs a warning if a message is truncated (bumped `LOG_MSG_SIZE` to 512).
- **WebSocket frame validation** — added size check (`MAX_WS_FRAME = 256`), per‑client rate limiting, and ASCII validation. Malformed frames are rejected with a log line instead of corrupting the parser.
- **SSE client memory safety** — `handleEvents()` uses `new (std::nothrow)` and wraps the previous client's teardown in `try/catch` to prevent crashes during client handoff.
- **Stuck keys on disconnect fixed** — `disconnectSTA()` now calls `releaseAllModifiers()`, `releaseMouseButtons()`, and `releaseHeldKeys()` before dropping the link, preventing stuck modifiers on the host.
- **Duplicate `let` declarations removed** — the frontend had three JS syntax errors (`wsReconnectTimer`, `sensSaveTimer`/`repeatSaveTimer`, `realtimeChangeTimer` declared twice). Page now loads without `ReferenceError`.
- **`update_html_tpl` syntax error fixed** — the `window.onload` block in the update page had a missing closing brace, which broke the entire script. Now correctly scoped.
- **`sta_html` `sendHTTP` reference error fixed** — moved the `sendHTTP` definition to the top of the script block; the immediate `updateStatus()` call now finds it.

### ⚡ Reliability improvements

- **Wi‑Fi retry with exponential backoff** — retries now use `RETRY_BACKOFF[] = {5000, 10000, 20000, 30000}` ms instead of a fixed interval.
- **Forward declarations added** — `checkUpdateTask`, `otaSecureTask`, and `startSecureOta` are forward‑declared at the top, so the file compiles cleanly under PlatformIO and pure `.cpp` builds (not just Arduino IDE auto‑prototyping).
- **Removed dead code** — unused `String fullHost = mdnsHostname;` removed from `setup()`; the redundant RTC check inside `fetchVersionInfo` removed (now uses `isRtcSynced()` once).

### 🖥 Frontend robustness

- **WebSocket reconnect with backoff and limit** — reconnects exponentially (2s → 3s → 4.5s → ... up to 30s) and stops after 30 attempts with a clear error message.
- **`sendHTTP` with timeout** — every `fetch()` call now has a 10–15 second timeout and rejects with a descriptive error instead of hanging.
- **Realtime input debounced & race‑free** — the dual‑handler race condition (which sent keys out of order) is fixed. Single `input` handler with 100 ms debounce; insertions, deletions, and paste events are correctly routed.
- **Settings save debounced to 800 ms** — dragging a slider no longer triggers dozens of NVS writes; the value is written once the user stops moving.
- **Mouse pad deadzone** — a 10‑pixel deadzone prevents jitter when tapping the pad on touchscreens. Border highlights blue during drag for visual feedback.
- **`beforeunload` cleanup with `keepalive`** — the `/reset_modifiers` request survives page unload thanks to `fetch(..., {keepalive: true})`, preventing stuck keys when closing the browser.
- **`testAll()` per‑test error handling** — one failing endpoint no longer aborts the entire test sequence; each test logs pass/fail independently.
- **Consumer key whitelist** — client‑side validation of consumer keys prevents accidentally sending unknown usage codes.
- **Gyro permission UX improved** — denied permissions uncheck the box instead of showing a blocking alert.

### 📡 Configuration

- **Configurable NTP servers (3 slots)** — set via `/set_ntp` from the update page; persisted in NVS.
- **Configurable mDNS hostname and domain** — set via `/set_mdns`; changing the name restarts the mDNS responder without a reboot.
- **New `/get_settings` endpoint** — returns all persisted settings in a single JSON blob, so the web UI syncs every slider and checkbox on page load.

### 📺 Consumer / TV controls

- **New buttons** — AV List, Back, Exit, Home added to the remote‑control card and the media/TV card.
- **New HID usages** — `CONSUMER_AV_LIST` (0x183), `CONSUMER_AC_BACK` (0x224), `CONSUMER_AC_EXIT` (0x204), `CONSUMER_HOME` (0x223).
- **Media/TV card reorganised** — grouped into rows: playback, navigation, volume/channel, power/home/av, input/back/exit.

### 🐛 Fixes summary

| Issue | Status in v10 |
|---|---|
| Nested `handleEvents()` compile error | ✅ Fixed |
| Duplicate `#define MAX_RETRIES` | ✅ Fixed |
| Manual upload skipped hash verification | ✅ Fixed (form‑supplied hash) |
| RTC check was `now > 57600` (nonsense) | ✅ Fixed (`isRtcSynced()` 2020–2100) |
| `jsonEscape` buffer overflow potential | ✅ Fixed (6x reserve) |
| WebSocket frame validation missing | ✅ Fixed (size + rate + ASCII) |
| SSE client allocation crash | ✅ Fixed (`std::nothrow` + `try/catch`) |
| Stuck modifiers on STA disconnect | ✅ Fixed (release helpers) |
| Duplicate `let` in frontend JS | ✅ Fixed (declarations hoisted) |
| `update_html_tpl` missing brace | ✅ Fixed |
| `sta_html` `sendHTTP` not defined at call time | ✅ Fixed (moved to top) |
| Wi‑Fi retry used fixed 5s interval | ✅ Fixed (exponential backoff) |
| `String fullHost` unused | ✅ Removed |
| Realtime input race condition | ✅ Fixed (single debounced handler) |
| Settings slider triggered many NVS writes | ✅ Fixed (800 ms debounce) |
| Pad click fired on tiny drags | ✅ Fixed (10 px deadzone) |
| Stuck keys on page close | ✅ Fixed (`keepalive` reset) |
| Gyro permission denial was silent | ✅ Fixed (unchecks + logs) |
| `testAll()` aborted on first failure | ✅ Fixed (per‑test try/catch) |

---

## 🧰 Hardware Requirements

### ✅ Boards that work well

| Board | Flash | PSRAM | Notes |
|---|---|---|---|
| **ESP32‑S3‑DevKitC‑1 (N8R2)** | 8 MB | 2 MB | **Recommended** – used for development |
| **ESP32‑S3‑DevKitC‑1 (N8R8)** | 8 MB | 8 MB | Same as above with more PSRAM |
| **ESP32‑S3‑DevKitC‑1 (N16R8)** | 16 MB | 8 MB | Best headroom for OTA + filesystem |
| **ESP32‑S3‑Mini‑1** | Varies | Varies | Compact, same USB‑OTG support |
| **ESP32‑S3‑Zero** | 4 MB | – | Works but tight on flash |
| **ESP32‑S2 boards** | Varies | – | Works; S2 has native USB‑OTG |
| **ESP32‑P4** | Varies | – | Works; new chip with USB 2.0 OTG |

### ❌ Boards that will NOT work

| Board | Why |
|---|---|
| **ESP32 (original)** | No native USB‑OTG peripheral |
| **ESP32‑C3** | USB Serial/JTAG only; no USB device‑mode OTG |
| **ESP32‑C6** | USB Serial/JTAG only; no USB device‑mode OTG |
| **ESP32‑H2** | No USB peripheral at all |

> The firmware requires the **USB‑OTG (TinyUSB)** peripheral, which only exists on ESP32‑S2 / S3 / P4.

### Minimum setup
- **ESP32‑S3** (or S2/P4) dev board
- **USB‑C cable** – data‑capable, not charge‑only
- **4 MB flash minimum**, **8 MB recommended**

---

## 📦 Software & Libraries

Written for the **Arduino framework** (compatible with Arduino IDE and PlatformIO).

### Required libraries

| Library | Source | Purpose |
|---|---|---|
| `WiFi` | built‑in | Wi‑Fi stack |
| `WebServer` | built‑in | HTTP server on port 80 |
| `DNSServer` | built‑in | Captive portal DNS |
| `WebSocketsServer` | [arduinoWebSockets](https://github.com/Links2004/arduinoWebSockets) | Real‑time mouse movement |
| `USB`, `USBHIDMouse`, `USBHIDKeyboard`, `USBHIDConsumerControl` | built‑in | TinyUSB HID |
| `Preferences` | built‑in | NVS settings storage |
| `HTTPClient` | built‑in | OTA downloads |
| `Update` | built‑in | Flash writing |
| `WiFiClientSecure` | built‑in | TLS |
| `mbedtls/sha256` | built‑in | Hashing |
| `ESPmDNS` | built‑in | `<name>.local` resolution |
| `esp_wifi`, `esp_sleep` | built‑in | Power management |
| `esp_partition`, `esp_ota_ops` | built‑in | Partition queries |

### Supported Arduino ESP32 Core versions

| Core | Status | Notes |
|---|---|---|
| **2.x** | ✅ Supported | Uses `setFingerprint()` during handshake |
| **3.x** | ✅ Supported | Uses `getFingerprintSHA256()` after handshake |
| **< 2.x** | ❌ Not supported | Missing USB HID APIs |

A compile‑time shim (`#if ESP_ARDUINO_VERSION_MAJOR >= 3`) picks the right API automatically.

---

## 🚀 Installation & Flashing

### 1. Clone the repository

```bash
git clone https://github.com/Aminiow/ESP32-HID-Web-Remote-Controller.git
cd ESP32-HID-Web-Remote-Controller
```

### 2. Open in Arduino IDE (or PlatformIO)

- **Arduino IDE**: open `ESP32-HID-Web-Remote-Controller.ino`
- **PlatformIO**: open the project folder

### 3. Board settings (Arduino IDE)

| Setting | Value |
|---|---|
| Board | **ESP32S3 Dev Module** |
| **USB Mode** | **USB‑OTG (TinyUSB)** |
| Upload Mode | UART0 / Hardware CDC |
| **USB CDC On Boot** | **Disabled** ← critical for HID |
| USB Firmware MSC On Boot | Enabled (S2/S3 only) |
| **Flash Size** | **8 MB / 64 Mb** *(or 16 MB for N16 boards)* |
| **Partition Scheme** | **8M with spiffs (3MB APP/1.5MB SPIFFS)** ← see next section |
| PSRAM | OPI PSRAM (if your board has it) |
| Upload Speed | 921600 |
| Erase All Before Sketch Upload | Enabled (optional, recommended on first flash) |

### 4. Upload

- Connect the ESP32‑S3 via the **USB‑C port that supports OTG** (usually labelled `USB` or `USB‑OTG`, not `UART`).
- Press **Upload**.
- If needed, hold **BOOT** while plugging in to enter download mode.

After flashing, the ESP32 creates Wi‑Fi network **`ESP32-HID`** (password `12345678`). The SSID is **hidden** — add it manually.

---

## 🧩 Partition Schemes Explained

The code uses **two app partitions** (for OTA) plus NVS. The **filesystem partition is unused** — the firmware never mounts SPIFFS/LittleFS/FATFS — but any scheme you pick will still reserve one.

### What the firmware actually needs

| Requirement | Size | Why |
|---|---|---|
| `nvs` | ≥ 20 KB | `Preferences` storage |
| `otadata` | 8 KB | OTA boot state |
| `app0` (`ota_0`) | ≥ 1.5 MB *(1.4 MB absolute floor)* | Active firmware |
| `app1` (`ota_1`) | **same size as app0** | OTA target |
| Filesystem | optional | Not used by the code |

Current firmware binary size: **~1.29 MB**. Add ~20–30% headroom for growth → **1.5 MB is the safe target per app partition**.

### Recommended schemes by flash size

| Flash | Recommended scheme | App per slot | Filesystem | Notes |
|---|---|---|---|---|
| **4 MB** | `Minimal SPIFFS (1.9MB APP with OTA/128KB SPIFFS)` | 1.875 MB ×2 | 128 KB | Small but works |
| **4 MB** | `No FS 4MB (2MB APP x2)` | 2.0 MB ×2 | – | Best 4MB option |
| **8 MB** | **`8M with spiffs (3MB APP/1.5MB SPIFFS)`** | 3.0 MB ×2 | 1.5 MB | **Firmware default** |
| **16 MB** | `16M Flash (3MB APP/9.9MB FATFS)` | 3.0 MB ×2 | 9.9 MB | Comfortable |
| **32 MB** | `32M Flash (4.8MB APP/22MB FATFS)` | 4.8 MB ×2 | 22 MB | Overkill but fine |

### Full compatibility matrix

| Scheme | Works? | App per slot | Notes |
|---|---|---|---|
| Default 4MB with spiffs | ⚠️ Very tight | 1.25 MB ×2 | ~20 KB headroom at 98% |
| Default 4MB with ffat | ⚠️ Very tight | 1.25 MB ×2 | Same |
| **8M with spiffs (3MB APP/1.5MB SPIFFS)** | ✅ **Recommended** | 3.0 MB ×2 | 41% usage |
| Minimal (1.3MB APP/700KB SPIFFS) | ❌ | single app | No `ota_1` |
| No FS 4MB (2MB APP x2) | ✅ | 2.0 MB ×2 | No FS — fine |
| No OTA (2MB APP/2MB SPIFFS) | ❌ | single app | No OTA |
| No OTA (1MB APP/3MB SPIFFS) | ❌ | single app | No OTA |
| No OTA (2MB APP/2MB FATFS) | ❌ | single app | No OTA |
| No OTA (1MB APP/3MB FATFS) | ❌ | single app | No OTA |
| Huge APP (3MB No OTA/1MB SPIFFS) | ❌ | single app | No OTA |
| Minimal SPIFFS (1.9MB APP with OTA/128KB SPIFFS) | ✅ | 1.875 MB ×2 | Good for 4 MB |
| 16M Flash (2MB APP/12.5MB FATFS) | ✅ | 2.0 MB ×2 | |
| 16M Flash (3MB APP/9.9MB FATFS) | ✅ | 3.0 MB ×2 | |
| RainMaker 4MB | ⚠️ Very tight | 1.25 MB ×2 | Same as Default 4MB |
| RainMaker 4MB No OTA | ❌ | single app | No OTA |
| RainMaker 8MB | ✅ | ~2.5–3 MB ×2 | |
| 32M Flash (4.8MB APP/22MB FATFS) | ✅ | 4.8 MB ×2 | |
| 32M Flash (4.8MB APP/22MB LittleFS) | ✅ | 4.8 MB ×2 | |
| 32M Flash (13MB APP/6.75MB SPIFFS) | ✅ | 13 MB ×2 | |
| ESP SR 16M (3MB APP/7MB SPIFFS/2.9MB MODEL) | ✅ | 3.0 MB ×2 | |
| Zigbee ZCZR 4MB with spiffs | ❌ | reduced by Zigbee | No room |
| Zigbee ZCZR 8MB with spiffs | ⚠️ Verify | reduced by Zigbee | Test it |
| Custom | depends | must have `ota_0` + `ota_1` ≥ 1.5 MB each | |

> **Rule of thumb:** if the scheme name contains **"No OTA"**, the firmware boots, but any `/trigger_update`, `/start_ota_*`, or auto‑update call will fail with `No OTA partition found`.

---

## 💾 Flash Size Requirements

| Flash | Verdict |
|---|---|
| **2 MB** | ❌ Cannot fit two 1.5 MB app partitions |
| **4 MB** | ⚠️ **Absolute minimum** — works today at ~98% usage |
| **8 MB** | ✅ **Recommended floor** — ~41% usage, real OTA headroom |
| **16 MB** | ✅ Comfortable |
| **32 MB** | ✅ Overkill but fully supported |

### Space used by the current firmware

Measured on a real **ESP32‑S3 DevKit N8R2** build:

| Metric | Value | Notes |
|---|---|---|
| Program (flash) | **~1.30 MB** | Compiled `.bin` |
| Global variables (RAM) | **~127 KB** | ~38% of 320 KB DRAM |
| Sketch + data | ~1.31 MB | Fits in 1.5 MB partition with ~200 KB headroom |

### OTA headroom needed

Each OTA update writes the incoming `.bin` to the **other** app partition:

```
app0 (active firmware)   ← currently running
app1 (OTA target)        ← new firmware written here, then reboot
```

So you need:

```
flash  ≥  2 × max(firmware size)  +  NVS  +  otadata  +  filesystem
```

With a 1.30 MB firmware, that means **≥ 2.6 MB for the two apps alone**. On 4 MB flash this leaves barely any room for filesystem or growth; on 8 MB there's plenty.

---

## 📱 Usage

### Quick Start

1. **Add the Wi‑Fi network** `ESP32-HID` (password `12345678`) manually — it won't appear in scans because it's hidden.
2. **Open any browser** — the captive portal redirects to `http://192.168.4.1/`, or use `esp32-hid.local`.
3. **Plug the ESP32** into your computer/TV via USB‑C.
4. **Use the web UI** to control the cursor, type, send media commands.

### Web Interface

- **Mouse Pad** – drag to move cursor; tap for left‑click (with 10 px deadzone)
- **Arrow keys** – hold for repeated movement (interval adjustable)
- **Mouse buttons** – left/right/middle, double‑click, press/release (visual feedback)
- **Keyboard grid** – full QWERTY layout with sticky modifiers
- **Text input** – type arbitrary ASCII text
- **Real‑time input** – live typing with backspace support (debounced 100 ms)
- **Settings** – sensitivity, repeat, legacy, boot protocol, TX power, power save (debounced 800 ms autosave)
- **Gyro Mouse** – phone orientation → cursor movement (with iOS permission handling)
- **Media / TV** – consumer control buttons
- **Logs** – client‑side log panel (session‑stored) + `/logs` JSON endpoint
- **Firmware Update** – dedicated `/update` page (see below)

### Wi‑Fi STA (Client) Mode

1. Click the **📶 WiFi** button (or go to `/sta`)
2. The page auto‑scans for Wi‑Fi networks
3. Click a network to autofill SSID and BSSID
4. Enter password (tick **Hidden network** if applicable)
5. Click **Connect** — status updates live every 2 seconds

The ESP32 remembers credentials and retries up to 3 times per boot with exponential backoff.

### Root page (`/`)

- **📶 WiFi** button → `/sta` settings page
- **⬆ Update** button → `/update` firmware page
- Wi‑Fi status bar refreshes every 3 seconds
- All sliders, checkboxes, and toggles sync from NVS on load via `/get_settings`

---

## 🔄 Firmware Update Methods

The firmware supports **four** update paths:

### 1. Manual upload (web UI)

`/update` → file picker → optional SHA‑256 hash field → **Upload & Update**

- Multipart POST to `/upload`
- Streams firmware chunks directly into `Update.write()`
- SHA‑256 computed incrementally as bytes arrive
- If a 64‑char hex hash is provided in the form, the computed hash is compared; on mismatch, the update is aborted
- If no hash is provided, a warning is logged and the update proceeds (still flashes safely — the binary is assumed from a trusted local source)
- On success: reboot

**Best for:** offline recovery, testing builds, bypassing network issues.

**No network calls are made during the upload** — this means the server stays responsive and the update works even when STA is disconnected.

### 2. Secure OTA (three‑tier)

`/update` → **Start Secure OTA** (`/start_ota_secure`)

The flow tries three tiers in order:

| Tier | Method | What it verifies |
|---|---|---|
| **1** | `WiFiClientSecure::setCACert(rootCACertificate)` | Full X.509 chain to **Secigo Root E46** |
| **2** | `setFingerprintSHA256()` *(core 3.x)* / `setFingerprint()` *(core 2.x)* | Leaf cert SHA‑256 of `raw.githubusercontent.com` |
| **3** | `setInsecure()` — **only after user confirmation** | Nothing — user explicitly trusts the network |

If tier 1 or 2 succeeds, the download proceeds automatically. If both fail, the SSE stream emits `ota_state: insecure_offer`, which reveals the **"Retry Insecurely"** button on the page. Clicking it starts a new task in `OTA_INSECURE` mode.

**Best for:** normal use — the CA tier usually succeeds, and the fingerprint tier catches the rare CA rotation.

### 3. URL / auto update

`/update` → **Check for Update** → **Trigger URL Update**

- `checkAndUpdate()` fetches `version.txt` from the configured URL and compares against `FW_VERSION_STR`
- If a newer version is available, the **Trigger URL Update** button calls `startSecureOta(false)` — same three‑tier flow as above

**Best for:** keeping devices up to date without manual uploads.

### 4. Automatic check

On boot (if STA connected) and once per 24 h in `loop()`, `checkAndUpdate()` runs in a FreeRTOS task. It only fetches metadata — it does **not** flash automatically.

**Best for:** notifying users that a new firmware is available via the web UI.

### Live progress (SSE)

During any secure OTA, the browser receives:

- `log` events — one per OTA log line
- `progress` events — `{written, total, pct}` every 500 ms
- `ota_state` events — `idle`, `running`, `insecure_offer`, `rebooting`, `failed`
- `time` events — uptime + UTC time every 1 s

The progress bar on the `/update` page updates live from the `progress` events.

### Update URL configuration

Two URLs, both editable via the web UI:

- **Version URL** — plain text: `version\nsha256_hash` (two lines)
- **Binary URL** — points to the firmware `.bin`

Defaults:
```
version.txt: https://github.com/Aminiow/ESP32-HID-Web-Remote-Controller/raw/refs/heads/main/version.txt
firmware.bin: https://github.com/Aminiow/ESP32-HID-Web-Remote-Controller/raw/refs/heads/main/firmware.bin
```

> **Note:** GitHub redirects `github.com/.../raw/...` → `raw.githubusercontent.com`. The fingerprint tier uses the **final** peer cert, so `GITHUB_LEAF_FP` must match `raw.githubusercontent.com`'s leaf, not `github.com`'s. The default fingerprint is updated for the current leaf.

---

## 🔐 Security Model

| Layer | Mechanism | Strength |
|---|---|---|
| Transport | TLS 1.2+ via mbedTLS | Standard |
| **Tier 1** | Root CA chain validation (Secigo E46) | Highest — full chain |
| **Tier 2** | Leaf cert SHA‑256 fingerprint | Strong — pins exact cert |
| **Tier 3** | Insecure (user‑confirmed) | Trust‑based |
| Firmware integrity | SHA‑256 of `.bin` body | Prevents corruption |
| Manual upload | SHA‑256 vs user‑supplied hash (optional) | Explicit |
| OTA URL whitelist | `github.com/Aminiow/` + `raw.githubusercontent.com/Aminiow/` | Prevents redirect hijacks |
| RTC sanity guard | `isRtcSynced()` (2020 ≤ now ≤ 2100) | Blocks TLS on garbage time |
| WebSocket frame validation | Size limit + rate limit + ASCII check | Prevents parser abuse |
| SSE memory safety | `std::nothrow` + `try/catch` teardown | No crashes on client swap |

### Fingerprint lifecycle

`raw.githubusercontent.com` is fronted by **Fastly**. The leaf cert rotates roughly every **90 days**.

When it rotates:
- Tier 1 (Root CA) still works — CA certs last years.
- Tier 2 (fingerprint) fails — the pinned hash no longer matches.
- Flow falls through to **tier 3 offer** — user clicks "Retry Insecurely".

To refresh the pin, run:

**PowerShell:**
```powershell
$c = New-Object System.Net.Sockets.TcpClient("raw.githubusercontent.com",443)
$s = New-Object System.Net.Security.SslStream($c.GetStream(),$false,{$true})
$s.AuthenticateAsClient("raw.githubusercontent.com")
$cert = $s.RemoteCertificate
$sha = [System.Security.Cryptography.SHA256]::Create().ComputeHash($cert.GetRawCertData())
($sha | ForEach-Object { $_.ToString("x2") }) -join ""
```

**Linux/macOS:**
```bash
openssl s_client -connect raw.githubusercontent.com:443 -servername raw.githubusercontent.com </dev/null 2>/dev/null \
  | openssl x509 -noout -fingerprint -sha256
```

Paste the result into `#define GITHUB_LEAF_FP "..."`.

### Firmware URL whitelist

The `isValidUpdateUrl()` check rejects any firmware URL that isn't under:

```
https://github.com/Aminiow/ESP32-HID...
https://raw.githubusercontent.com/Aminiow/...
```

This prevents an attacker from pointing the OTA URL at a malicious server even if they gain control of the settings endpoint. Both HTTP 3xx redirects and the initial URL are validated.

---

## ⚙️ Default Configuration

### Firmware defaults

| Setting | Default | Storage |
|---|---|---|
| Firmware version | `10.0.0` | `#define FW_VERSION_STR` |
| AP SSID | `ESP32-HID` | `ap_ssid` |
| AP password | `12345678` | `ap_password` |
| AP hidden | `true` | `WiFi.softAP(..., true)` |
| Sensitivity | `2.0` | NVS `settings/sens` |
| Repeat interval | `100 ms` | NVS `settings/repeat` |
| Legacy mode | `false` | NVS `settings/legacy` |
| Boot protocol | `false` | NVS `settings/bootproto` |
| Gyro enabled | `false` | NVS `settings/gyro` |
| TX power | `20 dBm` | NVS `settings/txpwr` |
| Power save | `true` | NVS `settings/psave` |
| Update version URL | GitHub `main/version.txt` | NVS `updates/verUrl` |
| Update binary URL | GitHub `main/firmware.bin` | NVS `updates/binUrl` |
| NTP server 1 | `pool.ntp.org` | NVS `settings/ntp1` |
| NTP server 2 | `time.google.com` | NVS `settings/ntp2` |
| NTP server 3 | `time.cloudflare.com` | NVS `settings/ntp3` |
| mDNS hostname | `esp32-hid` | NVS `settings/mdnsName` |
| mDNS domain | `local` | NVS `settings/mdnsDom` |
| Consumer volume step | HID usage `0xE9`/`0xEA` | `#define` |
| Max log entries | `200` | `#define` |
| Max log message | `512 bytes` | `#define LOG_MSG_SIZE` |
| Idle sleep threshold | `60 s` | hardcoded |
| STA retry backoff | `5s → 10s → 20s → 30s` | `RETRY_BACKOFF[]` |
| STA connect timeout | `15 s` | `CONNECT_TIMEOUT` |
| Max STA retries | `3` | `MAX_RETRIES` |
| WebSocket max frame | `256 bytes` | `MAX_WS_FRAME` |
| WebSocket rate window | `100 ms` | `WS_RATE_WINDOW` |
| WebSocket frames per window | `20` | `MAX_WS_FRAMES_PER_WINDOW` |
| OTA CA cert | Secigo Public Server Auth Root E46 | hardcoded PEM |
| GitHub leaf fingerprint | `71f1077d...cf84` | `#define` |

### NVS namespaces

| Namespace | Keys |
|---|---|
| `settings` | `sens`, `repeat`, `legacy`, `bootproto`, `gyro`, `txpwr`, `psave`, `ntp1`, `ntp2`, `ntp3`, `mdnsName`, `mdnsDom` |
| `wifi` | `ssid`, `pass`, `hidden`, `bssid`, `chan`, `lastChan` |
| `updates` | `verUrl`, `binUrl` |

> **Warning:** Switching partition scheme erases NVS. Re‑configure Wi‑Fi, NTP, and mDNS after the first boot on a new layout.

---

## 📊 Language Composition

This is a **single‑file Arduino sketch** (`.ino`) that embeds HTML, CSS, and JavaScript as `PROGMEM` string literals. Estimated composition by source bytes:

| Language | % | What it's used for |
|---|---|---|
| **C++** | **~52 %** | Core firmware logic, HID control, Wi‑Fi, OTA, TLS, NVS |
| **JavaScript** | **~18 %** | Web UI logic — mouse/keyboard events, SSE client, OTA progress |
| **HTML** | **~15 %** | Page structure — index, STA config, update page |
| **CSS** | **~13 %** | Dark theme, responsive layout, progress bar |
| **JSON / other** | **~2 %** | API responses, comments, markdown |

![C++](https://img.shields.io/badge/C%2B%2B-52%25-blue?style=flat-square&logo=cplusplus)
![JavaScript](https://img.shields.io/badge/JavaScript-18%25-yellow?style=flat-square&logo=javascript)
![HTML](https://img.shields.io/badge/HTML-15%25-orange?style=flat-square&logo=html5)
![CSS](https://img.shields.io/badge/CSS-13%25-blueviolet?style=flat-square&logo=css3)
![Other](https://img.shields.io/badge/Other-2%25-lightgrey?style=flat-square)

> Percentages are byte‑count estimates of the `.ino` source file, not counting comments or blank lines separately.

---

## 📸 Screenshots

> **Note:** Placeholder images — replace URLs with your own uploads.

### 📱 Phone view (touch layout)

<details>
<summary><b>Tap to expand — 6 phone screenshots</b></summary>

#### 1. Home / mouse pad

![Phone - Home](https://placehold.co/400x800/1e1e1e/5b9aff?text=Phone%3A+Home+%2F+Mouse+Pad)

*Top card with Wi‑Fi status, sensitivity slider, and the large touch pad. Drag to move, tap to left‑click.*

#### 2. Keyboard grid

![Phone - Keyboard](https://placehold.co/400x800/1e1e1e/5b9aff?text=Phone%3A+Keyboard)

*Full QWERTY with sticky modifiers and text input field.*

#### 3. Media controls

![Phone - Media](https://placehold.co/400x800/1e1e1e/5b9aff?text=Phone%3A+Media+Controls)

*Volume, mute, channel, power, and input selection buttons.*

#### 4. Wi‑Fi settings (`/sta`)

![Phone - WiFi](https://placehold.co/400x800/1e1e1e/5b9aff?text=Phone%3A+WiFi+Settings)

*Network scan, SSID/password fields, hidden network toggle, connect/disconnect/forget buttons.*

#### 5. Update page (`/update`)

![Phone - Update](https://placehold.co/400x800/1e1e1e/5b9aff?text=Phone%3A+Update)

*Secure OTA, auto‑update URLs, manual upload with optional hash field, live progress bar, log panel.*

#### 6. OTA in progress

![Phone - OTA Progress](https://placehold.co/400x800/1e1e1e/5b9aff?text=Phone%3A+OTA+Running)

*Progress bar + SSE log stream during a live secure OTA.*

</details>

### 🖥️ PC view (desktop layout)

<details>
<summary><b>Tap to expand — 4 PC screenshots</b></summary>

#### 1. Home — full two‑column layout

![PC - Home](https://placehold.co/1200x800/1e1e1e/5b9aff?text=PC%3A+Home+Full)

*Mouse pad on the left, keyboard on the right, media controls below.*

#### 2. Update page — full view

![PC - Update](https://placehold.co/1200x800/1e1e1e/5b9aff?text=PC%3A+Update+Page)

*Secure OTA, URL config, manual upload with hash field, live progress, log panel.*

#### 3. OTA progress with SSE log

![PC - OTA Progress](https://placehold.co/1200x800/1e1e1e/5b9aff?text=PC%3A+OTA+Live)

*Progress bar at 47%, log lines streaming in.*

#### 4. Raw logs (`/logs`)

![PC - Logs](https://placehold.co/1200x800/1e1e1e/5b9aff?text=PC%3A+Raw+Logs+JSON)

*JSON output of the in‑RAM ring buffer.*

</details>

---

## 🛠 Troubleshooting

| Symptom | Possible cause / solution |
|---|---|
| **TV/computer does not recognise USB HID** | 1. Ensure **USB CDC On Boot** is **Disabled**.<br>2. Use a data‑capable USB cable.<br>3. Power externally if USB can't supply enough current.<br>4. Verify the USB port supports OTG. |
| **Can't find Wi‑Fi AP** | SSID is **hidden**. Manually add `ESP32-HID` / `12345678`. |
| **Captive portal not redirecting** | Manually go to `http://192.168.4.1` or `esp32-hid.local`. |
| **Keyboard keys not sending** | Verify USB connection and that the host has focus on a text field. |
| **Stuck modifier keys after closing browser** | `beforeunload` handler should have cleaned up. If not, reload the page and click **🗑 Clear** then **📋 Logs** to see what happened. |
| **STA connection fails / retries** | Check SSID/password. Retries up to 3× with exponential backoff (5s → 10s → 20s → 30s). See `/logs`. |
| **Settings not saved** | Ensure NVS has enough space. Settings persist across power‑cycles. |
| **Wi‑Fi scan doesn't show networks** | Confirm range and antenna. Hidden networks may need manual BSSID entry. |
| **Firmware update fails** | Check URLs; if using a hash, ensure it matches; ensure STA is connected. |
| **"No OTA partition found"** | You're on a **"No OTA"** partition scheme. Switch to `8M with spiffs`. |
| **Sketch too big / 100% flash** | You're on 4 MB with `Default 4MB with spiffs`. Switch to `8M with spiffs` (you have 8 MB). |
| **Manual upload rejected with "Hash mismatch"** | The uploaded file doesn't match the hash you pasted. Either download a fresh `.bin` or clear the hash field to skip verification. |
| **Manual upload shows "No hash supplied — computed SHA‑256: ..."** | You left the hash field empty. This is fine — the firmware flashed successfully. Paste the hash next time for verification. |
| **OTA fails with "Fingerprint mismatch"** | GitHub rotated its leaf cert. Re‑capture `GITHUB_LEAF_FP` or use **Retry Insecurely**. |
| **OTA fails with "Root CA failed"** | NTP not synced, or CA rotation. Check `/logs` for `RTC not synced` — if so, connect STA and wait for NTP sync. |
| **Consumer controls not working** | Some hosts don't support USB HID consumer control. Try another device. |
| **Gyro mouse not working** | On iOS, grant motion permission when prompted. Check browser `deviceorientation` support. |
| **Web interface slow** | Disable power save; increase TX power; ensure not in idle sleep. |
| **Version not updating in UI** | Check `/update_status` returns valid JSON; UI polls on load. |
| **`configTime` compile error on core 3.x** | The firmware already passes 3 NTP servers max. If you add more, reduce to 3. |
| **`setFingerprint` compile error on core 3.x** | Use `getFingerprintSHA256()` post‑handshake. The `clientFingerprintHex()` helper handles this via `#if ESP_ARDUINO_VERSION_MAJOR >= 3`. |
| **`checkUpdateTask not declared` on PlatformIO** | Forward declarations are at the top of the file. If they got removed, re‑add `void checkUpdateTask(void*); void otaSecureTask(void*); void startSecureOta(bool);` after the `USBHIDConsumerControl ConsumerControl;` line. |
| **Duplicate `#define MAX_RETRIES` compile error** | Remove the `#define MAX_RETRIES 3` — only the `const int MAX_RETRIES = 3;` should remain. |

---

## 🔮 Coming in v11 (Roadmap)

The next major release focuses on UX polish, additional protocols, and further hardening.

### 🔐 Security

- **Cert pinning refresh via OTA** — allow the device to fetch updated `GITHUB_LEAF_FP` over a trusted channel.
- **Signed firmware images** — Ed25519 signature verification in addition to SHA‑256.
- **AP password rotation** — user‑configurable AP password with NVS persistence.
- **Optional HTTPS-only mode** — self-signed cert for the web UI, requiring the user to trust the cert once.

### 📶 Wi‑Fi

- **WPA3-only AP mode** — where supported.
- **Static IP configuration** for STA mode.
- **Multiple saved STA networks** — priority‑ordered list with automatic fallback.
- **Wi‑Fi hotspot fallback** — if STA fails, extend an existing network's range.

### 🖥 Web UI

- **Dark/light theme toggle** persisted in `localStorage`.
- **Config export/import** — download NVS blob as JSON, upload to restore.
- **Gesture editor** — user‑defined multi‑touch gestures mapped to actions.
- **On‑screen virtual gamepad** — D‑pad + shoulder buttons for gaming.

### 🔌 Protocols

- **BLE HID fallback** — on chips that support it, use Bluetooth HID when USB is unavailable.
- **MQTT bridge** — publish/subscribe for home automation (Home Assistant integration).
- **HTTP API schema** — OpenAPI/JSON schema for third‑party clients.

### 🛠 Firmware

- **Delta OTA updates** — only download the diff between versions (experimental).
- **Watchdog watchdog** — hardware WDT to recover from hangs.
- **Per‑host profiles** — remember sensitivity/repeat separately for each USB host.
- **Reduced memory footprint** — target ≤ 100 KB DRAM usage.

> **Note:** v11 is planned, not yet in development. APIs in v10 are stable and will remain compatible.

---

**Authorship:** Human · **Written by:** Human + AI · **Tested by:** Human

---

## 📜 License

MIT — see [LICENSE](LICENSE).

---

## 🤝 Contributing

Contributions welcome! Open an issue or PR on [GitHub](https://github.com/Aminiow/ESP32-HID-Web-Remote-Controller).

---

## 🙏 Acknowledgements

- [Espressif Systems](https://www.espressif.com/) — ESP32‑S3 and Arduino core
- [TinyUSB](https://github.com/hathach/tinyusb) — USB stack
- [arduinoWebSockets](https://github.com/Links2004/arduinoWebSockets) — WebSocket support
- [Secigo](https://www.secigo.com/) — TLS Root CA
- [Fastly](https://www.fastly.com/) — GitHub raw CDN

---

**Happy controlling!** 🎮
