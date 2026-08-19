#!/usr/bin/env python3
"""
HomeMonitor PC bridge — polls the homeserver (Pi-hole + Moonraker) and the PC's
own stats, streams display data to the Arduino over USB serial, and serves a
web panel to switch slides. Handles commands: ADBLOCK, PRINT, GCODE, TEMP,
PING, NOTIFY, SYNC.
"""
import sys, time, json, glob, subprocess, queue, threading
import urllib.request
from http.server import BaseHTTPRequestHandler, HTTPServer

SERVER = "100.125.116.96"   # kmaba-server tailnet IP (stable)
WEB_PORT = 8080
DISCORD = ("https://discord.com/api/webhooks/1494662807583723560/"
           "CFqfNB9Mqif49FdWwG6zjo6NQ_7LalfB1S-mOn-VqaXiOnZtR7AxsOpUt2ETkecqO9xG")

screen_cmds = queue.Queue()
serial_cmds = queue.Queue()   # generic serial commands (e.g. virtual-button gestures)

last_pst = None            # previous Moonraker print state, for auto-switch
current_pst = "?"          # most recent print state from poll()

PAGE = """<!doctype html><html><head><meta charset="utf-8"><meta name="viewport" content="width=device-width, initial-scale=1"><title>HomeMonitor</title>
<style>
*{box-sizing:border-box}
body{background:#0b0f14;color:#e6e6e6;font-family:system-ui,-apple-system,sans-serif;margin:0;padding:24px 16px;display:flex;justify-content:center}
.wrap{width:100%;max-width:380px;display:flex;flex-direction:column;align-items:center;gap:12px}
h1{font-size:22px;font-weight:600;margin:0 0 4px}
p{color:#8fa3b8;font-size:13px;margin:0 0 8px;text-align:center}
.btn{width:100%;padding:15px;border:none;border-radius:12px;font-size:17px;cursor:pointer;color:#fff;background:#1c2733;border:1px solid #2f4254;-webkit-tap-highlight-color:transparent}
.btn:active{background:#263647}
.btn.on{background:#2e6b4f;border-color:#4d9c78}
.vbtn{width:150px;height:150px;border-radius:50%;border:3px solid #2f4254;background:#1c2733;color:#fff;font-size:20px;display:flex;align-items:center;justify-content:center;cursor:pointer;user-select:none;-webkit-user-select:none;touch-action:none;-webkit-tap-highlight-color:transparent;transition:background .15s;margin-top:8px}
.vbtn.holding{background:#33405a;border-color:#5a7a9c}
.vbtn.held-long{background:#8a6d1f;border-color:#c9a227}
.vbtn.held-super{background:#1f5a8a;border-color:#2a7ac9}
#status{color:#9fb3c8;font-size:13px;min-height:18px;text-align:center}
@media (min-width:600px){ .btn{width:280px} }
</style></head><body>
<div class="wrap">
<h1>HomeMonitor</h1>
<p>choose the slide shown on the Arduino display</p>
<button class="btn" data-n="0">Pi-hole</button>
<button class="btn" data-n="1">Print</button>
<button class="btn" data-n="2">Weather</button>
<button class="btn" data-n="3">Azaan</button>
<button class="btn" data-n="4">Network</button>
<button class="btn" data-n="5">Clock</button>
<p style="margin-top:16px">virtual button &mdash; click: short, hold 1s: menu, hold 3s: backlight</p>
<div class="vbtn" id="vbtn">HOLD</div>
<div id="status"></div>
</div>
<script>
function go(n){fetch('/screen?n='+n).then(function(r){return r.text()}).then(function(){mark(n)}).catch(function(){document.getElementById('status').textContent='failed'});}
function mark(n){var b=document.querySelectorAll('.btn');b.forEach(function(x,i){x.classList.toggle('on',i==n);});}
var b=document.querySelectorAll('.btn');b.forEach(function(x){x.onclick=function(){go(x.getAttribute('data-n'));};});
function sendBtn(a){fetch('/btn?action='+a).then(function(){var m={short:'short press',long:'long press (menu)',super:'backlight toggle'};document.getElementById('status').textContent=m[a]||a;}).catch(function(){document.getElementById('status').textContent='failed'});}
(function(){
  var el=document.getElementById('vbtn'),fired=0,tL,tS;
  function down(e){if(e.cancelable)e.preventDefault();fired=0;el.classList.add('holding');
    tL=setTimeout(function(){fired=1;el.classList.add('held-long');sendBtn('long');},1000);
    tS=setTimeout(function(){fired=2;el.classList.add('held-super');sendBtn('super');},3000);}
  function up(){clearTimeout(tL);clearTimeout(tS);el.classList.remove('holding','held-long','held-super');if(fired===0)sendBtn('short');}
  el.addEventListener('mousedown',down);el.addEventListener('mouseup',up);el.addEventListener('mouseleave',up);
  el.addEventListener('touchstart',down,{passive:false});el.addEventListener('touchend',up);el.addEventListener('touchcancel',up);
})();
</script></body></html>"""

DL_PAGE = """<!doctype html><html><head><meta charset="utf-8"><meta name="viewport" content="width=device-width, initial-scale=1"><title>kmaba.link/d</title>
<style>
body{background:#0b0f14;color:#eee;font-family:system-ui,sans-serif;margin:0;padding:32px 16px;display:flex;justify-content:center}
.wrap{width:100%;max-width:400px}
h1{font-size:24px;margin:0 0 4px}
p{color:#8fa3b8;font-size:14px;margin:0 0 16px}
.card{background:#1c2733;border:1px solid #2f4254;border-radius:12px;padding:20px}
a.dl{display:block;background:#2e6b4f;color:#fff;text-decoration:none;padding:14px;border-radius:10px;text-align:center;font-size:16px;margin:8px 0}
a.dl:hover{background:#3a8764}
h3{font-size:15px;margin:16px 0 6px;color:#e6e6e6}
pre{background:#0d1117;padding:12px;border-radius:8px;overflow-x:auto;font-size:13px;color:#9fb3c8}
</style></head><body>
<div class="wrap">
<h1>kmaba.link/d</h1>
<p>Companion script for the Arduino display — shows time, weather and azaan.</p>
<div class="card">
<a class="dl" href="/companion.py" download>Download for Linux</a>
<a class="dl" href="/companion.py" download>Download for Windows</a>
<h3>Setup</h3>
<pre>pip install pyserial
python companion.py</pre>
<p style="font-size:13px">Plug the Arduino into a USB port, then run the script. It finds the display automatically and streams time, weather and prayer times.</p>
</div>
</div>
</body></html>"""

COMPANION_PATH = "/home/kmaba/companion.py"

class Web(BaseHTTPRequestHandler):
    def do_GET(self):
        if self.path.startswith("/screen"):
            try:
                n = int(self.path.split("n=")[1])
                screen_cmds.put(n)
                self.send_response(200)
                self.end_headers()
                self.wfile.write(b"ok")
            except Exception:
                self.send_response(400)
                self.end_headers()
            return
        if self.path.startswith("/btn"):
            try:
                action = self.path.split("action=")[1].split("&")[0]
                if action in ("short", "long", "super"):
                    serial_cmds.put("BTN:" + action)
                self.send_response(200)
                self.end_headers()
                self.wfile.write(b"ok")
            except Exception:
                self.send_response(400)
                self.end_headers()
            return
        if self.path.startswith("/dl"):
            self.send_response(200)
            self.send_header("Content-Type", "text/html")
            self.send_header("Cache-Control", "no-store, no-cache, must-revalidate")
            self.end_headers()
            self.wfile.write(DL_PAGE.encode())
            return
        if self.path.startswith("/companion.py"):
            try:
                with open(COMPANION_PATH) as f:
                    content = f.read()
                self.send_response(200)
                self.send_header("Content-Type", "text/x-python")
                self.send_header("Content-Disposition", 'attachment; filename="companion.py"')
                self.end_headers()
                self.wfile.write(content.encode())
            except Exception:
                self.send_response(404)
                self.end_headers()
            return
        self.send_response(200)
        self.send_header("Content-Type", "text/html")
        self.send_header("Cache-Control", "no-store, no-cache, must-revalidate")
        self.end_headers()
        self.wfile.write(PAGE.encode())

    def log_message(self, *a):
        pass

def start_web():
    srv = HTTPServer(("0.0.0.0", WEB_PORT), Web)
    threading.Thread(target=srv.serve_forever, daemon=True).start()
    print("web panel: http://<this-pc>:8080", flush=True)

GCODE_SCRIPTS = {
    "HOME":    "G28",
    "EXTRUDE": "G91\nG1 E10 F300\nG90",
    "RETRACT": "G91\nG1 E-10 F300\nG90",
}
TEMP_SCRIPTS = {
    "PLA":  "M104 S210\nM140 S60",
    "PETG": "M104 S240\nM140 S70",
    "OFF":  "M104 S0\nM140 S0",
}

def serial_ports():
    return sorted(glob.glob("/dev/ttyUSB*") + glob.glob("/dev/ttyACM*"))

def http_json(url, timeout=5):
    try:
        with urllib.request.urlopen(url, timeout=timeout) as r:
            return json.loads(r.read().decode())
    except Exception:
        return None

def http_post(url, body):
    try:
        req = urllib.request.Request(url, data=body.encode(),
                                     headers={"Content-Type": "application/json"}, method="POST")
        with urllib.request.urlopen(req, timeout=5) as r:
            return r.read().decode()[:60]
    except Exception as e:
        return "ERR %s" % e

def post_discord(msg):
    try:
        req = urllib.request.Request(DISCORD, data=json.dumps({"content": msg}).encode(),
                                     headers={"Content-Type": "application/json"}, method="POST")
        with urllib.request.urlopen(req, timeout=8) as r:
            return r.status
    except Exception as e:
        return "ERR %s" % e

# ---- slow data: weather + azaan (prayer) times ----
WEATHER_URL = ("https://api.open-meteo.com/v1/forecast"
               "?latitude=-31.978&longitude=115.951"
               "&current=temperature_2m,relative_humidity_2m,wind_speed_10m,weather_code"
               "&timezone=Australia%2FPerth")

weather_cache = None
azaan_cache = None

def fetch_weather():
    d = http_json(WEATHER_URL, timeout=10)
    if not d or "current" not in d:
        return None
    c = d["current"]
    try:
        temp = int(round(float(c.get("temperature_2m")) * 10))   # degC * 10
        hum = int(round(c.get("relative_humidity_2m", 0)))
        wind = int(round(c.get("wind_speed_10m", 0)))
        code = int(c.get("weather_code", 0))
        return (temp, hum, wind, code)
    except Exception:
        return None

def fetch_azaan():
    import datetime
    now = datetime.datetime.now()
    url = ("https://api.aladhan.com/v1/calendarByCity?city=Perth&country=Australia"
           "&method=3&month=%d&year=%d" % (now.month, now.year))
    d = http_json(url, timeout=10)
    if not d or not d.get("data"):
        return None
    try:
        t = d["data"][now.day - 1]["timings"]
        strip = lambda s: s.split(" ")[0] if s else ""
        return (strip(t["Fajr"]), strip(t["Dhuhr"]), strip(t["Asr"]),
                strip(t["Maghrib"]), strip(t["Isha"]))
    except Exception:
        return None

def poll_slow():
    global weather_cache, azaan_cache
    w = fetch_weather()
    if w:
        weather_cache = w
        print("weather:", w, flush=True)
    z = fetch_azaan()
    if z:
        azaan_cache = z
        print("azaan:", z, flush=True)

def slow_thread():
    while True:
        try:
            poll_slow()
        except Exception as e:
            print("slow data error: %s" % e, flush=True)
        time.sleep(600)   # refresh every 10 min

def poll():
    global current_pst
    out = []
    s = http_json("http://%s/api/stats/summary" % SERVER)
    if s and "queries" in s:
        q = s["queries"]
        out.append("P:%d,%d,%d" % (q["total"], q["blocked"], int(round(q["percent_blocked"]))))
    blk = http_json("http://%s/api/dns/blocking" % SERVER)
    state = "ON" if (blk and blk.get("blocking") == "enabled") else "OFF"
    mr = http_json("http://%s:7125/printer/objects/query?print_stats&display_status=progress&extruder=temperature,target&heater_bed=temperature,target" % SERVER)
    if mr and "result" in mr:
        st = mr["result"]["status"]
        ps = st.get("print_stats", {})
        pst = ps.get("state", "?")
        current_pst = pst
        fname = ps.get("filename", "") or ""
        elapsed = int(ps.get("print_duration", 0) / 60)
        prog = int(round(st.get("display_status", {}).get("progress", 0) * 100))
        ex = st.get("extruder", {})
        et = int(round(ex.get("temperature", 0)))
        etar = int(round(ex.get("target", 0)))
        bd = st.get("heater_bed", {})
        bt = int(round(bd.get("temperature", 0)))
        btar = int(round(bd.get("target", 0)))
        out.append("R:%s,%d,%d,%d,%d,%d,%d,%s" % (pst, prog, et, etar, bt, btar, elapsed, fname))
    out.append("N:SRV,0,%s" % SERVER)
    if weather_cache:
        out.append("W:%s,%d,%d,%d" % weather_cache)
    if azaan_cache:
        out.append("Z:%s" % ",".join(azaan_cache))
    t = time.localtime()
    out.append("C:%02d,%02d,%02d,%02d,%02d,%02d,%s" % (
        t.tm_hour, t.tm_min, t.tm_sec, t.tm_mday, t.tm_mon, t.tm_year % 100, state))
    return out

def send_all(ser):
    for line in poll():
        print("TX:", line, flush=True)
        ser.write((line + "\n").encode())
        time.sleep(0.05)        # let the Uno drain its 64-byte RX buffer before the next line

def check_print_switch(ser):
    """Auto-switch the LCD to the Print slide when a print starts."""
    global last_pst, current_pst
    if current_pst == "printing" and last_pst != "printing":
        time.sleep(0.05)
        ser.write(b"SCREEN:1\n")
        ser.flush()
        print("print started -> auto-switch to Print slide", flush=True)
    last_pst = current_pst

def serial_ports():
    return sorted(glob.glob("/dev/ttyUSB*") + glob.glob("/dev/ttyACM*"))

def connect_monitor(fixed=None):
    """Open the port that the HomeMonitor replies READY on (USB hot-plug aware)."""
    import serial
    candidates = [fixed] if fixed else serial_ports()
    for p in candidates:
        if not p:
            continue
        try:
            s = serial.Serial(p, 115200, timeout=1.0)
        except Exception:
            continue
        try:
            time.sleep(2.2)                      # let the Uno boot after DTR reset
            s.reset_input_buffer()
            t0 = time.time()
            buf = b""
            while time.time() - t0 < 3:
                d = s.read(256)
                if d:
                    buf += d
                    if b"READY" in buf:
                        print("monitor found on %s" % p, flush=True)
                        return s
        except Exception:
            pass
        try:
            s.close()
        except Exception:
            pass
    return None

def main():
    global last_pst
    start_web()
    threading.Thread(target=slow_thread, daemon=True).start()
    fixed = sys.argv[1] if len(sys.argv) > 1 else None
    ser = None
    last = 0
    while True:
        now = time.time()
        if ser is None:
            ser = connect_monitor(fixed)
            if ser:
                print("bridge up, server %s" % SERVER, flush=True)
                ser.write(b"MODE:FULL\n")
                time.sleep(0.05)
                send_all(ser)
                last = now
                check_print_switch(ser)          # switch to Print if already printing
            else:
                time.sleep(3)                    # nothing plugged in yet; keep listening
            continue

        try:
            while not screen_cmds.empty():       # web panel slide switch
                n = screen_cmds.get()
                print("web slide ->", n, flush=True)
                ser.write(("SCREEN:%d\n" % n).encode())

            while not serial_cmds.empty():       # web panel virtual button
                cmd = serial_cmds.get()
                print("web btn ->", cmd, flush=True)
                ser.write((cmd + "\n").encode())

            if now - last >= 5:
                last = now
                send_all(ser)
                check_print_switch(ser)

            if ser.in_waiting:
                line = ser.readline().decode().strip()
                print("RX:", line, flush=True)
                if line == "ADBLOCK":
                    blk = http_json("http://%s/api/dns/blocking" % SERVER)
                    enable = "true" if (blk and blk.get("blocking") == "disabled") else "false"
                    http_post("http://%s/api/dns/blocking" % SERVER, '{"blocking":%s}' % enable)
                elif line.startswith("ADBLOCK:ON"):
                    http_post("http://%s/api/dns/blocking" % SERVER, '{"blocking":true}')
                elif line.startswith("ADBLOCK:OFF"):
                    parts = line.split(":")
                    secs = parts[2] if len(parts) > 2 else None
                    body = '{"blocking":false}'
                    if secs and secs.isdigit() and int(secs) > 0:
                        body = '{"blocking":false,"timer":%d}' % int(secs)
                    http_post("http://%s/api/dns/blocking" % SERVER, body)
                elif line.startswith("PRINT:"):
                    action = line.split(":", 1)[1].lower()
                    r = http_post("http://%s:7125/printer/print/%s" % (SERVER, action), "{}")
                    print("print %s: %s" % (action, r), flush=True)
                elif line.startswith("GCODE:"):
                    key = line.split(":", 1)[1]
                    script = GCODE_SCRIPTS.get(key)
                    if script:
                        r = http_post("http://%s:7125/printer/gcode/script" % SERVER,
                                      json.dumps({"script": script}))
                        print("gcode %s: %s" % (key, r), flush=True)
                        ser.write(("MSG:%s|sent\n" % key).encode())
                elif line.startswith("TEMP:"):
                    key = line.split(":", 1)[1]
                    script = TEMP_SCRIPTS.get(key)
                    if script:
                        r = http_post("http://%s:7125/printer/gcode/script" % SERVER,
                                      json.dumps({"script": script}))
                        print("temp %s: %s" % (key, r), flush=True)
                        ser.write(("MSG:Temp %s|sent\n" % key).encode())
                elif line == "PING":
                    blk = http_json("http://%s/api/dns/blocking" % SERVER)
                    if blk:
                        ser.write(b"MSG:Server OK|reachable\n")
                        print("ping: ok", flush=True)
                    else:
                        ser.write(b"MSG:Server down|no response\n")
                        print("ping: down", flush=True)
                elif line == "NOTIFY":
                    code = post_discord("HomeMonitor: command sent from the LCD monitor")
                    if code == 204:
                        ser.write(b"MSG:Discord|sent\n")
                    else:
                        ser.write(b"MSG:Discord|failed\n")
                    print("discord:", code, flush=True)
                elif line == "SYNC":
                    send_all(ser)
                    ser.write(b"MSG:Sync|done\n")
                elif line == "READY":
                    # Arduino rebooted (e.g. reset button) while USB stayed up;
                    # let it finish booting, then re-send data + switch to Print.
                    last_pst = None
                    time.sleep(0.9)
                    send_all(ser)
                    check_print_switch(ser)
            time.sleep(0.05)
        except Exception as e:
            print("serial error, reconnecting: %s" % e, flush=True)
            last_pst = None                 # re-check print state after reconnect
            try:
                ser.close()
            except Exception:
                pass
            ser = None
            time.sleep(2)

if __name__ == "__main__":
    main()
