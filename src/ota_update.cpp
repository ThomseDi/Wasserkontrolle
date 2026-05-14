#include "ota_update.h"

#include <Arduino.h>
#include <ArduinoOTA.h>

void initOTA() {
  ArduinoOTA.setHostname("yellow-display");

  ArduinoOTA.onStart([]() {
    const char* updateType = (ArduinoOTA.getCommand() == U_FLASH) ? "Sketch" : "Filesystem";
    Serial.printf("OTA Start: %s\n", updateType);
  });

  ArduinoOTA.onEnd([]() {
    // Kein manuelles Restart hier: ESP32-OTA beendet den Vorgang selbst,
    // sonst kann der finale OK-Handshake zum Uploader abbrechen.
    Serial.println("OTA Ende");
  });

  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("OTA Fortschritt: %u%%\r", (progress * 100U) / total);
  });

  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("OTA Fehler [%u]: ", error);
    if (error == OTA_AUTH_ERROR) {
      Serial.println("Auth fehlgeschlagen");
    } else if (error == OTA_BEGIN_ERROR) {
      Serial.println("Begin fehlgeschlagen");
    } else if (error == OTA_CONNECT_ERROR) {
      Serial.println("Connect fehlgeschlagen");
    } else if (error == OTA_RECEIVE_ERROR) {
      Serial.println("Receive fehlgeschlagen");
    } else if (error == OTA_END_ERROR) {
      Serial.println("End fehlgeschlagen");
    }
  });

  ArduinoOTA.begin();
  Serial.println("OTA bereit: yellow-display");
}

void handleOTA() {
  ArduinoOTA.handle();
}