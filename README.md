# NETEEN-esp32

Standalone ESP32 fake access point + captive portal toolkit for authorized WiFi security testing. Runs fully offline with a joystick-controlled UI.

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

## Quick Start (Terminal Only — No Arduino IDE Needed)

Everything below works on **macOS** and **Linux**. Windows users can use WSL or adjust the serial port path (`/dev/ttyUSB0` instead of `/dev/cu.usbserial-XXXX`).

### 1. Install `arduino-cli`

**macOS (Homebrew):**
```bash
brew install arduino-cli
```

**Linux (official install script):**
```bash
curl -fsSL https://raw.githubusercontent.com/arduino/arduino-cli/master/install.sh | sh
sudo mv bin/arduino-cli /usr/local/bin/
```

Verify:
```bash
arduino-cli version
```

### 2. Install the ESP32 board core

```bash
arduino-cli config init
arduino-cli config add board_manager.additional_urls https://espressif.github.io/arduino-esp32/package_esp32_index.json
arduino-cli core update-index
arduino-cli core install esp32:esp32
```

This downloads ~500 MB on first install.

### 3. Install the required libraries

```bash
arduino-cli lib install "Adafruit SSD1306"
arduino-cli lib install "Adafruit GFX Library"
```

`WiFi`, `WebServer`, `DNSServer`, `LittleFS` and `Wire` ship with the ESP32 core.

### 4. Get the source code

**Clone the repo:**
```bash
git clone https://github.com/deauthme/NETEEN-esp32.git
cd NETEEN-esp32
```

**Or create the sketch folder manually and paste `NETEEN.ino` into it:**
```bash
mkdir -p ~/Documents/Arduino/NETEEN-esp32
nano ~/Documents/Arduino/NETEEN-esp32/NETEEN-esp32.ino
# paste the code, save with Ctrl+O, Enter, exit with Ctrl+X
```

> **Important:** the folder name must match the `.ino` filename. If you clone the repo, either rename `NETEEN.ino` → `NETEEN-esp32.ino` (and keep the folder as `NETEEN-esp32`), or compile from the folder name as-is.

### 5. Find your board's serial port

Plug in the ESP32 via USB, then:

```bash
arduino-cli board list
```

Look for something like:

```
/dev/cu.usbserial-0001   serial   Serial Port (USB)   Unknown
/dev/cu.wchusbserial-XXXX
```

On Linux it will be `/dev/ttyUSB0` or `/dev/ttyACM0`.

If nothing shows up:
- **macOS**: you may need a driver for the USB-serial chip (CH340, CP2102, FTDI). Install with `brew install --cask wch-ch34x-usb-serial-driver` for CH340-based boards.
- **Linux**: add your user to `dialout`: `sudo usermod -a -G dialout $USER`, then log out and back in.

### 6. Compile

```bash
arduino-cli compile --fqbn esp32:esp32:esp32 .
```

Or with a fully-qualified folder path:

```bash
arduino-cli compile --fqbn esp32:esp32:esp32 ~/Documents/Arduino/NETEEN-esp32
```

You should see something like:

```
Sketch uses 1059536 bytes (80%) of program storage space.
Global variables use 54104 bytes (16%) of dynamic memory.
```

### 7. Upload to the board

Replace `/dev/cu.usbserial-0001` with your actual port:

```bash
arduino-cli upload -p /dev/cu.usbserial-0001 --fqbn esp32:esp32:esp32 .
```

If upload fails with `Failed to connect to ESP32: Timed out waiting for packet header`:
- hold the **BOOT** button on the board
- press and release **EN** (reset)
- release **BOOT**
- rerun the upload command

### 8. Open the serial monitor

```bash
arduino-cli monitor -p /dev/cu.usbserial-0001 -c baudrate=115200,dtr=off,rts=off
```

The `dtr=off,rts=off` part is **critical** — without it the ESP32 stays in bootloader mode and you only see garbage. If your board resets itself constantly, this is the fix.

Exit the monitor with **Ctrl+C**.

### 9. One-liner compile + upload + monitor

Once everything works, you can chain commands:

```bash
arduino-cli compile --fqbn esp32:esp32:esp32 . && \
arduino-cli upload -p /dev/cu.usbserial-0001 --fqbn esp32:esp32:esp32 . && \
arduino-cli monitor -p /dev/cu.usbserial-0001 -c baudrate=115200,dtr=off,rts=off
```

### 10. Rebuild after editing code

Just rerun step 9. If you only changed a few lines, the incremental build is fast (~5 seconds).

To force a clean rebuild:

```bash
arduino-cli compile --fqbn esp32:esp32:esp32 --clean .
```

### Common issues

| Problem | Fix |
|---------|-----|
| `Platform 'esp32:esp32' not found` | Run `arduino-cli core install esp32:esp32` again |
| `error: 'rawDataPtr' has no member` (IRremote) | Library API changed — this project doesn't use IRremote, ensure you cloned this repo not an older variant |
| `Failed to connect to ESP32` | Hold BOOT, tap EN, release BOOT, retry upload |
| Serial shows only `?????` or garbage | Add `dtr=off,rts=off` to the monitor command |
| OLED stays black | Check I2C address — some modules use `0x3D` instead of `0x3C`; edit `OLED_ADDR` in the sketch |
| `magick: unable to read font` | Not related to this project — you're trying to rasterize an SVG, use a browser or `rsvg-convert` instead |
| Wrong board detected | Force upload: add `--board-options UploadSpeed=115200` to the upload command |

---

## Build & Flash (Arduino IDE)

If you prefer the GUI:

1. Install the ESP32 board package in **Boards Manager**
2. Install `Adafruit SSD1306` and `Adafruit GFX Library` in **Library Manager**
3. Open `NETEEN-esp32.ino`
4. Select **Tools → Board → ESP32 Dev Module**
5. Select **Tools → Port → /dev/cu.usbserial-XXXX**
6. Click **Upload**
7. Open **Tools → Serial Monitor**, baud rate `115200`

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
