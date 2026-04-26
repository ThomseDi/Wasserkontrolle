#include <Arduino.h>
#include "app_state.h"
#include "logic.h"
#include "display.h"
#include "web.h"

void setup() {
  delay(2000);
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n=== WasserKontrolle ===\n");

  initDisplayAndTouch();
  initSDCard();

  tft.setTextDatum(MC_DATUM);
  tft.setTextFont(2);
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.drawString("Verbinde mit WLAN...", 160, 120);

  initWiFiAndTime();
  initEspNow();
  initWebServer();

  drawMainPage();
  Serial.println("Setup fertig!\n");
}

void loop() {
  server.handleClient();

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