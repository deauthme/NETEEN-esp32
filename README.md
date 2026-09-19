# NETEEN v1.0.0

First public release of **NETEEN** — a standalone ESP32-based fake access point + captive portal toolkit for authorized WiFi security testing.

Runs completely offline on a single ESP32 dev board. No Raspberry Pi, no laptop, no internet backhaul needed.

---

## Features

### Fake Access Point
- 10 SSID categories with 100 pre-set names (EN + ES)
  - Cafes, Airports, Hotels, Malls, Restaurants, Transit, Gyms, Hospitals, Schools, Public
- Custom SSID naming (open network)

### Captive Portal
- 10 built-in login presets matching real services:
  - Google, Facebook, Instagram, Microsoft, Twitter/X, TikTok, Apple ID, Netflix, LinkedIn, Generic
- Each preset uses real brand colors, real SVG logos, correct field labels, and language footers
- Brand presets like Facebook/Instagram don't ask for a WiFi password (email + password only, like the real thing)
- Custom portal builder: 6 color themes + configurable field labels (Email, Username, Phone, PIN, etc.)
- Realistic "Verifying..." spinner with random 0.6-2s delay on submit
- Auto-redirect to `/success` page after capture

### Storage
- All captured credentials stored on internal flash via **LittleFS** (no SD card required)
- Each capture = separate `.txt` file, named by IP (`192-168-4-2.txt`, `192-168-4-2-2.txt` if repeated)
- Structured format: IP, device (parsed from User-Agent), SSID, style, and all submitted fields
- ~1.5 MB of usable storage (hundreds of captures)

### Admin Panel
- Available at `http://192.168.4.1/admin` while AP is running
- HTTP Basic Auth: user `admin` / pass `admin123`
- Features:
  - List all capture files with counts
  - View any capture inline
  - Delete individual files
  - **Delete ALL** button
  - **Download all** captures bundled as one `.txt`

### User Interface
- SSD1306 0.96" OLED display (I2C)
- KY-023 analog joystick navigation (up / down / select / back)
- Brand icons drawn directly on the OLED (monochrome, 13x13 pixel art)
- Live AP status: SSID, IP, connected clients, credential count
- Flash of last captured email on the running screen
- Lock screen mode (blank display, wakes on joystick press)

---

## Hardware

| Component | GPIO | Notes |
|-----------|------|-------|
| SSD1306 OLED (I2C) | SDA → 21, SCL → 22 | Address 0x3C |
| KY-023 Joystick | VRx → 34, VRy → 35, SW → 32 | ADC1 channels |
| Power | 5V via USB or 3.7V LiPo | ~150mA @ 5V |

Optional: buzzer, RGB LED.

**Board:** ESP32 Dev Module (any ESP32 with WiFi)

---

## Libraries

Install via Arduino Library Manager:

- `Adafruit SSD1306`
- `Adafruit GFX Library`

Built-in with ESP32 core (no extra install):

- `WiFi`
- `WebServer`
- `DNSServer`
- `LittleFS`
- `Wire`

---

## Build & Flash

```bash
arduino-cli compile --fqbn esp32:esp32:esp32 .
arduino-cli upload -p /dev/cu.usbserial-XXXX --fqbn esp32:esp32:esp32 .
```

Or just open `NETEEN.ino` in the Arduino IDE, select **ESP32 Dev Module**, and click **Upload**.

---

## Usage

1. Power on ESP32
2. Navigate the menu with the joystick:
   - **Up / Down** — scroll
   - **Press / Right** — select
   - **Left** — back / stop AP
3. `Fake Access Point` → `Standard` or `Custom` → pick SSID category → pick SSID → pick login style → **AP starts**
4. Victims connect to the network — their device's captive portal check will open the login page automatically
5. Captured credentials appear:
   - In the serial monitor
   - On the OLED display
   - In the admin panel at `http://192.168.4.1/admin`
   - As files on LittleFS

---

## Serial Output Format

```
========== CREDENTIAL CAPTURED ==========
IP: 192.168.4.2
Device: iPhone
SSID: Cafe WiFi Free
Style: Facebook
Email or Phone:
  victim@example.com
Password:
  hunter2
=========================================
```

Long values wrap at 60 chars, so you can copy-paste straight from a serial terminal.

---

## Disclaimer

**FOR EDUCATIONAL AND AUTHORIZED SECURITY TESTING ONLY.**

Do not use this tool on networks you do not own or have explicit written permission to test. Unauthorized interception of network traffic and credentials is illegal in most jurisdictions. The author is not responsible for any misuse or damage caused by this software.

---

## Author

made by **[@deauthme](https://github.com/deauthme)**

If you build something cool with this — open an issue or drop a star.
