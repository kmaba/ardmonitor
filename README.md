# ardmonitor

Smart USB desktop monitor running on an Arduino Mega 2560 (or Uno) with a 16×2 I2C LCD, push button, and piezo buzzer.

Supports **Full Mode** (connected to home server) and **Mini / Companion Mode** (lightweight desktop companion for Windows/Linux/macOS).

---

## Features

### 1. Dual Operating Modes
- **Full Homeserver Mode (6 Screens)**:
  - **Clock**: Big 3×2 custom-glyph `HH:MM` display (12-hour, ticking, default screen).
  - **Pi-hole**: DNS request count on the left, blocking percentage on the right.
  - **3D Printer (Moonraker / Klipper)**: Solid+mesh progress bar, remaining time, scrolling filename, temperatures. Auto-switches to this slide when a print starts.
  - **Weather**: Real-time temperature, humidity, wind speed, and weather conditions (Open-Meteo).
  - **Azaan / Prayer Times**: Next prayer name with live countdown in hours/minutes (AlAdhan).
  - **Network**: WiFi SSID, signal strength, and tailnet / local IP.
- **Companion / Mini Mode (3 Screens)**:
  - Automatically activates when running the standalone `companion.py` script.
  - Reduces slides to: **Clock · Weather · Azaan**.

### 2. Physical & Virtual Controls
- **Short Press**: Cycle active screens / navigate menus.
- **Long Press (900ms)**: Open contextual action menu (e.g. Pause/Resume/Cancel print, toggle Pi-hole adblock, manual sync).
- **Super Long Press (3s)**: Toggle LCD backlight on/off.
- **Virtual Button**: Web panel replicates physical button hold gestures (`short`, `long`, `super`).

---

## Hardware & Wiring

- **Board**: Arduino Mega 2560 (recommended for SRAM headroom) or Arduino Uno R3.
- **Display**: 16×2 HD44780 LCD with PCF8574 I2C backpack (Address `0x27`).
- **Button**: Pin **D8** → GND (`INPUT_PULLUP`, active LOW).
- **Buzzer**: Pin **D9** → GND.

### I2C Pin Connections:
| Board | SDA | SCL | VCC | GND |
|---|---|---|---|---|
| **Arduino Mega 2560** | Pin **20** | Pin **21** | 5V | GND |
| **Arduino Uno R3** | Pin **A4** | Pin **A5** | 5V | GND |

---

## Software

### 1. Arduino Firmware (`HomeMonitor.ino`)
Compile and flash using Arduino IDE or `arduino-builder`:
```sh
# For Mega 2560:
arduino-builder -fqbn arduino:avr:mega:cpu=atmega2560 HomeMonitor.ino
# For Uno:
arduino-builder -fqbn arduino:avr:uno HomeMonitor.ino
```

### 2. Standalone Companion App
Cross-platform lightweight driver for Windows, Linux, and macOS. Auto-detects the Arduino on any USB/COM port, sets the Arduino to Mini Mode, and streams local clock, weather, and prayer times:

- **Windows (`companion.bat`)**:
  - Fully automated 1-click bootstrap.
  - Automatically downloads `companion.py` from GitHub if missing.
  - Prompts for UAC and installs Python 3 if not present on the system.
  - Auto-installs `pyserial` and connects to the display.
- **Linux / macOS (`companion.sh`)**:
  - Automatically downloads `companion.py` if missing.
  - Auto-installs Python 3 via system package manager (apt/pacman/dnf/brew) if needed.
  - Auto-installs `pyserial` and runs the driver.
- **Manual**:
  ```sh
  pip install pyserial
  python companion.py
  ```

### 3. Full Home Server Bridge (`homemonitor_bridge.py`)
Runs as a systemd service on a home server or PC connected to the Arduino. Polls Pi-hole v6 API, Moonraker Klipper API, weather, and azaan, and serves a mobile-responsive web controller at `http://localhost:8080`.

---

## 3D Printable Enclosure

The `enclosure.scad` file provides a friction-fit 2-piece case with exact cutouts for the LCD, side USB/power cables, front button, and Arduino PCB standoffs. Pre-rendered STLs (`bottom_shell.stl`, `top_lid.stl`) are included.
