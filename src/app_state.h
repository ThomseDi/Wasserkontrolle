#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <WebServer.h>
#include <time.h>
#include <SPI.h>
#include <SD.h>
#include <TFT_eSPI.h>

#define FW_VERSION "v1.12"

// ===== Pins =====
#define TFT_BL     21
#define SD_CS      5
#define SD_MOSI    23
#define SD_MISO    19
#define SD_SCK     18

#define TOUCH_CS   33
#define TOUCH_IRQ  36
#define TOUCH_MOSI 32
#define TOUCH_MISO 39
#define TOUCH_CLK  25

// ===== WLAN =====
extern const char* ssid;
extern const char* wlanPass;
extern String startupStaticIP;
extern String startupSSID;
extern String startupPass;
extern int keyboardContext;

#define KBCTX_NONE 0
#define KBCTX_STARTUP_IP   1
#define KBCTX_STARTUP_SSID 2
#define KBCTX_STARTUP_PASS 3

// ===== Dateien =====
extern const char* WATER_LOG_FILE;
extern const char* COUNTER_FILE;
extern const time_t MIN_VALID_TIME;

// ===== SPI / Hardware =====
extern TFT_eSPI tft;
extern SPIClass sdSPI;
extern WebServer server;

#include <XPT2046_Touchscreen.h>
extern XPT2046_Touchscreen ts;

// ===== Status =====
extern bool sdOK;
extern bool lastTouched;
extern unsigned long lastTouch;
extern bool timeValid;
extern unsigned long lastClockCheck;

// ===== Touch-Kalibrierung =====
extern int16_t calX0, calY0, calX1, calY1;

// ===== Seiten =====
enum Page {
  PAGE_MAIN,
  PAGE_KEYBOARD,
  PAGE_FILES,
  PAGE_VIEW,
  PAGE_SDLOG,
  PAGE_OFFSET,
  PAGE_ENTKALKER
};
extern Page currentPage;

// ===== Entkalker-Alarm =====
extern uint32_t entkalkerVerbrauch;   // Liter seit letztem Wechsel
extern uint32_t entkalkerGrenzwert;   // Alarm-Grenzwert in Litern
extern bool     entkalkerAlarm;

// ===== Texteingabe =====
extern String inputText;
extern const int INPUT_MAX_LEN;

// ===== Offset =====
extern int selectedPeer;

// ===== Dateianzeige =====
extern String viewText;
extern String viewTitle;

// ===== Dateiliste =====
#define MAX_FILES 10
extern String fileNames[MAX_FILES];
extern int fileCount;
extern int fileScrollOffset;

// ===== Keyboard =====
extern bool shiftOn;
extern bool numMode;
extern const char* kbRow1;
extern const char* kbRow2;
extern const char* kbRow3;

#define KB_KEY_W  28
#define KB_KEY_H  32
#define KB_GAP    2
#define KB_STEP   (KB_KEY_W + KB_GAP)
#define KB_Y_START 100

// ===== MACs =====
extern uint8_t mac_haupt[6];
extern uint8_t mac_entkalker[6];
extern uint8_t mac_offen[6];

// ===== Peer-Status =====
struct PeerInfo {
  const char* name;
  uint8_t* mac;
  bool online;
  uint32_t zaehler;
  bool neuerWert;
  unsigned long lastSeen;
};

extern PeerInfo peers[3];

// ===== Datenpaket =====
struct WasserDaten {
  uint8_t  peerID;
  uint32_t zaehler;
};