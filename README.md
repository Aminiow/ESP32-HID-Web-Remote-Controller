# ESP32-HID-Web-Remote-Controller - V8

**ESP32‑S3 Wi‑Fi to USB HID bridge with web‑based mouse/keyboard control, captive portal, STA/AP mode, auto‑channel selection, hidden SSID, over‑the‑air (OTA) firmware updates, consumer controls (media keys), gyro mouse support, mDNS, Wi‑Fi power management, idle sleep, and **SHA‑256 verified firmware uploads**.**

Control your computer or TV wirelessly from your phone or tablet – settings survive power cycles.

[![GitHub release](https://img.shields.io/github/v/release/Aminiow/ESP32-HID-Web-Remote-Controller)](https://github.com/Aminiow/ESP32-HID-Web-Remote-Controller/releases)
[![PlatformIO](https://img.shields.io/badge/PlatformIO-ESP32S3-orange)](https://platformio.org/)
[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)

---

## 🌟 Features

- 🖱 **Mouse** – move, click (left/right/middle), double‑click, press/release, scroll wheel.
- ⌨ **Keyboard** – type text, send key taps (including special keys), press & hold modifiers (Ctrl, Alt, Shift, Win).
- 🔧 **Sticky modifiers** – tap to toggle Ctrl, Alt, Shift, Win (visual feedback on web UI).
- 💾 **Persistent settings** – sensitivity, repeat interval, legacy mode, boot protocol, gyro enable, TX power, and power save are saved in flash and restored after power‑cycle.
- 📶 **Wi‑Fi Access Point** – creates its own network with **auto‑channel selection** (scans for the least crowded channel).
- 🔒 **Hidden SSID** – the AP name (`ESP32-Mouse`) is hidden by default for privacy (can be changed in code).
- 🔗 **STA (Client) Mode** – can simultaneously connect to an existing Wi‑Fi network. Credentials are saved and auto‑reconnect with retries.
- 📡 **Wi‑Fi scanning** – scan for networks and connect via the web interface (supports WPA3 and hidden networks).
- 🔁 **Automatic retry** – if STA connection fails, it will retry up to 3 times with a configurable delay.
- 🔗 **Captive Portal** – any DNS request resolves to the ESP32; any HTTP request redirects to the web UI (HTTP 302).
- 📱 **Responsive Web Interface** – works on phones, tablets, and desktops; touch‑friendly with a mouse pad.
- 📊 **On‑board logging** – view logs in the web UI to help debug; raw logs available at `/logs` (JSON).
- 🎛 **Adjustable Settings** – sensitivity, repeat interval, legacy mode (slower key presses for older hosts), boot protocol (keyboard compatibility), TX power, and power save – settings are saved automatically with debounced HTTP requests.
- ⚡ **USB HID** – uses TinyUSB to emulate a standard USB mouse and keyboard – works out‑of‑the‑box on most OSes (Windows, macOS, Linux, Android, smart TVs).
- 🔄 **Robust USB enumeration** – `USB.begin()` is called first, ensuring the host detects the device reliably.
- 🛡 **JSON escaping** – all API responses are properly JSON‑escaped to prevent injection.
- 🚀 **Over‑the‑Air (OTA) Firmware Updates** – check for new firmware from a remote server, download and verify with SHA‑256, then reboot. Also supports manual upload of `.bin` files via the web UI.
- 🔐 **SHA‑256 Verified Uploads** – when uploading firmware manually, the ESP32 automatically fetches the expected hash from the version file (if WiFi is connected) and verifies the uploaded file before applying the update.
- 🎛️ **Consumer Controls (Media Keys)** – volume up/down, mute, channel up/down, power, input menu, and input select – perfect for controlling TVs and media players.
- 📱 **Gyro Mouse Support** – use your phone's orientation sensors to move the cursor by tilting the device.
- 🌐 **mDNS** – access the web interface at `esp32-mouse.local` (if your device supports mDNS/Bonjour).
- 🔋 **Wi‑Fi Power Management** – adjustable TX power (0–20 dBm) and modem sleep to reduce power consumption.
- 😴 **Idle Sleep** – when no Wi‑Fi clients are connected and STA is inactive, the ESP32 enters light sleep (reduces CPU frequency and enables maximum modem sleep) to save power.
- 📋 **Dynamic Version Display** – the web UI automatically shows the current firmware version in the page title, heading, and footer.

---

## 🆕 What's New in v8

- **SHA‑256 Verified Firmware Uploads** – when uploading a `.bin` file via the web UI, the ESP32 now fetches the expected hash from the version file (if WiFi is connected) and verifies the file before applying the update. This prevents corrupted or mismatched firmware from being installed.
- **Dynamic Version Display** – the web page title, main heading, and footer now automatically display the current firmware version (fetched from `/update_status`).
- **Improved Consumer Controls** – added `INPUT_MENU` (opens input menu) and `INPUT_SELECT` (direct input switch) for better TV/media control.
- **`/get_urls` Endpoint** – new API endpoint to fetch the current update URLs, allowing the web UI to load and display them without hardcoding.
- **Default Update URLs** – now point to the GitHub repository (`https://raw.githubusercontent.com/Aminiow/ESP32-HID-Web-Remote-Controller/main/version.txt` and `firmware.bin`) for easy out‑of‑the‑box updates.
- **Version Bump** – firmware version is now `8.0.0`.
- **HTML/UI Cleanup** – improved version display and update status handling.

---

## 🧰 Hardware Requirements

- **ESP32‑S3** (any board with native USB‑OTG support – e.g., DevKitC‑1, S3‑Mini, etc.)
- USB‑C cable (to connect to the host device)

> **Note:** The ESP32‑S3 must be configured to use **USB‑OTG (TinyUSB)** – not the default USB‑Serial‑JTAG.

---

## 📦 Software & Libraries

The code is written for the **Arduino framework** (compatible with Arduino IDE and PlatformIO).  
Required libraries (install via Arduino Library Manager or PlatformIO):

- `WiFi` (built‑in)
- `WebServer` (built‑in)
- `DNSServer` (built‑in)
- `WebSocketsServer` (from [arduinoWebSockets](https://github.com/Links2004/arduinoWebSockets))
- `USB` (built‑in)
- `USBHIDMouse` (built‑in)
- `USBHIDKeyboard` (built‑in)
- `USBHIDConsumerControl` (built‑in)
- `Preferences` (built‑in)
- `HTTPClient` (built‑in)
- `Update` (built‑in)
- `WiFiClientSecure` (built‑in)
- `mbedtls` (built‑in)
- `ESPmDNS` (built‑in)
- `esp_wifi` (built‑in)
- `esp_sleep` (built‑in)

---

## 🚀 Installation & Flashing

### 1. Clone the repository
```bash
git clone https://github.com/Aminiow/ESP32-HID-Web-Remote-Controller.git
cd ESP32-HID-Web-Remote-Controller
```

### 2. Open in Arduino IDE (or PlatformIO)

- In Arduino IDE: open the `.ino` file inside the folder.
- In PlatformIO: open the project folder.

### 3. Configure board settings

**For Arduino IDE:**
- Board: **ESP32S3 Dev Module**
- USB Mode: **USB‑OTG (TinyUSB)**
- Upload Mode: **UART0 / Hardware CDC** (or **USB‑OTG** if using the native USB port for flashing)
- USB CDC On Boot: **Disabled** (critical for HID)
- USB Firmware MSC On Boot: **Enabled** (ESP32-S2/3 Only)
- Upload Speed: 921600 (optional)

**For PlatformIO – example `platformio.ini`:**
```ini
[env:esp32-s3-devkitc-1]
platform = espressif32
board = esp32-s3-devkitc-1
framework = arduino
monitor_speed = 115200
build_flags =
    -DARDUINO_USB_MODE=1
    -DARDUINO_USB_CDC_ON_BOOT=0
```

### 4. Upload

- Connect your ESP32‑S3 via USB‑C (the port that supports OTG).
- Press the **Upload** button.
- If needed, hold the BOOT button during upload.

After flashing, the ESP32 will create the Wi‑Fi network **ESP32-Mouse** (password `12345678`). The SSID is **hidden** – you will need to manually add the network on your device.

---

## 📱 Usage

### Quick Start

1. **Manually add** the Wi‑Fi network `ESP32-Mouse` (password `12345678`) on your phone/tablet/laptop (it won't appear in scans because it's hidden).
2. **Open any web browser** and type any domain – the captive portal will redirect to `http://192.168.4.1/` – or use `esp32-mouse.local` if mDNS is supported.
3. **Plug the ESP32** into your computer/TV via USB‑C – it will be recognised as a mouse and keyboard.
4. **Use the web UI** to control the cursor, type, send media commands, and more.

### Web Interface

- **Mouse Pad** – drag to move the cursor; tap for a left‑click.
- **Arrow keys** – hold for repeated movement (repeat interval adjustable).
- **Mouse buttons** – left, right, middle click; double‑click; press/release.
- **Keyboard grid** – full QWERTY layout with modifiers.
  - Modifier keys (Ctrl, Alt, Win) – tap for press/release, or hold to toggle sticky mode (visual feedback).
  - Shift toggles on click – useful for uppercase letters.
- **Text input** – type arbitrary text (ASCII only) with one click.
- **Real‑time input** – type live; backspace works.
- **Settings** – sensitivity, repeat interval, legacy mode, boot protocol, TX power, and power save. Settings are saved automatically to flash with debounce (400ms).
- **Gyro Mouse** – enable to use your phone's orientation sensors to move the cursor. (On iOS, you may need to grant permission.)
- **Media Controls** – volume up/down, mute, channel up/down, power, input menu, and input select – perfect for TVs and media players.
- **Logs** – view client‑side logs; raw logs available at `/logs` (JSON).
- **Firmware Update** – set URLs, check for updates, trigger updates, and upload `.bin` files manually. When uploading, the ESP32 will verify the SHA‑256 hash against the version file (if available).

### Wi‑Fi STA (Client) Mode

1. Click the **📶** icon in the top‑right corner of the main page (or go to `/sta`).
2. The page will automatically scan for available Wi‑Fi networks.
3. Click on a network to fill in the SSID and BSSID.
4. Enter the password.
5. Tick **Hidden network** if your network is hidden.
6. Click **Connect** – the ESP32 will attempt to connect.
7. The status will update automatically. Once connected, you can still access the web UI via the ESP32's AP IP (`192.168.4.1`) or via `esp32-mouse.local`.

> **Note:** The ESP32 remembers the STA credentials and will attempt to reconnect after each power‑cycle (with retries). If connection fails, it will retry up to 3 times with a 5‑second interval.

### Media / Consumer Controls

The **Media / TV** card provides buttons for common consumer control commands:
- 🔊 Volume Up/Down
- 🔇 Mute
- 📺 Channel Up/Down
- ⏻ Power
- 📡 Menu – opens the input source menu
- 📡 Select – switches to a specific input (works on many TVs)

These commands are sent via the USB HID Consumer Control interface and work with most TVs, media players, and computers that support USB HID consumer controls.

### Gyro Mouse Control

Enable the **Gyro Mouse** checkbox to use your phone's orientation sensors:
- Tilt the device forward/backward to move the cursor vertically.
- Tilt left/right to move horizontally.
- The sensitivity is controlled by the main **Sensitivity** slider.
- On iOS, you may need to grant motion permission when prompted.

---

## ⚙️ Configuration

### Hidden SSID

By default, the AP SSID is hidden. To make it visible, change this line in `setup()`:

```cpp
WiFi.softAP(ap_ssid, ap_password, bestChannel, true);  // true = hidden, false = visible
```

To change the SSID and password, modify these constants at the top of the file:

```cpp
const char* ap_ssid = "ESP32-Mouse";
const char* ap_password = "12345678";
```

### Auto‑Channel Selection

The ESP32 automatically scans nearby networks at boot and selects the least crowded channel (1-11) for its AP. This reduces interference and improves Wi‑Fi stability.

### USB Enumeration Order

v8 uses the proven order:
```cpp
USB.begin();
Keyboard.begin();
Mouse.begin();
ConsumerControl.begin();
```
This ensures the host detects the device reliably on the first attempt, even on older TVs and computers.

### Boot Protocol Mode

When enabled, this sends an empty HID report between keystrokes, which improves compatibility with older BIOS/UEFI systems and some smart TVs that expect a clean release between key presses. Enable it via the web UI or by setting `bootProtocolMode = true` in Preferences.

### Wi‑Fi Power Management

- **TX Power** – adjust the output power of the Wi‑Fi radio from 0 to 20 dBm. Lower values reduce power consumption and range.
- **Power Save** – when enabled, the ESP32 enters modem sleep when the Wi‑Fi is idle, reducing power consumption. Disable for lower latency.

### Idle Sleep

When no Wi‑Fi clients are connected to the AP and STA is inactive for 60 seconds, the ESP32 enters a light sleep state:
- CPU frequency is reduced to 80 MHz.
- Maximum modem sleep is enabled.
- The device wakes up immediately when a client connects or when the STA becomes active.

### Firmware Update URLs

The update mechanism requires two URLs:
- **Version URL** – points to a plain text file containing the version string (e.g., `8.0.0`) and optionally a SHA‑256 hash of the firmware binary on the next line.
- **Binary URL** – points to the actual firmware `.bin` file.

Default URLs point to the GitHub repository for easy updates. You can change them via the web UI or the `/set_urls` endpoint. When uploading firmware manually, the ESP32 will automatically fetch the expected hash from the version URL (if WiFi is connected) and verify the uploaded file.

---

## 🛠 Troubleshooting

| Symptom | Possible cause / solution |
|---------|---------------------------|
| **TV/computer does not recognise USB HID** | 1. Ensure **USB CDC On Boot** is **Disabled** in board settings.<br>2. Try a different USB cable (data‑capable).<br>3. Power the ESP32 externally if the USB port can't supply enough current.<br>4. The code uses `USB.begin(); Keyboard.begin(); Mouse.begin();` – this order works on most hosts.<br>5. If you have a USB‑C to USB‑A cable, ensure it has the proper pull‑up resistors. |
| **Can’t find Wi‑Fi AP** | The SSID is **hidden**. Manually add the network `ESP32-Mouse` with password `12345678`. |
| **Captive portal not redirecting** | Manually type `http://192.168.4.1` or `esp32-mouse.local` in your browser. |
| **Keyboard keys not sending** | Verify the USB connection and that the host has focus on a text field. |
| **STA connection fails / retries** | Check the SSID and password. The ESP32 will retry up to 3 times. Check logs at `/logs`. |
| **Settings not saved** | Ensure the Preferences namespace has enough space. Settings persist across power‑cycles. |
| **Wi‑Fi scan doesn't show networks** | Make sure you are in range and the antenna is connected. |
| **Firmware update fails** | Check the version and binary URLs. The SHA‑256 hash must match (if provided). Ensure STA is connected. |
| **Manual upload rejected with "Hash mismatch"** | The uploaded firmware file does not match the expected hash from the version file. Download a fresh copy or disable hash checking (only possible by modifying the code). |
| **Consumer controls not working** | Some hosts may not support consumer control commands. Test with a different device. |
| **Gyro mouse not working** | On iOS, you must grant permission when prompted. Check that the browser supports `deviceorientation` events. |
| **Web interface slow or unresponsive** | Disable power save or increase TX power. Ensure the device is not in idle sleep. |
| **Version not updating in UI** | Check that `/update_status` is reachable and returns valid JSON. The UI polls this endpoint every 30 seconds. |

---

## 📜 License

This project is licensed under the **MIT License** – see the [LICENSE](LICENSE) file for details.

---

## 🤝 Contributing

Contributions and bug reports are welcome!  
Open an issue or submit a pull request on [GitHub](https://github.com/Aminiow/ESP32-HID-Web-Remote-Controller).

---

## 🙏 Acknowledgements

- [Espressif Systems](https://www.espressif.com/) for the ESP32‑S3 and Arduino core.
- [TinyUSB](https://github.com/hathach/tinyusb) for the USB stack.
- [arduinoWebSockets](https://github.com/Links2004/arduinoWebSockets) for WebSocket support.

---

**Happy controlling!** 🎮
