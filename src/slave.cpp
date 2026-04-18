#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>

#define LED_PIN 2
#define IMPULS_PIN 27

const char* routerSSID = "6360Achalmstr";

// ===== MAC Master =====
uint8_t masterMAC[] = {0xB0, 0xCB, 0xD8, 0xDB, 0x06, 0x04};

// ===== Slave MACs =====
uint8_t slaveMacs[][6] = {
  {0xE0, 0x8C, 0xFE, 0x58, 0x5D, 0x7C},
  {0xE0, 0x8C, 0xFE, 0x59, 0x59, 0x94},
  {0xE0, 0x8C, 0xFE, 0xB6, 0xEB, 0x34}
};

uint8_t myPeerID = 0;

// ===== Datenpaket =====
struct WasserDaten {
  uint8_t  peerID;
  uint32_t pulseCount;
};

// ===== Impulszähler =====
volatile uint32_t pulseCount = 0;
volatile unsigned long lastPulseTime = 0;

uint32_t lastSentCount = 0;
unsigned long lastSendTime = 0;

// ===== Interrupt =====
void IRAM_ATTR onPulse() {
  unsigned long now = micros();

  // Entprellung (5 ms)
  if (now - lastPulseTime > 5000) {
    pulseCount++;
    lastPulseTime = now;
  }
}

// ===== Empfang =====
void onDataRecv(const uint8_t *mac, const uint8_t *data, int len) {
  digitalWrite(LED_PIN, HIGH);
  delay(20);
  digitalWrite(LED_PIN, LOW);
}

// ===== Senden =====
void sendeZaehler(uint32_t count) {
  WasserDaten daten;
  daten.peerID = myPeerID;
  daten.pulseCount = count;

  esp_now_send(masterMAC, (uint8_t*)&daten, sizeof(daten));

  Serial.printf("Gesendet: PeerID=%d, Impulse=%lu\n", daten.peerID, daten.pulseCount);
}

// ===== SETUP =====
void setup() {
  Serial.begin(115200);

  pinMode(LED_PIN, OUTPUT);
  pinMode(IMPULS_PIN, INPUT_PULLUP);

  attachInterrupt(digitalPinToInterrupt(IMPULS_PIN), onPulse, FALLING);

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  // Kanal suchen
  uint8_t channel = 1;
  int n = WiFi.scanNetworks();
  for (int i = 0; i < n; i++) {
    if (WiFi.SSID(i) == routerSSID) {
      channel = WiFi.channel(i);
      break;
    }
  }
  WiFi.scanDelete();
  esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);

  // MAC erkennen
  uint8_t mac[6];
  WiFi.macAddress(mac);

  for (int i = 0; i < 3; i++) {
    if (memcmp(mac, slaveMacs[i], 6) == 0) {
      myPeerID = i + 2;
      break;
    }
  }

  if (myPeerID == 0) myPeerID = 2;

  Serial.printf("PeerID: %d\n", myPeerID);

  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW Fehler");
    return;
  }

  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, masterMAC, 6);
  peerInfo.channel = channel;
  peerInfo.encrypt = false;

  esp_now_add_peer(&peerInfo);

  esp_now_register_recv_cb(onDataRecv);

  Serial.println("Bereit für echte Impulse");
}

// ===== LOOP =====
void loop() {

  uint32_t currentCount;

  noInterrupts();
  currentCount = pulseCount;
  interrupts();

  // alle 1 Sekunde senden (wenn neue Impulse da sind)
  if (millis() - lastSendTime > 1000) {

    if (currentCount != lastSentCount) {
      uint32_t diff = currentCount - lastSentCount;

      sendeZaehler(diff);

      lastSentCount = currentCount;
    }

    lastSendTime = millis();
  }
}