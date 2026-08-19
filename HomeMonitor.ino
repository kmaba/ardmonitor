/*
 * HomeMonitor (PC-driven) — Uno + 16x2 I2C LCD (0x27)
 * Buzzer D9, button D8 (button -> GND, INPUT_PULLUP, active LOW).
 *
 * PC streams data (P:/R:/N:/C:/S:/MSG:). Arduino -> PC:
 *   ADBLOCK:ON | ADBLOCK:OFF[:<sec>]
 *   PRINT:PAUSE | PRINT:RESUME | PRINT:CANCEL
 *   GCODE:HOME | GCODE:EXTRUDE | GCODE:RETRACT
 *   TEMP:PLA | TEMP:PETG | TEMP:OFF
 *   PING | NOTIFY | SYNC
 * UI: short press cycles screens; hold opens context menu (bar fills on the
 * BOTTOM row only); in menu short-press moves, long-press selects.
 */
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

LiquidCrystal_I2C lcd(0x27, 16, 2);

const int BUTTON_PIN = 8;    // button -> GND, INPUT_PULLUP, active LOW
const int BUZZER_PIN = 9;

// ---------------- sounds (very quiet, monotone, 25% duty) ----------------
void quietTone(unsigned int freq, unsigned int ms) {
  unsigned long period = 1000000UL / freq;
  unsigned long on = period * 25 / 100;
  unsigned long off = period - on;
  unsigned long end = millis() + ms;
  while ((long)(end - millis()) > 0) {
    digitalWrite(BUZZER_PIN, HIGH);
    delayMicroseconds(on);
    digitalWrite(BUZZER_PIN, LOW);
    delayMicroseconds(off);
  }
  digitalWrite(BUZZER_PIN, LOW);
}
void beep()       { quietTone(440, 40); }
void beepSelect() { quietTone(440, 110); }

// ---------------- screens ----------------
enum { SCR_PIHOLE, SCR_PRINT, SCR_WEATHER, SCR_AZAAN, SCR_NET, SCR_CLOCK, SCR_COUNT };
int screen = SCR_CLOCK;
int prevScreen = -1;
bool companionMode = false;
const char* screenNames[SCR_COUNT] = { "Pi-hole", "Print", "Weather", "Azaan", "Network", "Clock" };

char scrL1[SCR_COUNT][17];
char scrL2[SCR_COUNT][17];
bool scrHas[SCR_COUNT] = { false, false, false, false, false, false };

char printState[10] = "standby";
char printFname[21] = "";
int  pPct = 0, pE = 0, pET = 0, pB = 0, pBT = 0, pElapsed = 0;

// live clock state (from C: line, ticks locally)
byte clkH = 0, clkM = 0, clkS = 0;
int  clkDay = 0, clkMon = 0, clkYr = 0;
char clkBlk[4] = "?";
unsigned long clkSyncMs = 0;
bool clockValid = false;

// pi-hole stats (requests + blocked %)
long piholeQueries = 0;
long piholeBlocked = 0;
int piholePct = 0;
// weather (temp stored as degC * 10, e.g. 234 = 23.4C)
int wTemp10 = 0;
int wHum = 0, wWind = 0, wCode = 0;
// azaan prayer times "HH:MM" (24h)
char az[5][6];

// ---------------- menu ----------------
typedef struct { const char* label; const char* cmd; } MenuItem;

MenuItem piholeMenu[] = {
  { "Back", "" },
  { "Adblock ON", "ADBLOCK:ON" },
  { "Adblock OFF", "ADBLOCK:OFF:0" },
  { "Off 10s", "ADBLOCK:OFF:10" },
  { "Off 30s", "ADBLOCK:OFF:30" },
  { "Off 1m", "ADBLOCK:OFF:60" },
  { "Off 5m", "ADBLOCK:OFF:300" },
  { "Off 1h", "ADBLOCK:OFF:3600" },
};
MenuItem printIdle[] = {
  { "Back", "" },
  { "Home", "GCODE:HOME" },
  { "Extrude 10mm", "GCODE:EXTRUDE" },
  { "Retract 10mm", "GCODE:RETRACT" },
  { "Preheat PLA", "TEMP:PLA" },
  { "Preheat PETG", "TEMP:PETG" },
  { "Cooldown", "TEMP:OFF" },
};
MenuItem printBusy[] = {
  { "Back", "" },
  { "Pause", "PRINT:PAUSE" },
  { "Cancel", "PRINT:CANCEL" },
  { "Cooldown", "TEMP:OFF" },
};
MenuItem printPaused[] = {
  { "Back", "" },
  { "Resume", "PRINT:RESUME" },
  { "Cancel", "PRINT:CANCEL" },
  { "Cooldown", "TEMP:OFF" },
};
MenuItem netMenu[] = {
  { "Back", "" },
  { "Ping server", "PING" },
  { "Notify Discord", "NOTIFY" },
};
MenuItem clockMenu[] = {
  { "Back", "" },
  { "Sync now", "SYNC" },
};
MenuItem simpleMenu[] = {
  { "Back", "" },
  { "Sync now", "SYNC" },
};

MenuItem* menuItems = NULL;
const char* menuTitle = "";
int menuLen = 0;
int menuIdx = 0;

// ---------------- UI state ----------------
enum { ST_SCREEN, ST_MENU } state = ST_SCREEN;

char msg1[17] = "";
char msg2[17] = "";
unsigned long msgUntil = 0;
const unsigned long MSG_MS = 2500;

// ---------------- button (debounced edges, active LOW) ----------------
const unsigned long DEBOUNCE = 50;
const unsigned long LONG_MS = 900;
const unsigned long SUPER_LONG_MS = 3000;

bool redraw = true;                 // force full redraw after a screen switch / data change

bool btnPrev = false;
bool btnNow = false;
unsigned long lastBtnChange = 0;
unsigned long btnDownMs = 0;
bool longFired = false;
bool superLongFired = false;
bool backlightOn = true;
int lastBarCells = -1;

void shortPressAction() {
  beep();
  redraw = true;
  if (state == ST_MENU) {
    menuIdx = (menuIdx + 1) % menuLen;
  } else if (companionMode) {
    // Companion mode: only cycle Weather, Azaan, Clock
    if (screen == SCR_WEATHER) screen = SCR_AZAAN;
    else if (screen == SCR_AZAAN) screen = SCR_CLOCK;
    else screen = SCR_WEATHER;
  } else {
    screen = (screen + 1) % SCR_COUNT;
  }
}

void selectOption() {
  const char* cmd = menuItems[menuIdx].cmd;
  if (strcmp(cmd, "") == 0) {                 // Back
    state = ST_SCREEN;
  } else {
    Serial.println(cmd);
    state = ST_SCREEN;
    strncpy(msg1, menuItems[menuIdx].label, 16); msg1[16] = 0;
    strcpy(msg2, "sent to PC");
    msgUntil = millis() + MSG_MS;
  }
  redraw = true;
}

void enterPrintMenu() {
  if (strcmp(printState, "printing") == 0)      { menuItems = printBusy; menuLen = 4; }
  else if (strcmp(printState, "paused") == 0)   { menuItems = printPaused; menuLen = 4; }
  else                                          { menuItems = printIdle; menuLen = 7; }
  menuTitle = "Print";
}

void longPressAction() {
  beepSelect();
  redraw = true;
  if (state == ST_MENU) {
    selectOption();
  } else {
    state = ST_MENU;
    menuIdx = 0;
    switch (screen) {
      case SCR_PIHOLE:
        menuItems = piholeMenu; menuLen = sizeof(piholeMenu) / sizeof(MenuItem); menuTitle = "Pi-hole"; break;
      case SCR_PRINT:
        enterPrintMenu(); break;
      case SCR_NET:
        menuItems = netMenu; menuLen = sizeof(netMenu) / sizeof(MenuItem); menuTitle = "Network"; break;
      case SCR_CLOCK:
        menuItems = clockMenu; menuLen = sizeof(clockMenu) / sizeof(MenuItem); menuTitle = "Clock"; break;
      case SCR_WEATHER:
      case SCR_AZAAN:
        menuItems = simpleMenu; menuLen = sizeof(simpleMenu) / sizeof(MenuItem); menuTitle = screenNames[screen]; break;
    }
  }
}

void toggleBacklight() {
  backlightOn = !backlightOn;
  if (backlightOn) lcd.backlight(); else lcd.noBacklight();
  beep();
  state = ST_SCREEN;
  redraw = true;
}

void updateButton() {
  unsigned long now = millis();
  bool raw = (digitalRead(BUTTON_PIN) == LOW);
  if (raw != btnPrev) {
    btnPrev = raw;
    lastBtnChange = now;
  }
  if (now - lastBtnChange >= DEBOUNCE) {
    if (raw != btnNow) {
      btnNow = raw;
      if (btnNow) {
        btnDownMs = now;
        longFired = false;
        superLongFired = false;
        lastBarCells = -1;
      } else if (!longFired) {
        shortPressAction();
      }
    }
  }
  if (btnNow && !longFired && (now - btnDownMs) >= LONG_MS) {
    longFired = true;
    longPressAction();
  }
  if (btnNow && !superLongFired && (now - btnDownMs) >= SUPER_LONG_MS) {
    superLongFired = true;
    toggleBacklight();
  }
}

// ---------------- LCD ----------------
char lineCache[2][17];

void lcdLine(int row, const char* s) {
  char line[17];
  strncpy(line, s, 16);
  line[16] = 0;
  for (int i = strlen(line); i < 16; i++) line[i] = ' ';
  line[16] = 0;
  if (!redraw && strncmp(line, lineCache[row], 16) == 0) return;
  strncpy(lineCache[row], line, 16);
  lcd.setCursor(0, row);
  lcd.print(line);
}

// custom bar glyphs: solid fill columns + mesh-hatch for the unfilled track
void createBarChars() {
  for (int fill = 0; fill <= 5; fill++) {
    byte bm[8];
    for (int r = 0; r < 8; r++) {
      byte v = 0;
      for (int c = 0; c < 5; c++) {
        bool on = (c < fill) || ((c + r) % 2 == 0);   // filled=solid, rest=mesh
        if (on) v |= (1 << (4 - c));
      }
      bm[r] = v;
    }
    lcd.createChar(fill, bm);
  }
}

// ---- big 3x2 digits (8 segment glyphs, HD44780 slots 0-7) ----
byte segLT[8]  = { B00111, B11111, B11111, B11111, B11111, B11111, B11111, B11111 };
byte segUB[8]  = { B11111, B11111, B11111, B00000, B00000, B00000, B00000, B00000 };
byte segRT[8]  = { B11100, B11111, B11111, B11111, B11111, B11111, B11111, B11111 };
byte segLL[8]  = { B11111, B11111, B11111, B11111, B11111, B11111, B11111, B00111 };
byte segLB[8]  = { B00000, B00000, B00000, B00000, B00000, B11111, B11111, B11111 };
byte segLR[8]  = { B11111, B11111, B11111, B11111, B11111, B11111, B11111, B11100 };
byte segUMB[8] = { B11111, B11111, B11111, B00000, B00000, B00000, B11111, B11111 };
byte segLMB[8] = { B11111, B00000, B00000, B00000, B00000, B11111, B11111, B11111 };

int glyphMode = 0;   // 0 = bar glyphs (print), 1 = big-digit glyphs (clock)

void createBigDigitChars() {
  lcd.createChar(0, segLT);
  lcd.createChar(1, segUB);
  lcd.createChar(2, segRT);
  lcd.createChar(3, segLL);
  lcd.createChar(4, segLB);
  lcd.createChar(5, segLR);
  lcd.createChar(6, segUMB);
  lcd.createChar(7, segLMB);
}

void ensureBarGlyphs() {
  if (glyphMode == 0) return;
  createBarChars();
  glyphMode = 0;
}

void ensureBigDigitGlyphs() {
  if (glyphMode == 1) return;
  createBigDigitChars();
  glyphMode = 1;
}

void drawBigDigit(int col, int d) {
  // clear the digit cell first so partial digits (1,4,7,9) leave no stale blocks
  lcd.setCursor(col, 0); lcd.print("   ");
  lcd.setCursor(col, 1); lcd.print("   ");
  switch (d) {
    case 0:
      lcd.setCursor(col, 0); lcd.write(0); lcd.write(1); lcd.write(2);
      lcd.setCursor(col, 1); lcd.write(3); lcd.write(4); lcd.write(5); break;
    case 1:
      lcd.setCursor(col + 1, 0); lcd.write(2);
      lcd.setCursor(col + 1, 1); lcd.write(5); break;
    case 2:
      lcd.setCursor(col, 0); lcd.write(6); lcd.write(6); lcd.write(2);
      lcd.setCursor(col, 1); lcd.write(3); lcd.write(7); lcd.write(7); break;
    case 3:
      lcd.setCursor(col, 0); lcd.write(6); lcd.write(6); lcd.write(2);
      lcd.setCursor(col, 1); lcd.write(7); lcd.write(7); lcd.write(5); break;
    case 4:
      lcd.setCursor(col, 0); lcd.write(0); lcd.write(4); lcd.write(2);
      lcd.setCursor(col + 2, 1); lcd.write(5); break;
    case 5:
      lcd.setCursor(col, 0); lcd.write(0); lcd.write(6); lcd.write(6);
      lcd.setCursor(col, 1); lcd.write(7); lcd.write(7); lcd.write(5); break;
    case 6:
      lcd.setCursor(col, 0); lcd.write(0); lcd.write(6); lcd.write(6);
      lcd.setCursor(col, 1); lcd.write(3); lcd.write(7); lcd.write(5); break;
    case 7:
      lcd.setCursor(col, 0); lcd.write(1); lcd.write(1); lcd.write(2);
      lcd.setCursor(col + 2, 1); lcd.write(5); break;
    case 8:
      lcd.setCursor(col, 0); lcd.write(0); lcd.write(6); lcd.write(2);
      lcd.setCursor(col, 1); lcd.write(3); lcd.write(7); lcd.write(5); break;
    case 9:
      lcd.setCursor(col, 0); lcd.write(0); lcd.write(6); lcd.write(2);
      lcd.setCursor(col + 2, 1); lcd.write(5); break;
  }
}

void formatHM(long mins, char* out) {
  if (mins < 0) mins = 0;
  snprintf(out, 8, "%ld:%02ld", mins / 60, mins % 60);
}

// slow scroll window for the filename (half-speed marquee)
void fnameWindow(const char* text, int win, char* out) {
  int len = strlen(text);
  static int sp = 0;
  static unsigned long lastMove = 0;
  if (len > win) {
    if (millis() - lastMove >= 600) {
      lastMove = millis();
      sp++;
      if (sp > len - win) sp = 0;
    }
  } else {
    sp = 0;
  }
  for (int i = 0; i < win; i++) {
    int idx = sp + i;
    out[i] = (idx < len) ? text[idx] : ' ';
  }
  out[win] = 0;
}

// print screen: custom-glyph bar (solid+mesh) + pct on top, remaining + filename below
void renderPrint() {
  static int lastPx = -1, lastPct = -1;
  const int BAR_CHARS = 11;
  int fillPx = (long)pPct * BAR_CHARS * 5 / 100;
  if (redraw || fillPx != lastPx || pPct != lastPct) {
    lastPx = fillPx;
    lastPct = pPct;
    lcd.setCursor(0, 0);
    for (int i = 0; i < BAR_CHARS; i++) {
      int px = fillPx - i * 5;
      lcd.write((uint8_t)(px >= 5 ? 5 : (px > 0 ? px : 0)));
    }
    lcd.write(' ');
    char pbuf[8];
    snprintf(pbuf, 8, "%3d%%", pPct);
    lcd.print(pbuf);
  }

  char l2[17];
  int len = snprintf(l2, 17, "R ");
  char r[8];
  long rem = 0;
  if (pPct > 0) rem = (long)pElapsed * (100 - pPct) / pPct;
  formatHM(rem, r);
  len += snprintf(l2 + len, 17 - len, "%s ", r);
  int win = 16 - len;
  if (win < 3) win = 3;
  fnameWindow(printFname, win, l2 + len);
  lcdLine(1, l2);
}

void currentTime(byte& h, byte& m, byte& s) {
  unsigned long elapsed = (millis() - clkSyncMs) / 1000;
  long ss = clkS + (long)(elapsed % 60);
  long mm = clkM + (long)(elapsed / 60) + ss / 60;
  long hh = clkH + mm / 60;
  s = (byte)(ss % 60);
  m = (byte)(mm % 60);
  h = (byte)(hh % 24);
}

// live ticking big HH:MM clock (12-hour, no AM/PM)
void renderClock(bool first) {
  byte h, m, s;
  currentTime(h, m, s);

  byte h12, md;
  if (!clockValid) {
    h12 = 0; md = 0;                    // 00:00 placeholder until synced
  } else {
    h12 = h % 12; if (h12 == 0) h12 = 12;
    md = m;
  }

  bool colonOn = ((millis() / 500) % 2) == 0;
  static byte lastH = 255, lastM = 255;
  static bool lastColon = false;

  if (first) {
    // just entered the clock screen: clear once, then force full redraw
    lcd.setCursor(0, 0); lcd.print("                ");
    lcd.setCursor(0, 1); lcd.print("                ");
    lastH = 255; lastM = 255; lastColon = !colonOn;
  }

  // incremental: only update the parts that changed (no full-screen clear)
  if (h12 != lastH) {
    lastH = h12;
    drawBigDigit(1, h12 / 10);
    drawBigDigit(5, h12 % 10);
  }
  if (md != lastM) {
    lastM = md;
    drawBigDigit(9, md / 10);
    drawBigDigit(13, md % 10);
  }
  if (colonOn != lastColon) {
    lastColon = colonOn;
    lcd.setCursor(8, 0); lcd.print(colonOn ? ":" : " ");
    lcd.setCursor(8, 1); lcd.print(colonOn ? ":" : " ");
  }
}

// ---- weather ----
const char* weatherDesc(int code) {
  if (code == 0) return "Clear sky";
  if (code == 1) return "Mostly clear";
  if (code == 2) return "Partly cloudy";
  if (code == 3) return "Overcast";
  if (code >= 45 && code <= 48) return "Fog";
  if (code >= 51 && code <= 57) return "Drizzle";
  if (code >= 61 && code <= 67) return "Rain";
  if (code >= 71 && code <= 77) return "Snow";
  if (code >= 80 && code <= 82) return "Showers";
  if (code >= 95 && code <= 99) return "Thunderstorm";
  return "?";
}

void renderWeather() {
  if (!scrHas[SCR_WEATHER]) {
    lcdLine(0, "Weather");
    lcdLine(1, "waiting PC...");
    return;
  }
  char l1[17];
  snprintf(l1, 17, "%d.%dC %d%% %dk/h", wTemp10 / 10, abs(wTemp10 % 10), wHum, wWind);
  lcdLine(0, l1);
  lcdLine(1, weatherDesc(wCode));
}

// ---- azaan: next prayer + countdown ----
void renderAzaan() {
  if (!scrHas[SCR_AZAAN]) {
    lcdLine(0, "Azaan");
    lcdLine(1, "waiting PC...");
    return;
  }
  byte h, m, s;
  currentTime(h, m, s);
  int nowMin = h * 60 + m;

  const char* names[5] = { "Fajr", "Dhuhr", "Asr", "Maghrib", "Isha" };
  int tm[5];
  for (int i = 0; i < 5; i++) {
    tm[i] = ((az[i][0]-'0')*10 + (az[i][1]-'0')) * 60 + ((az[i][3]-'0')*10 + (az[i][4]-'0'));
  }
  int next = -1, nextMin = 0;
  for (int i = 0; i < 5; i++) {
    if (tm[i] > nowMin) { next = i; nextMin = tm[i]; break; }
  }
  if (next < 0) { next = 0; nextMin = tm[0] + 24*60; }   // tomorrow's Fajr

  int diff = nextMin - nowMin;
  char l1[17], l2[17];
  snprintf(l1, 17, "%s %s", names[next], az[next]);
  if (diff >= 60) snprintf(l2, 17, "in %dh %02dm", diff / 60, diff % 60);
  else snprintf(l2, 17, "in %d min", diff);
  lcdLine(0, l1);
  lcdLine(1, l2);
}

// ---- pi-hole: requests left, blocked % right ----
void renderPihole() {
  if (!scrHas[SCR_PIHOLE]) {
    lcdLine(0, "Pi-hole");
    lcdLine(1, "waiting PC...");
    return;
  }
  char l1[17], l2[17];
  snprintf(l1, 17, "Requests   Block");
  snprintf(l2, 17, "%-10ld %4d%%", piholeQueries, piholePct);
  lcdLine(0, l1);
  lcdLine(1, l2);
}

void renderScreen() {
  if (!scrHas[screen]) {
    lcdLine(0, screenNames[screen]);
    lcdLine(1, "waiting PC...");
    return;
  }
  lcdLine(0, scrL1[screen]);
  lcdLine(1, scrL2[screen]);
}

void renderMenu() {
  char buf[17];
  snprintf(buf, 17, "%s %d/%d", menuTitle, menuIdx + 1, menuLen);
  lcdLine(0, buf);
  snprintf(buf, 17, "> %s", menuItems[menuIdx].label);
  lcdLine(1, buf);
}

// bottom-row-only press bar (white squares); top row stays untouched
void renderPressBar() {
  unsigned long dur = millis() - btnDownMs;
  int cells = (int)((long)dur * 16 / LONG_MS);
  if (cells > 16) cells = 16;
  if (cells < 0) cells = 0;
  if (cells != lastBarCells) {
    lastBarCells = cells;
    char bar[17];
    for (int i = 0; i < 16; i++) bar[i] = (i < cells) ? (char)0xFF : ' ';
    bar[16] = 0;
    lcdLine(1, bar);
  }
}

// PC screen removed; this is the main render dispatch
void render() {
  if (btnNow && !longFired) {
    renderPressBar();
    redraw = false;
    prevScreen = -1;
    return;
  }
  if (msgUntil && millis() < msgUntil) {
    lcdLine(0, msg1);
    lcdLine(1, msg2);
    redraw = false;
    prevScreen = -1;
    return;
  }
  if (state == ST_MENU) {
    renderMenu();
    redraw = false;
    prevScreen = -1;
    return;
  }
  if (screen == SCR_PRINT) {
    ensureBarGlyphs();
    renderPrint();
    redraw = false;
    prevScreen = screen;
    return;
  }
  if (screen == SCR_PIHOLE) {
    renderPihole();
    redraw = false;
    prevScreen = screen;
    return;
  }
  if (screen == SCR_WEATHER) {
    renderWeather();
    redraw = false;
    prevScreen = screen;
    return;
  }
  if (screen == SCR_AZAAN) {
    renderAzaan();
    redraw = false;
    prevScreen = screen;
    return;
  }
  if (screen == SCR_CLOCK) {
    ensureBigDigitGlyphs();
    renderClock(screen != prevScreen);
    redraw = false;
    prevScreen = screen;
    return;
  }
  renderScreen();
  redraw = false;
  prevScreen = screen;
}

// ---------------- serial protocol ----------------
void handleLine(String s) {
  s.trim();
  if (s.length() == 0) return;
  redraw = true;

  if (s.startsWith("MODE:COMPANION")) {
    companionMode = true;
    if (screen != SCR_WEATHER && screen != SCR_AZAAN && screen != SCR_CLOCK) {
      screen = SCR_CLOCK;
    }
    return;
  }
  else if (s.startsWith("MODE:FULL")) {
    companionMode = false;
    return;
  }

  if (s.startsWith("P:")) {
    companionMode = false;
    int c1 = s.indexOf(',', 2);
    int c2 = s.indexOf(',', c1 + 1);
    if (c1 < 0 || c2 < 0) return;
    long q = s.substring(2, c1).toInt();
    long b = s.substring(c1 + 1, c2).toInt();
    int pct = s.substring(c2 + 1).toInt();
    piholeQueries = q;
    piholeBlocked = b;
    piholePct = pct;
    scrHas[SCR_PIHOLE] = true;
  }
  else if (s.startsWith("R:")) {
    int c[7];
    int pos = 2;
    bool ok = true;
    for (int i = 0; i < 7; i++) {
      c[i] = s.indexOf(',', pos);
      if (c[i] < 0) { ok = false; break; }
      pos = c[i] + 1;
    }
    if (!ok) return;
    String st = s.substring(2, c[0]);
    st.toCharArray(printState, sizeof(printState));
    pPct = s.substring(c[0] + 1, c[1]).toInt();
    pE   = s.substring(c[1] + 1, c[2]).toInt();
    pET  = s.substring(c[2] + 1, c[3]).toInt();
    pB   = s.substring(c[3] + 1, c[4]).toInt();
    pBT  = s.substring(c[4] + 1, c[5]).toInt();
    pElapsed = s.substring(c[5] + 1, c[6]).toInt();
    String fname = s.substring(c[6] + 1);
    fname.toCharArray(printFname, sizeof(printFname));
    scrHas[SCR_PRINT] = true;
  }
  else if (s.startsWith("N:")) {
    int c1 = s.indexOf(',', 2);
    int c2 = s.indexOf(',', c1 + 1);
    if (c1 < 0 || c2 < 0) return;
    String ssid = s.substring(2, c1);
    int rssi = s.substring(c1 + 1, c2).toInt();
    String ip = s.substring(c2 + 1);
    snprintf(scrL1[SCR_NET], 17, "WiFi %s %d", ssid.c_str(), rssi);
    snprintf(scrL2[SCR_NET], 17, "%s", ip.c_str());
    scrHas[SCR_NET] = true;
  }
  else if (s.startsWith("C:")) {
    int idx[7];
    int pos = 2;
    bool ok = true;
    for (int i = 0; i < 6; i++) {
      idx[i] = s.indexOf(',', pos);
      if (idx[i] < 0) { ok = false; break; }
      pos = idx[i] + 1;
    }
    if (!ok) return;
    clkH = (byte)s.substring(2, idx[0]).toInt();
    clkM = (byte)s.substring(idx[0] + 1, idx[1]).toInt();
    clkS = (byte)s.substring(idx[1] + 1, idx[2]).toInt();
    clkDay = s.substring(idx[2] + 1, idx[3]).toInt();
    clkMon = s.substring(idx[3] + 1, idx[4]).toInt();
    clkYr = s.substring(idx[4] + 1, idx[5]).toInt();
    String blk = s.substring(idx[5] + 1);
    strncpy(clkBlk, blk.c_str(), 3); clkBlk[3] = 0;
    clkSyncMs = millis();
    clockValid = true;
    scrHas[SCR_CLOCK] = true;
  }
  else if (s.startsWith("W:")) {               // weather: temp*10,hum,wind,code
    int c1 = s.indexOf(',', 2);
    int c2 = s.indexOf(',', c1 + 1);
    int c3 = s.indexOf(',', c2 + 1);
    if (c1 < 0 || c2 < 0 || c3 < 0) return;
    wTemp10 = s.substring(2, c1).toInt();
    wHum = s.substring(c1 + 1, c2).toInt();
    wWind = s.substring(c2 + 1, c3).toInt();
    wCode = s.substring(c3 + 1).toInt();
    scrHas[SCR_WEATHER] = true;
  }
  else if (s.startsWith("Z:")) {               // azaan: 5 prayer times HH:MM
    int pos = 2;
    for (int i = 0; i < 5; i++) {
      int c = s.indexOf(',', pos);
      String t = (c < 0) ? s.substring(pos) : s.substring(pos, c);
      t.toCharArray(az[i], 6);
      if (c < 0) break;
      pos = c + 1;
    }
    scrHas[SCR_AZAAN] = true;
  }
  else if (s.startsWith("SCREEN:")) {         // web panel slide switch
    int n = s.substring(7).toInt();
    if (n >= 0 && n < SCR_COUNT) {
      screen = n;
      state = ST_SCREEN;
      msgUntil = 0;
      redraw = true;
    }
  }
  else if (s.startsWith("BTN:")) {            // virtual button gesture from web panel
    String act = s.substring(4);
    if (act == "short") shortPressAction();
    else if (act == "long") longPressAction();
    else if (act == "super") toggleBacklight();
  }
  else if (s.startsWith("MSG:")) {
    int bar = s.indexOf('|', 4);
    if (bar < 0) return;
    String a = s.substring(4, bar);
    String b = s.substring(bar + 1);
    strncpy(msg1, a.c_str(), 16); msg1[16] = 0;
    strncpy(msg2, b.c_str(), 16); msg2[16] = 0;
    msgUntil = millis() + MSG_MS;
  }
}

void drainSerial() {
  static String buf;
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n') {
      handleLine(buf);
      buf = "";
    } else if (c != '\r') {
      buf += c;
    }
  }
}

// ---------------- setup ----------------
void setup() {
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  lcd.begin();
  lcd.backlight();
  lcd.clear();
  createBarChars();
  lcdLine(0, "kmaba/ardmonitor");
  lcdLine(1, "github.com");
  beepSelect();

  Serial.begin(115200);
  delay(300);
  Serial.println("READY");
  delay(600);
}

// ---------------- loop ----------------
void loop() {
  updateButton();
  drainSerial();
  if (msgUntil && millis() > msgUntil) {
    msgUntil = 0;
    redraw = true;
  }
  render();
  delay(10);
}
