# ESP32-HID-Web-Remote-Controller - V1
ESP32‑S3 Wi‑Fi to USB HID bridge with web‑based mouse/keyboard control, captive portal, and STA/AP mode.

**ESP32‑S3 Wi‑Fi to USB HID bridge with a web‑based remote control**  
Turn your ESP32 into a wireless mouse and keyboard – works with any device that has a USB port (computers, smart TVs, Android, etc.).

[![GitHub release](https://img.shields.io/github/v/release/Aminiow/ESP32-HID-Web-Remote-Controller)](https://github.com/Aminiow/ESP32-HID-Web-Remote-Controller/releases)
[![PlatformIO](https://img.shields.io/badge/PlatformIO-ESP32S3-orange)](https://platformio.org/)
[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)

---

## 🌟 Features

- 🖱 **Mouse control** – move, click (left/right/middle), double‑click, press/release, scroll wheel.
- ⌨ **Keyboard control** – type text, send individual key taps, press & hold keys (including modifiers).
- 🔧 **Sticky modifiers** – toggle Ctrl, Alt, Shift, Win for shortcuts (long‑press on the web UI toggles them).
- 📶 **Dual Wi‑Fi modes** – runs as an Access Point (AP) and can simultaneously connect to a Wi‑Fi network (STA).
- 🔍 **Auto‑channel selection** – scans nearby networks and picks the least crowded Wi‑Fi channel for your AP.
- 🔒 **Hidden SSID** – option to hide the AP name for extra privacy.
- 📱 **Captive portal** – any DNS request resolves to the ESP32’s IP; any HTTP request redirects to the web interface.
- 🎨 **Responsive web UI** – works on phones, tablets, and desktops; touch‑friendly with a dedicated mouse pad.
- 💾 **Settings persistence** – sensitivity, repeat interval, and legacy mode are saved in NVS (flash memory).
- 📊 **On‑board logging** – view logs in the web UI to help debug.
- 🔗 **STA auto‑connect** – remembers and tries to connect to a saved Wi‑Fi network after boot.
- ⚡ **USB HID** – uses TinyUSB to emulate a standard USB mouse and keyboard – works out‑of‑the‑box on most OSes.

---

## 🧰 Hardware Requirements

- **ESP32‑S3** (any board with native USB‑OTG support – e.g., DevKitC‑1, S3‑Mini, etc.)
- USB‑C cable (to connect to the host device)
- (Optional) external power supply if the host USB port cannot provide enough current

> **Note:** The ESP32‑S3 must be configured to use **USB‑OTG (TinyUSB)** – not the default USB‑Serial‑JTAG.

---

## 📦 Software & Libraries

The code is written for the **Arduino framework** (compatible with Arduino IDE and PlatformIO).  
Required libraries (install via Arduino Library Manager or PlatformIO):

- [`WiFi`](https://github.com/espressif/arduino-esp32/tree/master/libraries/WiFi) (built‑in)
- [`WebServer`](https://github.com/espressif/arduino-esp32/tree/master/libraries/WebServer) (built‑in)
- [`DNSServer`](https://github.com/espressif/arduino-esp32/tree/master/libraries/DNSServer) (built‑in)
- [`WebSocketsServer`](https://github.com/Links2004/arduinoWebSockets) – for fast mouse movement.
- [`USB`](https://github.com/espressif/arduino-esp32/tree/master/libraries/USB) (built‑in)
- [`USBHIDMouse`](https://github.com/espressif/arduino-esp32/tree/master/libraries/USB) (built‑in)
- [`USBHIDKeyboard`](https://github.com/espressif/arduino-esp32/tree/master/libraries/USB) (built‑in)
- [`Preferences`](https://github.com/espressif/arduino-esp32/tree/master/libraries/Preferences) (built‑in)

> **For PlatformIO** – simply add the required libraries to your `platformio.ini`; the code includes all necessary `#include` directives.

---

## 🚀 Installation & Flashing

### 1. Clone the repository
```bash
git clone https://github.com/Aminiow/ESP32-HID-Web-Remote-Controller.git
cd ESP32-HID-Web-Remote-Controller
```

### 2. Open the project in Arduino IDE (or PlatformIO)

- In Arduino IDE: open the `.ino` file inside the folder.
- In PlatformIO: open the project folder.

### 3. Configure the board settings

**For Arduino IDE:**
- Board: **ESP32S3 Dev Module**
- USB Mode: **USB‑OTG (TinyUSB)**
- USB CDC On Boot: **Disabled** (important for HID to work)
- Upload Speed: 921600 (optional)
- Partition Scheme: **Default 4MB with spiffs** (or any that fits)

**For PlatformIO:**  
Add the following to your `platformio.ini`:

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

### 4. Upload the code

- Connect your ESP32‑S3 via USB‑C (make sure it’s the one that supports OTG, not just serial).
- Press the **Upload** button.

> **First upload may require holding the BOOT button** while connecting – release after upload starts.

After flashing, the ESP32 will restart and create a Wi‑Fi access point named **ESP32-Mouse** (password: `12345678`). The SSID can be hidden (see Configuration).

---

## 📱 Usage

### Connect to the ESP32

- On your phone, tablet, or laptop, **connect to the Wi‑Fi network** `ESP32-Mouse` (password `12345678`).
- If the SSID is hidden, manually add it.
- Once connected, open any web browser and type **any domain** – the captive portal will redirect to `http://192.168.4.1/` (the ESP32’s IP).
- The web interface will load automatically.

> The captive portal works because the ESP32 runs a DNS server that responds to all queries with its own IP address.

### Control your host device

1. **Plug the ESP32 into your computer/TV** using the USB‑C port (the same one used for programming – it will enumerate as a mouse and keyboard).
2. Use the web UI on your phone/tablet to control the mouse pointer and type text.
3. The mouse movements, clicks, and keyboard inputs are sent directly via USB HID – no drivers needed.

### Web Interface Overview

- **Mouse Pad** – drag to move the cursor; tap for left click.
- **Arrow keys** – hold to repeat movement (useful for fine‑tuning).
- **Mouse buttons** – left, right, middle click; double‑click; press/release.
- **Keyboard grid** – full QWERTY layout with modifiers (Ctrl, Alt, Win, Shift).
  - Toggle modifiers by **long‑pressing** Ctrl/Alt/Win (short press = tap).
  - Shift toggles on click (for uppercase).
- **Text input** – type arbitrary text (ASCII only) with one click.
- **Real‑time input** – type live; backspace works.
- **Settings sliders** – adjust sensitivity and repeat interval.
- **STA status** – shows if connected to a Wi‑Fi network and its IP.

---

## ⚙️ Configuration

### Hidden SSID

To hide the AP name, modify the `WiFi.softAP()` call in `setup()`:

```cpp
WiFi.softAP(ap_ssid, ap_password, channel, true);  // last parameter = hidden
```

### STA (Client) Mode

- The ESP32 can simultaneously connect to an existing Wi‑Fi network (STA) while still serving its own AP.
- To set this up, connect to the AP, go to the **Wi‑Fi settings** page (`http://192.168.4.1/sta`), scan for networks, and enter credentials.
- The credentials are saved in NVS and the ESP32 will attempt to auto‑connect on every boot (after a 10‑second delay to allow USB enumeration).

### Auto‑Channel Selection

The firmware automatically scans for nearby networks at boot (after USB initialisation) and selects the least crowded channel for its AP. This reduces interference and improves Wi‑Fi stability.

### Settings Persistence

- Sensitivity, repeat interval, and legacy mode are saved to NVS and restored after power‑cycle.
- Changes made via the sliders are saved automatically.

---

## 🛠 Troubleshooting

| Symptom | Possible cause / solution |
|---------|---------------------------|
| **TV/computer does not recognise USB HID** | 1. Ensure **USB CDC On Boot** is **Disabled** in board settings.<br>2. Try a different USB cable (data‑capable).<br>3. Power the ESP32 externally (some USB ports don’t provide enough current).<br>4. Re‑order `Mouse.begin(); Keyboard.begin(); USB.begin();` – the code uses this order. |
| **Wi‑Fi AP not visible** | The SSID may be hidden – manually add the network `ESP32-Mouse` with password `12345678`. |
| **Can’t connect to AP** | Check if the ESP32 is still booting – wait ~10 seconds after power‑on. Also ensure no other device is using the same IP (192.168.4.1). |
| **Captive portal not redirecting** | Make sure the client’s DNS is set to the ESP32 (most devices auto‑detect). Try typing `http://192.168.4.1` directly. |
| **Keyboard keys not sending** | Check that the USB cable is properly connected and the host device recognises the keyboard. Test with a simple key (e.g., type “hello” in a text field on the host). |

---

## 📜 License

This project is licensed under the **MIT License** – see the [LICENSE](LICENSE) file for details.

---

## 🤝 Contributing

Contributions, bug reports, and feature requests are welcome!  
Please open an issue or submit a pull request on [GitHub](https://github.com/Aminiow/ESP32-HID-Web-Remote-Controller).

---

## 🙏 Acknowledgements

- [Espressif Systems](https://www.espressif.com/) for the ESP32‑S3 and the Arduino core.
- [TinyUSB](https://github.com/hathach/tinyusb) for the USB stack.
- [arduinoWebSockets](https://github.com/Links2004/arduinoWebSockets) for WebSocket support.

---

## 📖 Further Reading

- [ESP32‑S3 USB OTG Guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/peripherals/usb_otg.html)
- [Arduino ESP32 USB HID Examples](https://github.com/espressif/arduino-esp32/tree/master/libraries/USB)
- [Captive Portal on ESP32](https://github.com/espressif/arduino-esp32/tree/master/libraries/DNSServer)

---

**Happy hacking!**
