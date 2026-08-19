#!/usr/bin/env python3
"""
HomeMonitor Companion App — Lightweight cross-platform display driver (Windows / Linux / macOS)
Streams Clock (local time), Weather (Open-Meteo), and Azaan prayer times (AlAdhan)
to the Arduino HomeMonitor display over USB serial.

Requirements:
    pip install pyserial
Usage:
    python companion.py
"""

import sys, time, json, datetime, threading
import urllib.request

try:
    import serial
    import serial.tools.list_ports
except ImportError:
    print("Error: pyserial is required. Install it with:\n    pip install pyserial")
    sys.exit(1)

BAUD = 115200

# Weather: Open-Meteo for Kewdale / Perth (-31.978, 115.951)
WEATHER_URL = (
    "https://api.open-meteo.com/v1/forecast"
    "?latitude=-31.978&longitude=115.951"
    "&current=temperature_2m,relative_humidity_2m,wind_speed_10m,weather_code"
    "&timezone=Australia%2FPerth"
)

weather_cache = None
azaan_cache = None


def http_json(url, timeout=10):
    try:
        req = urllib.request.Request(url, headers={"User-Agent": "HomeMonitor-Companion/1.0"})
        with urllib.request.urlopen(req, timeout=timeout) as r:
            return json.loads(r.read().decode())
    except Exception as e:
        return None


def fetch_weather():
    d = http_json(WEATHER_URL)
    if not d or "current" not in d:
        return None
    c = d["current"]
    try:
        temp10 = int(round(float(c.get("temperature_2m")) * 10))
        hum = int(round(c.get("relative_humidity_2m", 0)))
        wind = int(round(c.get("wind_speed_10m", 0)))
        code = int(c.get("weather_code", 0))
        return (temp10, hum, wind, code)
    except Exception:
        return None


def fetch_azaan():
    now = datetime.datetime.now()
    url = (
        f"https://api.aladhan.com/v1/calendarByCity?city=Perth&country=Australia"
        f"&method=3&month={now.month}&year={now.year}"
    )
    d = http_json(url)
    if not d or not d.get("data"):
        return None
    try:
        t = d["data"][now.day - 1]["timings"]
        strip = lambda s: s.split(" ")[0] if s else ""
        return (
            strip(t["Fajr"]),
            strip(t["Dhuhr"]),
            strip(t["Asr"]),
            strip(t["Maghrib"]),
            strip(t["Isha"]),
        )
    except Exception:
        return None


def background_poller():
    global weather_cache, azaan_cache
    while True:
        w = fetch_weather()
        if w:
            weather_cache = w
            print(f"[+] Weather updated: {w[0]/10:.1f}°C, {w[1]}% hum, {w[2]}km/h")
        z = fetch_azaan()
        if z:
            azaan_cache = z
            print(f"[+] Azaan updated: Fajr {z[0]}, Dhuhr {z[1]}, Asr {z[2]}, Maghrib {z[3]}, Isha {z[4]}")
        time.sleep(600)  # poll every 10 min


def find_serial_port():
    """Scan and find Arduino / CH340 / USB serial ports."""
    ports = serial.tools.list_ports.comports()
    for p in ports:
        desc = (p.description or "").lower()
        hwid = (p.hwid or "").lower()
        # Common USB-UART identifiers: CH340 (1a86:7523), FTDI, CP210x, Arduino (2341)
        if any(k in desc or k in hwid for k in ["ch340", "1a86", "arduino", "usb serial", "cp210", "ftdi"]):
            return p.device
    # Fallback to first available port if only one exists
    if len(ports) == 1:
        return ports[0].device
    return None


def connect_device():
    port = find_serial_port()
    if not port:
        return None
    try:
        s = serial.Serial(port, BAUD, timeout=1.0)
        time.sleep(2.0)  # Wait for Arduino DTR reboot
        s.reset_input_buffer()
        print(f"[*] Connected to HomeMonitor on {port}")
        # Send handshake to switch Arduino to Companion mode (3 screens)
        s.write(b"MODE:COMPANION\n")
        time.sleep(0.05)
        return s
    except Exception as e:
        return None


def get_clock_line():
    t = time.localtime()
    return f"C:{t.tm_hour:02d},{t.tm_min:02d},{t.tm_sec:02d},{t.tm_mday:02d},{t.tm_mon:02d},{t.tm_year%100:02d},ON\n"


def main():
    print("=========================================")
    print("   HomeMonitor Companion (Mini Mode)    ")
    print("   Screens: Clock · Weather · Azaan     ")
    print("=========================================")
    print("[*] Searching for Arduino USB device...")

    threading.Thread(target=background_poller, daemon=True).start()

    ser = None
    last_slow_send = 0

    while True:
        if ser is None:
            ser = connect_device()
            if not ser:
                time.sleep(2)
                continue
            last_slow_send = 0

        try:
            now = time.time()

            # Send slow data (Weather & Azaan) every 5 seconds
            if now - last_slow_send >= 5:
                last_slow_send = now
                if weather_cache:
                    w_line = f"W:{weather_cache[0]},{weather_cache[1]},{weather_cache[2]},{weather_cache[3]}\n"
                    ser.write(w_line.encode())
                    time.sleep(0.05)
                if azaan_cache:
                    z_line = f"Z:{','.join(azaan_cache)}\n"
                    ser.write(z_line.encode())
                    time.sleep(0.05)

            # Send Clock line every 1 second
            ser.write(get_clock_line().encode())
            time.sleep(1.0)

            # Check for incoming lines (e.g. READY after Arduino reset)
            while ser.in_waiting:
                line = ser.readline().decode(errors="ignore").strip()
                if line == "READY":
                    print("[*] Device reset detected, re-sending handshake...")
                    ser.write(b"MODE:COMPANION\n")
                    last_slow_send = 0

        except Exception as e:
            print(f"[!] Connection lost ({e}), reconnecting...")
            try:
                ser.close()
            except Exception:
                pass
            ser = None
            time.sleep(2)


if __name__ == "__main__":
    main()
