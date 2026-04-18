#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <WebServer.h>
#include <time.h>
#include <SPI.h>
#include <SD.h>
#include <TFT_eSPI.h>

#define TFT_BL     21
#define SD_CS      5
#define SD_MOSI    23
#define SD_MISO    19
#define SD_SCK     18

// Touch Pins (VSPI)
#define TOUCH_CS   33
#define TOUCH_IRQ  36
#define TOUCH_MOSI 32
#define TOUCH_MISO 39
#define TOUCH_CLK  25

// SPI-Trick: XPT2046 auf VSPI umleiten
SPIClass touchSPI(VSPI);
#define SPI touchSPI
#include <XPT2046_Touchscreen.h>
#undef SPI

// ===== WLAN =====
const char* ssid     = "6360Achalmstr";
const char* wlanPass = "mostkrug2011";

// ===== Dateien =====
const char* WATER_LOG_FILE   = "/wasserlog.csv";
const char* COUNTER_FILE     = "/zaehlerstaende.csv";

// ===== Hardware =====
TFT_eSPI tft = TFT_eSPI();
SPIClass sdSPI(HSPI);
XPT2046_Touchscreen ts(TOUCH_CS, 255);
WebServer server(80);

// ===== Status =====
bool sdOK = false;
bool lastTouched = false;
unsigned long lastTouch = 0;

// Touch-Kalibrierung
int16_t calX0 = 300, calY0 = 300;
int16_t calX1 = 3800, calY1 = 3800;

// ===== Seiten =====
enum Page { PAGE_MAIN, PAGE_KEYBOARD, PAGE_FILES, PAGE_VIEW, PAGE_SDLOG };
Page currentPage = PAGE_MAIN;

// ===== Texteingabe =====
String inputText = "";
const int INPUT_MAX_LEN = 80;

// ===== Dateianzeige =====
String viewText = "";
String viewTitle = "";

// ===== Dateiliste =====
#define MAX_FILES 10
String fileNames[MAX_FILES];
int fileCount = 0;
int fileScrollOffset = 0;

// ===== Keyboard =====
bool shiftOn = false;
bool numMode = false;
const char* kbRow1 = "QWERTZUIOP";
const char* kbRow2 = "ASDFGHJKL";
const char* kbRow3 = "YXCVBNM";

#define KB_KEY_W  28
#define KB_KEY_H  32
#define KB_GAP    2
#define KB_STEP   (KB_KEY_W + KB_GAP)
#define KB_Y_START 100

// ===== MAC-Adressen der Peers =====
uint8_t mac_haupt[]     = {0xE0, 0x8C, 0xFE, 0x58, 0x5D, 0x7C};
uint8_t mac_entkalker[] = {0xE0, 0x8C, 0xFE, 0x59, 0x59, 0x94};
uint8_t mac_offen[]     = {0xE0, 0x8C, 0xFE, 0xB6, 0xEB, 0x34};

// ===== Peer-Status =====
struct PeerInfo {
  const char* name;
  uint8_t* mac;
  bool online;
  uint32_t zaehler;       // GESAMTSTAND
  bool neuerWert;
  unsigned long lastSeen;
};

PeerInfo peers[3] = {
  {"Hauptwasseruhr",  mac_haupt,      false, 0, false, 0},
  {"Entkalkeruhr",    mac_entkalker,  false, 0, false, 0},
  {"(offen)",         mac_offen,      false, 0, false, 0}
};

// ===== Datenpaket vom Slave =====
// Slave sendet DIFFERENZ / Impulsblock
struct WasserDaten {
  uint8_t  peerID;
  uint32_t zaehler;
};

// ===== Vorwärtsdeklarationen =====
void drawMainPage();
void updatePeerStatus();
void drawKeyboardPage();
void drawNumberPage();
void drawFilesPage();
void drawViewPage();
void drawSDLogPage();
void loadFileList();

// ===== Hilfsfunktionen =====
String getTimeString() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) return "---";
  char buf[30];
  strftime(buf, sizeof(buf), "%d.%m.%Y %H:%M:%S", &timeinfo);
  return String(buf);
}

String htmlEscape(const String &s) {
  String out;
  out.reserve(s.length() + 20);
  for (size_t i = 0; i < s.length(); i++) {
    char c = s[i];
    if (c == '&') out += "&amp;";
    else if (c == '<') out += "&lt;";
    else if (c == '>') out += "&gt;";
    else out += c;
  }
  return out;
}

uint8_t getWiFiChannel() {
  uint8_t primaryChan = 0;
  wifi_second_chan_t secondChan;
  esp_wifi_get_channel(&primaryChan, &secondChan);
  return primaryChan;
}

void addPeer(uint8_t *mac) {
  esp_now_peer_info_t info = {};
  memcpy(info.peer_addr, mac, 6);
  info.channel = getWiFiChannel();
  info.encrypt = false;
  esp_now_add_peer(&info);
}

int nextFileNumber() {
  for (int i = 1; i <= 999; i++) {
    char fname[20];
    sprintf(fname, "/notiz%03d.txt", i);
    if (!SD.exists(fname)) return i;
  }
  return 999;
}

// ===== Zählerstand speichern / laden =====
void ensureCounterFile() {
  if (!sdOK) return;
  if (!SD.exists(COUNTER_FILE)) {
    File f = SD.open(COUNTER_FILE, FILE_WRITE);
    if (f) {
      f.println("Sensor;Gesamtstand");
      f.println("Hauptwasseruhr;0");
      f.println("Entkalkeruhr;0");
      f.println("(offen);0");
      f.close();
    }
  }
}

void saveCounterState() {
  if (!sdOK) return;

  SD.remove(COUNTER_FILE);
  File f = SD.open(COUNTER_FILE, FILE_WRITE);
  if (!f) return;

  f.println("Sensor;Gesamtstand");
  for (int i = 0; i < 3; i++) {
    f.printf("%s;%lu\n", peers[i].name, peers[i].zaehler);
  }
  f.close();
}

void loadCounterState() {
  if (!sdOK) return;

  ensureCounterFile();

  File f = SD.open(COUNTER_FILE, FILE_READ);
  if (!f) return;

  // Header überspringen
  String line = f.readStringUntil('\n');
  line.trim();

  while (f.available()) {
    String row = f.readStringUntil('\n');
    row.trim();
    if (row.length() == 0) continue;

    int sep = row.indexOf(';');
    if (sep < 0) continue;

    String sensor = row.substring(0, sep);
    String value  = row.substring(sep + 1);

    for (int i = 0; i < 3; i++) {
      if (sensor == peers[i].name) {
        peers[i].zaehler = (uint32_t)value.toInt();
      }
    }
  }

  f.close();
}

// ===== Logging =====
void ensureWaterLogFile() {
  if (!sdOK) return;
  if (!SD.exists(WATER_LOG_FILE)) {
    File f = SD.open(WATER_LOG_FILE, FILE_WRITE);
    if (f) {
      f.println("Zeitstempel;Sensor;Impulse;Gesamtstand");
      f.close();
    }
  }
}

void logToSD(int peerIdx, uint32_t impulseBlock, uint32_t gesamtstand) {
  if (!sdOK) return;

  String zeit = getTimeString();
  File f = SD.open(WATER_LOG_FILE, FILE_APPEND);
  if (f) {
    f.printf("%s;%s;%lu;%lu\n",
             zeit.c_str(),
             peers[peerIdx].name,
             impulseBlock,
             gesamtstand);
    f.close();
  }
}

// ===== Touch lesen =====
bool getTouch(int &tx, int &ty) {
  TS_Point p = ts.getPoint();
  bool touched = (p.z >= 400);
  if (p.x >= 8000 || p.y >= 8000 || p.x <= 100 || p.y <= 100) touched = false;

  if (touched && !lastTouched) {
    tx = map(p.x, calX0, calX1, 0, 319);
    ty = map(p.y, calY0, calY1, 0, 239);
    tx = constrain(tx, 0, 319);
    ty = constrain(ty, 0, 239);
    lastTouched = true;
    return true;
  }

  if (!touched) lastTouched = false;
  return false;
}

// ===== UI Hilfen =====
void drawBtn(int x, int y, int w, int h, const char* label, uint16_t color) {
  tft.fillRoundRect(x, y, w, h, 4, color);
  tft.drawRoundRect(x, y, w, h, 4, TFT_WHITE);
  tft.setTextDatum(MC_DATUM);
  tft.setTextSize(1);
  tft.setTextColor(TFT_WHITE, color);
  tft.drawString(label, x + w/2, y + h/2);
}

void drawBtnBig(int x, int y, int w, int h, const char* label, uint16_t color) {
  tft.fillRoundRect(x, y, w, h, 5, color);
  tft.drawRoundRect(x, y, w, h, 5, TFT_WHITE);
  tft.setTextDatum(MC_DATUM);
  tft.setTextSize(1);
  tft.setTextColor(TFT_WHITE, color);
  tft.drawString(label, x + w/2, y + h/2);
}

bool btnHit(int bx, int by, int bw, int bh, int tx, int ty) {
  return (tx >= bx && tx <= bx + bw && ty >= by && ty <= by + bh);
}

void drawWrappedText(String text, int x, int y, int maxW, uint16_t color) {
  tft.setTextDatum(TL_DATUM);
  tft.setTextSize(1);
  tft.setTextColor(color, TFT_BLACK);

  int cx = x, cy = y;
  String word = "";

  for (unsigned int i = 0; i <= text.length(); i++) {
    char c = (i < text.length()) ? text[i] : ' ';
    if (c == '\n') {
      tft.drawString(word, cx, cy);
      word = "";
      cx = x;
      cy += 12;
    } else if (c == ' ' || i == text.length()) {
      int ww = tft.textWidth(word + " ");
      if (cx + ww > x + maxW) {
        cx = x;
        cy += 12;
      }
      tft.drawString(word + " ", cx, cy);
      cx += ww;
      word = "";
    } else {
      word += c;
    }
  }
}

// ===== ESP-NOW =====
void onDataSent(const uint8_t *mac, esp_now_send_status_t status) {
  for (int i = 0; i < 3; i++) {
    if (memcmp(mac, peers[i].mac, 6) == 0) {
      peers[i].online = (status == ESP_NOW_SEND_SUCCESS);
    }
  }
}

void onDataRecv(const uint8_t *mac, const uint8_t *data, int len) {
  if (len != sizeof(WasserDaten)) return;

  WasserDaten empfangen;
  memcpy(&empfangen, data, sizeof(WasserDaten));

  int idx = empfangen.peerID - 2;
  if (idx < 0 || idx >= 3) return;

  // WICHTIG:
  // Slave sendet Impulsblöcke / Differenzen.
  // Darum hier AUFADDEN auf den Gesamtzähler.
  uint32_t impulseBlock = empfangen.zaehler;
  peers[idx].zaehler += impulseBlock;
  peers[idx].online = true;
  peers[idx].neuerWert = true;
  peers[idx].lastSeen = millis();

  Serial.printf("Empfangen von %s: +%lu Impulse, Gesamt=%lu Liter\n",
                peers[idx].name,
                impulseBlock,
                peers[idx].zaehler);

  logToSD(idx, impulseBlock, peers[idx].zaehler);
  saveCounterState();
}

// ===== Browser =====
String buildLogHtmlTable() {
  if (!sdOK) return "<p>Keine SD-Karte verfügbar.</p>";

  File f = SD.open(WATER_LOG_FILE, FILE_READ);
  if (!f) return "<p>Kein Wasserlog vorhanden.</p>";

  // Letzte 30 Zeilen puffern
  const int MAX_LINES = 30;
  String lines[MAX_LINES];
  int count = 0;

  while (f.available()) {
    String line = f.readStringUntil('\n');
    line.trim();
    if (line.length() > 0) {
      lines[count % MAX_LINES] = line;
      count++;
    }
  }
  f.close();

  String html = "<table style='width:100%;border-collapse:collapse;margin-top:20px'>";
  html += "<tr>"
          "<th style='text-align:left;border-bottom:1px solid #555;padding:6px'>Zeit</th>"
          "<th style='text-align:left;border-bottom:1px solid #555;padding:6px'>Sensor</th>"
          "<th style='text-align:right;border-bottom:1px solid #555;padding:6px'>Impulse</th>"
          "<th style='text-align:right;border-bottom:1px solid #555;padding:6px'>Gesamt</th>"
          "</tr>";

  int start = (count > MAX_LINES) ? count - MAX_LINES : 0;
  int shown = (count > MAX_LINES) ? MAX_LINES : count;

  for (int i = 0; i < shown; i++) {
    String row = lines[(start + i) % MAX_LINES];

    int p1 = row.indexOf(';');
    int p2 = row.indexOf(';', p1 + 1);
    int p3 = row.indexOf(';', p2 + 1);

    if (p1 < 0 || p2 < 0 || p3 < 0) continue;

    String zeit   = row.substring(0, p1);
    String sensor = row.substring(p1 + 1, p2);
    String imp    = row.substring(p2 + 1, p3);
    String gesamt = row.substring(p3 + 1);

    html += "<tr>";
    html += "<td style='padding:6px;border-bottom:1px solid #333'>" + htmlEscape(zeit) + "</td>";
    html += "<td style='padding:6px;border-bottom:1px solid #333'>" + htmlEscape(sensor) + "</td>";
    html += "<td style='padding:6px;border-bottom:1px solid #333;text-align:right'>" + htmlEscape(imp) + "</td>";
    html += "<td style='padding:6px;border-bottom:1px solid #333;text-align:right'>" + htmlEscape(gesamt) + "</td>";
    html += "</tr>";
  }

  html += "</table>";
  return html;
}

void handleRoot() {
  String zeitstempel = getTimeString();

  String html = "<!DOCTYPE html><html><head><meta charset='UTF-8'>"
    "<meta http-equiv='refresh' content='5'><title>WasserKontrolle</title>"
    "<style>"
    "body{font-family:Arial;background:#1a1a2e;color:#eee;padding:20px}"
    "h1{text-align:center;color:#00d4ff}"
    ".p{background:#16213e;border-radius:10px;padding:15px;margin:12px 0;display:flex;justify-content:space-between;align-items:center}"
    ".on{border-left:4px solid #0f8}.off{border-left:4px solid #f44}"
    ".n{font-weight:bold}.l{color:#00d4ff;font-size:1.2em;margin-top:6px}"
    ".s{padding:6px 14px;border-radius:20px;font-weight:bold}"
    ".son{background:#00ff8833;color:#0f8}.soff{background:#ff444433;color:#f44}"
    "a{color:#8fd3ff}"
    "</style></head><body>";

  html += "<h1>&#128167; WasserKontrolle</h1>";
  html += "<div style='text-align:center;color:#aaa;font-size:0.9em'>Stand: " + zeitstempel + "</div>";

  for (int i = 0; i < 3; i++) {
    const char* cls  = peers[i].online ? "on" : "off";
    const char* txt  = peers[i].online ? "ONLINE" : "OFFLINE";
    const char* scls = peers[i].online ? "son" : "soff";

    html += "<div class='p " + String(cls) + "'>";
    html += "<div><div class='n'>" + String(peers[i].name) + "</div>";
    html += "<div class='l'>" + String(peers[i].zaehler) + " Liter</div></div>";
    html += "<span class='s " + String(scls) + "'>" + txt + "</span>";
    html += "</div>";
  }

  html += "<h2 style='margin-top:30px;color:#ffd166'>Letzte Wasserlog-Einträge</h2>";
  html += buildLogHtmlTable();

  html += "<div style='text-align:center;color:#555;margin-top:30px;font-size:0.85em'>"
          "Auto-Refresh 5s | IP: " + WiFi.localIP().toString() + "</div>";

  html += "</body></html>";

  server.send(200, "text/html", html);
}

// ===== Hauptseite =====
void drawMainPage() {
  tft.fillScreen(TFT_BLACK);

  tft.setTextDatum(TC_DATUM);
  tft.setTextSize(1);
  tft.setTextFont(2);
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.drawString("WasserKontrolle", 160, 2);

  drawBtnBig(5,   20, 60, 28, "NOTIZ",   0x0400);
  drawBtnBig(70,  20, 60, 28, "DATEIEN", 0x000A);
  drawBtnBig(135, 20, 60, 28, "LETZTE",  0x4808);
  drawBtnBig(200, 20, 60, 28, "SD LOG",  TFT_NAVY);
  drawBtnBig(265, 20, 50, 28, "DEL",     TFT_MAROON);

  tft.drawFastHLine(0, 52, 320, TFT_DARKGREY);

  for (int i = 0; i < 3; i++) {
    int y = 56 + i * 55;

    tft.setTextDatum(TL_DATUM);
    tft.setTextFont(2);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString(peers[i].name, 10, y);

    uint16_t col = peers[i].online ? TFT_GREEN : TFT_RED;
    tft.fillCircle(300, y + 7, 7, col);

    tft.setTextFont(4);
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    char litBuf[20];
    snprintf(litBuf, sizeof(litBuf), "%lu L", peers[i].zaehler);
    tft.drawString(litBuf, 10, y + 22);

    tft.setTextFont(2);
    tft.setTextColor(col, TFT_BLACK);
    tft.drawString(peers[i].online ? "ON" : "OFF", 270, y + 26);

    if (i < 2) tft.drawFastHLine(10, y + 50, 300, TFT_DARKGREY);
  }

  tft.setTextDatum(TL_DATUM);
  tft.setTextFont(1);
  if (WiFi.status() == WL_CONNECTED) {
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.drawString("IP: " + WiFi.localIP().toString(), 5, 225);
  }
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.drawString(getTimeString(), 160, 225);
}

void updatePeerStatus() {
  for (int i = 0; i < 3; i++) {
    int y = 56 + i * 55;
    uint16_t col = peers[i].online ? TFT_GREEN : TFT_RED;

    tft.fillCircle(300, y + 7, 7, col);

    tft.fillRect(10, y + 22, 250, 24, TFT_BLACK);
    tft.setTextDatum(TL_DATUM);
    tft.setTextFont(4);
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    char litBuf[20];
    snprintf(litBuf, sizeof(litBuf), "%lu L", peers[i].zaehler);
    tft.drawString(litBuf, 10, y + 22);

    tft.fillRect(265, y + 26, 35, 16, TFT_BLACK);
    tft.setTextFont(2);
    tft.setTextColor(col, TFT_BLACK);
    tft.drawString(peers[i].online ? "ON" : "OFF", 270, y + 26);
  }

  tft.fillRect(160, 225, 160, 10, TFT_BLACK);
  tft.setTextDatum(TL_DATUM);
  tft.setTextFont(1);
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.drawString(getTimeString(), 160, 225);
}

void handleMainPage(int tx, int ty) {
  if (btnHit(5, 20, 60, 28, tx, ty)) {
    inputText = "";
    currentPage = PAGE_KEYBOARD;
    return;
  }

  if (btnHit(70, 20, 60, 28, tx, ty)) {
    currentPage = PAGE_FILES;
    return;
  }

  if (btnHit(135, 20, 60, 28, tx, ty)) {
    int num = nextFileNumber() - 1;
    if (num >= 1) {
      char fname[20];
      sprintf(fname, "/notiz%03d.txt", num);
      delay(100);
      File f = SD.open(fname, FILE_READ);
      if (f) {
        viewTitle = String(fname);
        viewText = "";
        while (f.available()) viewText += (char)f.read();
        f.close();
        currentPage = PAGE_VIEW;
        return;
      }
    }
    return;
  }

  if (btnHit(200, 20, 60, 28, tx, ty)) {
    currentPage = PAGE_SDLOG;
    return;
  }

  if (btnHit(265, 20, 50, 28, tx, ty)) {
    File root = SD.open("/");
    if (root) {
      File entry;
      while ((entry = root.openNextFile())) {
        String name = "/" + String(entry.name());
        entry.close();
        if (name.endsWith(".txt")) {
          SD.remove(name.c_str());
        }
      }
      root.close();
    }
    drawMainPage();
    return;
  }
}

// ===== SD-LOG Seite =====
void drawSDLogPage() {
  tft.fillScreen(TFT_BLACK);
  tft.setTextDatum(TC_DATUM);
  tft.setTextFont(2);
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.drawString("Wasser-Log (SD)", 160, 5);
  drawBtn(260, 5, 55, 18, "ZURUECK", TFT_RED);
  tft.drawFastHLine(0, 26, 320, TFT_DARKGREY);

  if (!sdOK) {
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TFT_RED, TFT_BLACK);
    tft.drawString("Keine SD-Karte!", 160, 120);
    return;
  }

  File f = SD.open(WATER_LOG_FILE);
  if (!f) {
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TFT_RED, TFT_BLACK);
    tft.drawString("Datei nicht gefunden!", 160, 120);
    return;
  }

  String zeilen[10];
  int count = 0;
  while (f.available()) {
    String line = f.readStringUntil('\n');
    line.trim();
    if (line.length() > 0) {
      zeilen[count % 10] = line;
      count++;
    }
  }
  f.close();

  tft.setTextDatum(TL_DATUM);
  tft.setTextFont(1);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);

  int startIdx = (count > 10) ? count - 10 : 0;
  int displayed = (count > 10) ? 10 : count;
  for (int i = 0; i < displayed; i++) {
    tft.drawString(zeilen[(startIdx + i) % 10], 5, 30 + i * 18);
  }

  tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
  char infoBuf[30];
  snprintf(infoBuf, sizeof(infoBuf), "%d Eintraege", count > 0 ? count - 1 : 0);
  tft.drawString(infoBuf, 5, 220);
}

void handleSDLogPage(int tx, int ty) {
  if (btnHit(260, 5, 55, 18, tx, ty)) {
    currentPage = PAGE_MAIN;
  }
}

// ===== Keyboard =====
void drawKeyboardPage() {
  tft.fillScreen(TFT_BLACK);
  tft.setTextDatum(TL_DATUM);
  tft.setTextSize(1);
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.drawString("Neue Notiz eingeben:", 5, 5);

  tft.fillRect(5, 20, 310, 30, TFT_DARKGREY);
  tft.drawRect(5, 20, 310, 30, TFT_WHITE);
  tft.setTextColor(TFT_WHITE, TFT_DARKGREY);

  String showText = inputText;
  if (showText.length() > 45) showText = ".." + showText.substring(showText.length() - 43);
  tft.drawString(showText + "_", 10, 30);

  drawBtn(5, 58, 50, 30, "OK", TFT_GREEN);
  drawBtn(60, 58, 60, 30, "ZURUECK", TFT_RED);
  drawBtn(125, 58, 40, 30, "DEL", TFT_ORANGE);
  drawBtn(170, 58, 50, 30, "LEER", TFT_BLUE);
  drawBtn(225, 58, 40, 30, "ABC", shiftOn ? TFT_YELLOW : 0x4208);
  drawBtn(270, 58, 45, 30, "123", 0x4208);

  int y1 = KB_Y_START;
  int x1 = (320 - 10 * KB_STEP) / 2;
  for (int i = 0; i < 10; i++) {
    char key[2] = {kbRow1[i], 0};
    if (!shiftOn) key[0] += 32;
    drawBtn(x1 + i * KB_STEP, y1, KB_KEY_W, KB_KEY_H, key, 0x2104);
  }

  int y2 = y1 + KB_KEY_H + KB_GAP;
  int x2 = (320 - 9 * KB_STEP) / 2;
  for (int i = 0; i < 9; i++) {
    char key[2] = {kbRow2[i], 0};
    if (!shiftOn) key[0] += 32;
    drawBtn(x2 + i * KB_STEP, y2, KB_KEY_W, KB_KEY_H, key, 0x2104);
  }

  int y3 = y2 + KB_KEY_H + KB_GAP;
  int x3 = (320 - 7 * KB_STEP) / 2;
  for (int i = 0; i < 7; i++) {
    char key[2] = {kbRow3[i], 0};
    if (!shiftOn) key[0] += 32;
    drawBtn(x3 + i * KB_STEP, y3, KB_KEY_W, KB_KEY_H, key, 0x2104);
  }

  drawBtn(x3 + 7 * KB_STEP, y3, KB_KEY_W, KB_KEY_H, ".", 0x2104);
  drawBtn(x3 + 8 * KB_STEP, y3, KB_KEY_W, KB_KEY_H, ",", 0x2104);
  drawBtn(x3 + 9 * KB_STEP, y3, KB_KEY_W, KB_KEY_H, "!", 0x2104);
}

void drawNumberPage() {
  tft.fillScreen(TFT_BLACK);
  tft.setTextDatum(TL_DATUM);
  tft.setTextSize(1);
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.drawString("Neue Notiz eingeben:", 5, 5);

  tft.fillRect(5, 20, 310, 30, TFT_DARKGREY);
  tft.drawRect(5, 20, 310, 30, TFT_WHITE);
  tft.setTextColor(TFT_WHITE, TFT_DARKGREY);

  String showText = inputText;
  if (showText.length() > 45) showText = ".." + showText.substring(showText.length() - 43);
  tft.drawString(showText + "_", 10, 30);

  drawBtn(5, 58, 50, 30, "OK", TFT_GREEN);
  drawBtn(60, 58, 60, 30, "ZURUECK", TFT_RED);
  drawBtn(125, 58, 40, 30, "DEL", TFT_ORANGE);
  drawBtn(170, 58, 50, 30, "LEER", TFT_BLUE);
  drawBtn(225, 58, 40, 30, "ABC", 0x4208);
  drawBtn(270, 58, 45, 30, "123", TFT_YELLOW);

  const char* nums = "1234567890";
  int y1 = KB_Y_START;
  int x1 = (320 - 10 * KB_STEP) / 2;
  for (int i = 0; i < 10; i++) {
    char key[2] = {nums[i], 0};
    drawBtn(x1 + i * KB_STEP, y1, KB_KEY_W, KB_KEY_H, key, 0x0010);
  }

  const char* syms1 = "+-*/=@#&()";
  int y2 = y1 + KB_KEY_H + KB_GAP;
  for (int i = 0; i < 10; i++) {
    char key[2] = {syms1[i], 0};
    drawBtn(x1 + i * KB_STEP, y2, KB_KEY_W, KB_KEY_H, key, 0x0010);
  }

  const char* syms2 = ".,;:!?\"'-_";
  int y3 = y2 + KB_KEY_H + KB_GAP;
  for (int i = 0; i < 10; i++) {
    char key[2] = {syms2[i], 0};
    drawBtn(x1 + i * KB_STEP, y3, KB_KEY_W, KB_KEY_H, key, 0x0010);
  }
}

void handleKeyboardPage(int tx, int ty) {
  if (btnHit(5, 58, 50, 30, tx, ty)) {
    if (inputText.length() > 0) {
      int num = nextFileNumber();
      char fname[20];
      sprintf(fname, "/notiz%03d.txt", num);
      delay(100);
      File f = SD.open(fname, FILE_WRITE);
      if (f) {
        f.print(inputText);
        f.close();
      }
    }
    currentPage = PAGE_MAIN;
    return;
  }

  if (btnHit(60, 58, 60, 30, tx, ty)) {
    currentPage = PAGE_MAIN;
    return;
  }

  if (btnHit(125, 58, 40, 30, tx, ty)) {
    if (inputText.length() > 0) inputText.remove(inputText.length() - 1);
    if (numMode) drawNumberPage(); else drawKeyboardPage();
    return;
  }

  if (btnHit(170, 58, 50, 30, tx, ty)) {
    if (inputText.length() < INPUT_MAX_LEN) inputText += " ";
    if (numMode) drawNumberPage(); else drawKeyboardPage();
    return;
  }

  if (btnHit(225, 58, 40, 30, tx, ty)) {
    if (numMode) {
      numMode = false;
      drawKeyboardPage();
    } else {
      shiftOn = !shiftOn;
      drawKeyboardPage();
    }
    return;
  }

  if (btnHit(270, 58, 45, 30, tx, ty)) {
    numMode = !numMode;
    if (numMode) drawNumberPage(); else drawKeyboardPage();
    return;
  }

  if (numMode) {
    const char* nums = "1234567890";
    const char* syms1 = "+-*/=@#&()";
    const char* syms2 = ".,;:!?\"'-_";
    int x1 = (320 - 10 * KB_STEP) / 2;
    int y1 = KB_Y_START, y2 = y1 + KB_KEY_H + KB_GAP, y3 = y2 + KB_KEY_H + KB_GAP;

    for (int i = 0; i < 10; i++) {
      if (btnHit(x1 + i * KB_STEP, y1, KB_KEY_W, KB_KEY_H, tx, ty)) {
        if (inputText.length() < INPUT_MAX_LEN) inputText += nums[i];
        drawNumberPage();
        return;
      }
      if (btnHit(x1 + i * KB_STEP, y2, KB_KEY_W, KB_KEY_H, tx, ty)) {
        if (inputText.length() < INPUT_MAX_LEN) inputText += syms1[i];
        drawNumberPage();
        return;
      }
      if (btnHit(x1 + i * KB_STEP, y3, KB_KEY_W, KB_KEY_H, tx, ty)) {
        if (inputText.length() < INPUT_MAX_LEN) inputText += syms2[i];
        drawNumberPage();
        return;
      }
    }
  } else {
    int y1 = KB_Y_START;
    int x1 = (320 - 10 * KB_STEP) / 2;
    for (int i = 0; i < 10; i++) {
      if (btnHit(x1 + i * KB_STEP, y1, KB_KEY_W, KB_KEY_H, tx, ty)) {
        char c = kbRow1[i];
        if (!shiftOn) c += 32;
        if (inputText.length() < INPUT_MAX_LEN) inputText += c;
        drawKeyboardPage();
        return;
      }
    }

    int y2 = y1 + KB_KEY_H + KB_GAP;
    int x2 = (320 - 9 * KB_STEP) / 2;
    for (int i = 0; i < 9; i++) {
      if (btnHit(x2 + i * KB_STEP, y2, KB_KEY_W, KB_KEY_H, tx, ty)) {
        char c = kbRow2[i];
        if (!shiftOn) c += 32;
        if (inputText.length() < INPUT_MAX_LEN) inputText += c;
        drawKeyboardPage();
        return;
      }
    }

    int y3 = y2 + KB_KEY_H + KB_GAP;
    int x3 = (320 - 7 * KB_STEP) / 2;
    for (int i = 0; i < 7; i++) {
      if (btnHit(x3 + i * KB_STEP, y3, KB_KEY_W, KB_KEY_H, tx, ty)) {
        char c = kbRow3[i];
        if (!shiftOn) c += 32;
        if (inputText.length() < INPUT_MAX_LEN) inputText += c;
        drawKeyboardPage();
        return;
      }
    }

    const char extras[] = {'.', ',', '!'};
    for (int i = 0; i < 3; i++) {
      if (btnHit(x3 + (7 + i) * KB_STEP, y3, KB_KEY_W, KB_KEY_H, tx, ty)) {
        if (inputText.length() < INPUT_MAX_LEN) inputText += extras[i];
        drawKeyboardPage();
        return;
      }
    }
  }
}

// ===== Dateiliste =====
void loadFileList() {
  fileCount = 0;
  File root = SD.open("/");
  if (!root) return;

  File entry;
  while ((entry = root.openNextFile()) && fileCount < MAX_FILES) {
    String name = String(entry.name());
    if (name.endsWith(".txt")) {
      fileNames[fileCount] = "/" + name;
      fileCount++;
    }
    entry.close();
  }
  root.close();
}

void drawFilesPage() {
  tft.fillScreen(TFT_BLACK);
  tft.setTextDatum(TC_DATUM);
  tft.setTextSize(2);
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.drawString("SD-Dateien", 160, 5);
  drawBtn(250, 5, 65, 22, "ZURUECK", TFT_RED);

  if (fileCount == 0) {
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString("Keine Dateien", 160, 120);
    return;
  }

  int perPage = 6, startY = 35;
  for (int i = 0; i < perPage && (i + fileScrollOffset) < fileCount; i++) {
    drawBtn(10, startY + i * 32, 300, 28, fileNames[i + fileScrollOffset].c_str(), 0x000A);
  }

  if (fileScrollOffset > 0) drawBtn(10, 225, 60, 15, "HOCH", 0x4208);
  if (fileScrollOffset + perPage < fileCount) drawBtn(250, 225, 60, 15, "RUNTER", 0x4208);
}

void handleFilesPage(int tx, int ty) {
  if (btnHit(250, 5, 65, 22, tx, ty)) {
    currentPage = PAGE_MAIN;
    return;
  }

  if (btnHit(10, 225, 60, 15, tx, ty) && fileScrollOffset > 0) {
    fileScrollOffset--;
    drawFilesPage();
    return;
  }

  if (btnHit(250, 225, 60, 15, tx, ty)) {
    fileScrollOffset++;
    drawFilesPage();
    return;
  }

  int perPage = 6, startY = 35;
  for (int i = 0; i < perPage && (i + fileScrollOffset) < fileCount; i++) {
    if (btnHit(10, startY + i * 32, 300, 28, tx, ty)) {
      int idx = i + fileScrollOffset;
      delay(100);
      File f = SD.open(fileNames[idx].c_str(), FILE_READ);
      if (f) {
        viewTitle = fileNames[idx];
        viewText = "";
        while (f.available()) viewText += (char)f.read();
        f.close();
        currentPage = PAGE_VIEW;
        return;
      }
    }
  }
}

// ===== Datei anzeigen =====
void drawViewPage() {
  tft.fillScreen(TFT_BLACK);
  tft.setTextDatum(TL_DATUM);
  tft.setTextSize(1);
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.drawString(viewTitle, 5, 5);
  drawBtn(200, 2, 55, 18, "ZURUECK", TFT_RED);
  drawBtn(260, 2, 55, 18, "LOESCHEN", TFT_MAROON);
  tft.drawLine(0, 22, 319, 22, TFT_DARKGREY);
  drawWrappedText(viewText, 5, 28, 310, TFT_WHITE);
}

void handleViewPage(int tx, int ty) {
  if (btnHit(200, 2, 55, 18, tx, ty)) {
    currentPage = PAGE_FILES;
    return;
  }

  if (btnHit(260, 2, 55, 18, tx, ty)) {
    SD.remove(viewTitle.c_str());
    currentPage = PAGE_MAIN;
    return;
  }
}

// ===== SETUP =====
void setup() {
  delay(2000);
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n=== WasserKontrolle ===\n");

  // Display
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);
  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);
  Serial.println("Display OK");

  // Touch
  SPI.begin(TOUCH_CLK, TOUCH_MISO, TOUCH_MOSI, TOUCH_CS);
  ts.begin();
  ts.setRotation(1);
  Serial.println("Touch OK");
  lastTouch = millis() + 3000;

  // SD
  sdSPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
  if (SD.begin(SD_CS, sdSPI)) {
    sdOK = true;
    Serial.println("SD-Karte OK");
    ensureWaterLogFile();
    ensureCounterFile();
    loadCounterState();
    Serial.println("Zählerstände geladen");
  } else {
    sdOK = false;
    Serial.println("SD-Karte FEHLER!");
  }

  // WLAN AP+STA
  WiFi.mode(WIFI_AP_STA);
  Serial.printf("Verbinde mit WLAN: %s\n", ssid);
  WiFi.begin(ssid, wlanPass);

  tft.setTextDatum(MC_DATUM);
  tft.setTextFont(2);
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.drawString("Verbinde mit WLAN...", 160, 120);

  int timeout = 0;
  while (WiFi.status() != WL_CONNECTED && timeout < 20) {
    delay(500);
    timeout++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("WLAN OK! IP: %s\n", WiFi.localIP().toString().c_str());
    configTzTime("CET-1CEST,M3.5.0,M10.5.0/3", "pool.ntp.org", "time.nist.gov");
  } else {
    Serial.println("WLAN fehlgeschlagen!");
  }

  // ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW Fehler!");
  } else {
    esp_now_register_send_cb(onDataSent);
    esp_now_register_recv_cb(onDataRecv);
    addPeer(mac_haupt);
    addPeer(mac_entkalker);
    addPeer(mac_offen);
    Serial.printf("ESP-NOW OK, 3 Peers (Kanal %d)\n", getWiFiChannel());
  }

  // Webserver
  server.on("/", handleRoot);
  server.begin();
  Serial.println("Webserver OK");

  drawMainPage();
  Serial.println("Setup fertig!\n");
}

// ===== LOOP =====
void loop() {
  server.handleClient();

  // Offline-Erkennung
  for (int i = 0; i < 3; i++) {
    if (peers[i].online && millis() - peers[i].lastSeen > 10000) {
      peers[i].online = false;
    }
  }

  int tx, ty;
  if (getTouch(tx, ty) && millis() - lastTouch > 300) {
    lastTouch = millis();

    switch (currentPage) {
      case PAGE_MAIN:     handleMainPage(tx, ty); break;
      case PAGE_KEYBOARD: handleKeyboardPage(tx, ty); break;
      case PAGE_FILES:    handleFilesPage(tx, ty); break;
      case PAGE_VIEW:     handleViewPage(tx, ty); break;
      case PAGE_SDLOG:    handleSDLogPage(tx, ty); break;
    }

    static Page lastPage = PAGE_MAIN;
    if (currentPage != lastPage) {
      switch (currentPage) {
        case PAGE_MAIN:     drawMainPage(); break;
        case PAGE_KEYBOARD: numMode = false; drawKeyboardPage(); break;
        case PAGE_FILES:    fileScrollOffset = 0; loadFileList(); drawFilesPage(); break;
        case PAGE_VIEW:     drawViewPage(); break;
        case PAGE_SDLOG:    drawSDLogPage(); break;
      }
      lastPage = currentPage;
    }
  }

  // Hauptseite aktualisieren + Ping
  static unsigned long lastUpdate = 0;
  if (currentPage == PAGE_MAIN && millis() - lastUpdate > 2000) {
    lastUpdate = millis();

    uint8_t ping = 1;
    for (int i = 0; i < 3; i++) {
      esp_now_send(peers[i].mac, &ping, sizeof(ping));
      delay(20);
    }

    delay(100);
    updatePeerStatus();
  }

  delay(50);
}