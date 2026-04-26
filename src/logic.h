#pragma once
#include "app_state.h"

// Hilfsfunktionen
String htmlEscape(const String &s);
String getTimeString();
void checkTimeValid();
bool peerIsOnline(int i);
uint8_t getWiFiChannel();
void addPeer(uint8_t *mac);
int nextFileNumber();

// Zähler / SD
void ensureCounterFile();
void saveCounterState();
void loadCounterState();
void ensureWaterLogFile();
void logToSD(int peerIdx, uint32_t impulseBlock, uint32_t gesamtstand);
void clearWaterLog();

// ESP-NOW
void onDataSent(const uint8_t *mac, esp_now_send_status_t status);
void onDataRecv(const uint8_t *mac, const uint8_t *data, int len);

// Dauerlauf-Alarm
void checkDauerlaufAlarm();

// Entkalker-Alarm
void saveEntkalkerState();
void loadEntkalkerState();
void resetEntkalker();
void checkEntkalkerAlarm();

// Initialisierung
void initDisplayAndTouch();
void initSDCard();
void initWiFiAndTime();
void initEspNow();
void initWebServer();