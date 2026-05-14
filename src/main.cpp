#include <Arduino.h>
#include "app_state.h"
#include "logic.h"
#include "display.h"
#include "web.h"
#include "ota_update.h"

static void drawStartupProfilePage() {
  tft.fillScreen(TFT_BLACK);
  tft.setTextDatum(TC_DATUM);
  tft.setTextFont(2);
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.drawString("WLAN-Profil waehlen", 160, 12);

  drawBtnBig(15, 40, 140, 45, "PROFIL 1", TFT_DARKGREEN);
  drawBtnBig(165, 40, 140, 45, "PROFIL 2", TFT_NAVY);

  tft.setTextDatum(TL_DATUM);
  tft.setTextFont(1);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString("P1: MOSTKRUG2.4", 15, 95);
  tft.drawString("PW: mostkrug2025", 15, 107);
  tft.drawString("IP: 192.168.2.75", 15, 119);

  tft.drawString("P2: 6360Achalmstr", 165, 95);
  tft.drawString("PW: mostkrug2011", 165, 107);
  tft.drawString("IP: 192.168.1.187", 165, 119);

  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.drawString("Danach koennen SSID/Pass/IP noch geaendert werden.", 12, 150);
  tft.drawString("Tippe auf PROFIL 1 oder PROFIL 2.", 12, 164);
}

static void drawStartupStatus(const char* msg, uint16_t color = TFT_YELLOW) {
  tft.fillRect(0, 206, 320, 34, TFT_BLACK);
  tft.drawFastHLine(0, 205, 320, TFT_DARKGREY);
  tft.setTextDatum(TL_DATUM);
  tft.setTextFont(1);
  tft.setTextColor(color, TFT_BLACK);
  tft.drawString(msg, 6, 216);
}

void setup() {
  delay(2000);
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n=== WasserKontrolle ===\n");

  initDisplayAndTouch();

  const bool startupConfigLoaded = loadStartupConfig();

  if (startupConfigLoaded) {
    drawStartupStatus("Gespeicherte WLAN-Daten geladen", TFT_GREEN);
  } else {
    startupSSID = "MOSTKRUG2.4";
    startupPass = "mostkrug2025";
    startupStaticIP = "192.168.2.75";
    saveStartupConfig();
    drawStartupStatus("Keine gespeicherten Daten - Profil 1 geladen", TFT_GREEN);
  }

  initSDCard();

  tft.setTextDatum(MC_DATUM);
  tft.setTextFont(2);
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.drawString("Verbinde mit WLAN...", 160, 120);

  initWiFiAndTime();
  initOTA();
  initEspNow();
  initWebServer();

  drawMainPage();
  Serial.println("Setup fertig!\n");
}

void loop() {
  handleOTA();
  server.handleClient();
  checkAndReconnectWiFi();

  if (millis() - lastClockCheck > 10000) {
    lastClockCheck = millis();
    checkTimeValid();
  }

  checkDauerlaufAlarm();
  checkEntkalkerAlarm();

  for (int i = 0; i < 3; i++) {
    peers[i].online = peerIsOnline(i);
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
      case PAGE_OFFSET:   handleOffsetPage(tx, ty); break;
      case PAGE_ENTKALKER: handleEntkalkerPage(tx, ty); break;
    }

    static Page lastPage = PAGE_MAIN;
    if (currentPage != lastPage) {
      switch (currentPage) {
        case PAGE_MAIN:     drawMainPage(); break;
        case PAGE_KEYBOARD: numMode = false; drawKeyboardPage(); break;
        case PAGE_FILES:    fileScrollOffset = 0; loadFileList(); drawFilesPage(); break;
        case PAGE_VIEW:     drawViewPage(); break;
        case PAGE_SDLOG:    drawSDLogPage(); break;
        case PAGE_OFFSET:   drawOffsetPage(); break;
        case PAGE_ENTKALKER: drawEntkalkerPage(); break;
      }
      lastPage = currentPage;
    }
  }

  static unsigned long lastPeerPing = 0;
  static unsigned long lastMainRefresh = 0;

  if (currentPage == PAGE_MAIN && millis() - lastMainRefresh > 250) {
    lastMainRefresh = millis();
    updatePeerStatus();
  }

  if (currentPage == PAGE_MAIN && millis() - lastPeerPing > 2000) {
    lastPeerPing = millis();

    uint8_t ping = 1;
    for (int i = 0; i < 3; i++) {
      esp_now_send(peers[i].mac, &ping, sizeof(ping));
      delay(20);
    }
  }

  delay(50);
}