# ESP32-HID-Web-Remote-Controller - V4

**ESP32‑S3 USB HID (Mouse/Keyboard) with Wi‑Fi AP, captive portal, persistent settings, STA (client) mode with retry, logging, and robust USB enumeration.**  
Control your computer or TV wirelessly from your phone or tablet – settings survive power cycles.

[![GitHub release](https://img.shields.io/github/v/release/Aminiow/ESP32-HID-Web-Remote-Controller)](https://github.com/Aminiow/ESP32-HID-Web-Remote-Controller/releases)
[![PlatformIO](https://img.shields.io/badge/PlatformIO-ESP32S3-orange)](https://platformio.org/)
[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)

---

## 🌟 Features

- 🖱 **Mouse** – move, click (left/right/middle), double‑click, press/release, scroll wheel.
- ⌨ **Keyboard** – type text, send key taps (including special keys), press & hold modifiers (Ctrl, Alt, Shift, Win).
- 🔧 **Sticky modifiers** – tap to toggle Ctrl, Alt, Shift, Win (visual feedback on web UI).
- 💾 **Persistent settings** – sensitivity, repeat interval, and legacy mode are saved in flash (Preferences) and restored after power‑cycle.
- 📶 **Wi‑Fi Access Point** – creates its own network (`ESP32-Mouse` / password `12345678`).
- 🔗 **STA (Client) Mode** – can simultaneously connect to an existing Wi‑Fi network. Credentials are saved and auto‑reconnect with retries and exponential backoff.
- 📡 **Wi‑Fi scanning** – scan for networks and connect via the web interface (supports WPA3 and hidden networks).
- 🔁 **Automatic retry** – if STA connection fails, it will retry up to 3 times with a configurable delay.
- 🔗 **Captive Portal** – any DNS request resolves to the ESP32; any HTTP request shows the web UI.
- 📱 **Responsive Web Interface** – works on phones, tablets, and desktops; touch‑friendly with a mouse pad.
- 📊 **On‑board logging** – view logs in the web UI to help debug; raw logs available at `/logs` (JSON‑escaped).
- 🎛 **Adjustable Settings** – sensitivity, repeat interval, and legacy mode (slower key presses for older hosts) – settings are saved automatically with debounced HTTP requests.
- ⚡ **USB HID** – uses TinyUSB to emulate a standard USB mouse and keyboard – works out‑of‑the‑box on most OSes (Windows, macOS, Linux, Android, smart TVs).
- 🔄 **Improved USB enumeration** – `USB.begin()` is called first with a small delay, ensuring the host detects the device reliably.
- 🛡 **JSON escaping** – all API responses are properly JSON‑escaped to prevent injection.

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
- `Preferences` (built‑in)

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
- USB CDC On Boot: **Disabled** (critical for HID)
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

After flashing, the ESP32 will create the Wi‑Fi network **ESP32-Mouse** (password `12345678`).

---

## 📱 Usage

### Quick Start

1. **Connect** your phone/tablet/laptop to the Wi‑Fi network `ESP32-Mouse`.
2. **Open any web browser** and type any domain – the captive portal will redirect to `http://192.168.4.1/`.
3. **Plug the ESP32** into your computer/TV via USB‑C – it will be recognised as a mouse and keyboard.
4. **Use the web UI** to control the cursor and type.

### Web Interface

- **Mouse Pad** – drag to move the cursor; tap for a left‑click.
- **Arrow keys** – hold for repeated movement (repeat interval adjustable).
- **Mouse buttons** – left, right, middle click; double‑click; press/release.
- **Keyboard grid** – full QWERTY layout with modifiers.
  - Modifier keys (Ctrl, Alt, Win) – tap for press/release, or hold to toggle sticky mode (visual feedback).
  - Shift toggles on click – useful for uppercase letters.
- **Text input** – type arbitrary text (ASCII only) with one click.
- **Real‑time input** – type live; backspace works.
- **Settings** – sensitivity and repeat interval sliders, legacy mode checkbox (slower key timing). Settings are saved automatically to flash with debounce (400ms).
- **Logs** – view client‑side logs; raw logs available at `/logs` (JSON).

### Wi‑Fi STA (Client) Mode

1. Click the **📶** icon in the top‑right corner of the main page (or go to `/sta`).
2. The page will automatically scan for available Wi‑Fi networks.
3. Click on a network to fill in the SSID and BSSID.
4. Enter the password.
5. Tick **Hidden network** if your network is hidden.
6. Click **Connect** – the ESP32 will attempt to connect.
7. The status will update automatically. Once connected, you can still access the web UI via the ESP32's AP IP (`192.168.4.1`).

> **Note:** The ESP32 remembers the STA credentials and will attempt to reconnect after each power‑cycle (with retries). If connection fails, it will retry up to 3 times with a 5‑second interval.

---

## 🛠 Troubleshooting

| Symptom | Possible cause / solution |
|---------|---------------------------|
| **TV/computer does not recognise USB HID** | 1. Ensure **USB CDC On Boot** is **Disabled** in board settings.<br>2. Try a different USB cable (data‑capable).<br>3. Power the ESP32 externally if the USB port can't supply enough current.<br>4. The code now uses `USB.begin(); delay(100); Mouse.begin(); Keyboard.begin();` – this order is proven to work on most hosts.<br>5. If you have a USB‑C to USB‑A cable, ensure it has the proper pull‑up resistors (some cheap cables are charge‑only). |
| **Can’t connect to Wi‑Fi AP** | Check that the ESP32 is powered and the AP is active (look for `ESP32-Mouse` in Wi‑Fi scans). |
| **Captive portal not redirecting** | Manually type `http://192.168.4.1` in your browser. |
| **Keyboard keys not sending** | Verify the USB connection and that the host has focus on a text field. |
| **STA connection fails / retries** | Check the SSID and password. The ESP32 will retry up to 3 times. If it still fails, check the logs at `/logs` for details. |
| **Settings not saved** | Ensure the Preferences namespace has enough space (default should be fine). Settings are saved to flash – they persist across power‑cycles. |
| **Wi‑Fi scan doesn't show networks** | Make sure you are in range and that the ESP32’s antenna is connected. On the STA settings page, click “Retry” if the scan times out. |

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
