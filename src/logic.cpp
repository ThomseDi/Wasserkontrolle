#include "logic.h"
#include "web.h"
#include "notification.h"

// ===== Definitionen globaler Variablen =====
const char* ssid     = "6360Achalmstr";
const char* wlanPass = "mostkrug2011";

const char* WATER_LOG_FILE = "/wasserlog.csv";
const char* COUNTER_FILE   = "/zaehlerstaende.csv";
const time_t MIN_VALID_TIME = 1776000000;

TFT_eSPI tft = TFT_eSPI();
SPIClass sdSPI(HSPI);
XPT2046_Touchscreen ts(TOUCH_CS, 255);
WebServer server(80);

bool sdOK = false;
bool lastTouched = false;
unsigned long lastTouch = 0;
bool timeValid = false;
unsigned long lastClockCheck = 0;

int16_t calX0 = 300, calY0 = 300, calX1 = 3800, calY1 = 3800;

Page currentPage = PAGE_MAIN;

String inputText = "";
const int INPUT_MAX_LEN = 80;

int selectedPeer = 0;

String viewText = "";
String viewTitle = "";

String fileNames[MAX_FILES];
int fileCount = 0;
int fileScrollOffset = 0;

bool shiftOn = false;
bool numMode = false;
const char* kbRow1 = "QWERTZUIOP";
const char* kbRow2 = "ASDFGHJKL";
const char* kbRow3 = "YXCVBNM";

uint8_t mac_haupt[]     = {0xE0, 0x8C, 0xFE, 0x58, 0x5D, 0x7C};
uint8_t mac_entkalker[] = {0xE0, 0x8C, 0xFE, 0x59, 0x59, 0x94};
uint8_t mac_offen[]     = {0xE0, 0x8C, 0xFE, 0xB6, 0xEB, 0x34};

PeerInfo peers[3] = {
  {"Hauptwasseruhr", mac_haupt, false, 0, false, 0},
  {"Entkalkeruhr",   mac_entkalker, false, 0, false, 0},
  {"(offen)",        mac_offen, false, 0, false, 0}
};

unsigned long lastNightAlarmMillis = 0;

// ===== Dauerlauf-Alarm =====
static unsigned long haupt_flussStart        = 0;
static unsigned long haupt_flussZuletzt      = 0;
static unsigned long lastDauerlaufAlarmMillis = 0;

// ===== Entkalker-Alarm =====
const char* ENTKALKER_FILE = "/entkalker.csv";
uint32_t entkalkerVerbrauch = 0;
uint32_t entkalkerGrenzwert = 1500;
bool     entkalkerAlarm     = false;
static unsigned long lastEntkalkerPushMillis = 0;

// ===== Hilfsfunktionen =====
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

void checkTimeValid() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    timeValid = false;
    return;
  }

  time_t now;
  time(&now);
  timeValid = (now >= MIN_VALID_TIME);
}

String getTimeString() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) return "---";

  time_t now;
  time(&now);
  if (now < MIN_VALID_TIME) return "---";

  char buf[30];
  strftime(buf, sizeof(buf), "%d.%m.%Y %H:%M:%S", &timeinfo);
  return String(buf);
}

bool peerIsOnline(int i) {
  return (millis() - peers[i].lastSeen <= 10000);
}

// ===== Zähler / SD =====
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
  if (!timeValid) return;

  String zeit = getTimeString();
  if (zeit == "---") return;

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

void clearWaterLog() {
  if (!sdOK) return;
  SD.remove(WATER_LOG_FILE);
  ensureWaterLogFile();
}

// ===== Entkalker SD =====
void saveEntkalkerState() {
  if (!sdOK) return;
  SD.remove(ENTKALKER_FILE);
  File f = SD.open(ENTKALKER_FILE, FILE_WRITE);
  if (!f) return;
  f.printf("verbrauch;%lu\n", entkalkerVerbrauch);
  f.printf("grenzwert;%lu\n", entkalkerGrenzwert);
  f.printf("alarm;%d\n", (int)entkalkerAlarm);
  f.close();
}

void loadEntkalkerState() {
  if (!sdOK) return;
  File f = SD.open(ENTKALKER_FILE, FILE_READ);
  if (!f) return;
  while (f.available()) {
    String row = f.readStringUntil('\n');
    row.trim();
    int sep = row.indexOf(';');
    if (sep < 0) continue;
    String key = row.substring(0, sep);
    String val = row.substring(sep + 1);
    if (key == "verbrauch") entkalkerVerbrauch = (uint32_t)val.toInt();
    if (key == "grenzwert") entkalkerGrenzwert = (uint32_t)val.toInt();
    if (key == "alarm")     entkalkerAlarm     = (val.toInt() != 0);
  }
  f.close();

  // Entkalker-Anzeige auf Hauptseite mit dem Wechselzaehler synchron halten
  peers[1].zaehler = entkalkerVerbrauch;
}

void resetEntkalker() {
  entkalkerVerbrauch  = 0;
  entkalkerAlarm      = false;
  lastEntkalkerPushMillis = 0;
  saveEntkalkerState();
  peers[1].zaehler = 0;
  saveCounterState();
}

void checkEntkalkerAlarm() {
  if (!entkalkerAlarm) return;

  const bool cooldownPassed =
    (lastEntkalkerPushMillis == 0) ||
    (millis() - lastEntkalkerPushMillis >= 86400000UL); // 1x pro Tag

  if (cooldownPassed) {
    sendPushover("ACHTUNG: Entkalker wechseln!");
    lastEntkalkerPushMillis = millis();
  }
}

// ===== ESP-NOW =====
void onDataSent(const uint8_t *mac, esp_now_send_status_t status) {
  for (int i = 0; i < 3; i++) {
    if (memcmp(mac, peers[i].mac, 6) == 0) {
      if (status == ESP_NOW_SEND_SUCCESS) {
        peers[i].lastSeen = millis();
        peers[i].online = true;
      }
    }
  }
}

void onDataRecv(const uint8_t *mac, const uint8_t *data, int len) {
  if (len != sizeof(WasserDaten)) return;

  WasserDaten empfangen;
  memcpy(&empfangen, data, sizeof(WasserDaten));

  int idx = empfangen.peerID - 2;
  if (idx < 0 || idx >= 3) return;

  uint32_t impulseBlock = empfangen.zaehler;

  peers[idx].zaehler += impulseBlock;
  peers[idx].online = true;
  peers[idx].neuerWert = true;
  peers[idx].lastSeen = millis();

  Serial.printf("Empfangen von %s: +%lu Impulse, Gesamt=%lu Liter\n",
                peers[idx].name,
                impulseBlock,
                peers[idx].zaehler);

  // Dauerlauf-Tracking für Hauptwasseruhr (Peer 0)
  if (idx == 0 && impulseBlock > 0) {
    if (haupt_flussStart == 0) haupt_flussStart = millis();
    haupt_flussZuletzt = millis();
  }

  // Entkalker-Verbrauch tracken (Peer 1 = Entkalkeruhr)
  if (idx == 1 && impulseBlock > 0) {
    entkalkerVerbrauch += impulseBlock;
    peers[idx].zaehler = entkalkerVerbrauch;
    if (!entkalkerAlarm && entkalkerVerbrauch >= entkalkerGrenzwert) {
      entkalkerAlarm = true;
    }
    saveEntkalkerState();
  }

  if (impulseBlock > 0 && timeValid) {
    struct tm timeinfo;
    if (getLocalTime(&timeinfo)) {
      const bool isNightWindow = (timeinfo.tm_hour >= 1 && timeinfo.tm_hour < 5);
      const bool cooldownPassed =
        (lastNightAlarmMillis == 0) ||
        (millis() - lastNightAlarmMillis >= 600000UL);

      if (isNightWindow && cooldownPassed) {
        sendPushover("WASSERALARM NACHT - Wasser laeuft!");
        lastNightAlarmMillis = millis();
      }
    }
  }

  logToSD(idx, impulseBlock, peers[idx].zaehler);
  saveCounterState();
}

void checkDauerlaufAlarm() {
  if (haupt_flussStart == 0) return;

  // Kein Impuls seit 90 Sekunden => Fluss gestoppt
  if (millis() - haupt_flussZuletzt > 90000UL) {
    haupt_flussStart  = 0;
    haupt_flussZuletzt = 0;
    return;
  }

  // Noch keine 20 Minuten Dauerfluss
  if (millis() - haupt_flussStart < 1200000UL) return;

  // Cooldown 10 Minuten
  const bool cooldownPassed =
    (lastDauerlaufAlarmMillis == 0) ||
    (millis() - lastDauerlaufAlarmMillis >= 600000UL);
  if (!cooldownPassed) return;

  // Display-Alarm
  tft.fillRect(0, 200, 320, 40, TFT_RED);
  tft.setTextDatum(MC_DATUM);
  tft.setTextFont(2);
  tft.setTextColor(TFT_WHITE, TFT_RED);
  tft.drawString("ALARM: Wasser laeuft seit 20 Min!", 160, 220);

  // Pushover
  sendPushover("ALARM: Wasser laeuft seit 20 Minuten - moeglicher Wasserbruch!");
  lastDauerlaufAlarmMillis = millis();
}

// ===== Initialisierung =====
void initDisplayAndTouch() {
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);

  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);
  Serial.println("Display OK");

  SPI.begin(TOUCH_CLK, TOUCH_MISO, TOUCH_MOSI, TOUCH_CS);
  ts.begin();
  ts.setRotation(1);
  Serial.println("Touch OK");
  lastTouch = millis() + 3000;
}

void initSDCard() {
  sdSPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
  if (SD.begin(SD_CS, sdSPI)) {
    sdOK = true;
    Serial.println("SD-Karte OK");
    ensureWaterLogFile();
    ensureCounterFile();
    loadCounterState();
    loadEntkalkerState();
    Serial.println("Zählerstände geladen");
  } else {
    sdOK = false;
    Serial.println("SD-Karte FEHLER!");
  }
}

void initWiFiAndTime() {
  WiFi.mode(WIFI_AP_STA);
  Serial.printf("Verbinde mit WLAN: %s\n", ssid);
  WiFi.begin(ssid, wlanPass);

  int timeout = 0;
  while (WiFi.status() != WL_CONNECTED && timeout < 20) {
    delay(500);
    timeout++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("WLAN OK! IP: %s\n", WiFi.localIP().toString().c_str());

    configTzTime("CET-1CEST,M3.5.0,M10.5.0/3", "pool.ntp.org", "time.nist.gov");

    for (int i = 0; i < 20; i++) {
      checkTimeValid();
      if (timeValid) break;
      delay(500);
    }

    if (timeValid) {
      Serial.printf("Zeit OK: %s\n", getTimeString().c_str());
    } else {
      Serial.println("Zeit noch nicht korrekt synchronisiert");
    }
  } else {
    Serial.println("WLAN fehlgeschlagen!");
  }
}

void initEspNow() {
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
}

void initWebServer() {
  server.on("/", handleRoot);
  server.on("/log", handleLogPage);
  server.on("/download", handleDownloadCSV);
  server.on("/clearlog", handleClearLog);
  server.begin();
  Serial.println("Webserver OK");
}