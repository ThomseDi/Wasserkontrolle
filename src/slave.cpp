#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>

#define LED_PIN 2
#define IMPULS_PIN 27

const char* routerSSID = "MOSTKRUG2.4";

const unsigned long CHANNEL_RECHECK_INTERVAL_MS = 300000;
const uint8_t SEND_FAILURE_THRESHOLD = 3;

// ===== MAC Master =====
uint8_t masterMAC[] = {0xB0, 0xCB, 0xD8, 0xDB, 0x06, 0x04};

// ===== Slave MACs =====
uint8_t slaveMacs[][6] = {
  {0xE0, 0x8C, 0xFE, 0x58, 0x5D, 0x7C},
  {0xE0, 0x8C, 0xFE, 0x59, 0x59, 0x94},
  {0xE0, 0x8C, 0xFE, 0xB6, 0xEB, 0x34}
};

uint8_t myPeerID = 0;
uint8_t currentChannel = 1;
unsigned long lastChannelCheckTime = 0;
volatile uint8_t consecutiveSendFailures = 0;
volatile bool forceChannelRescan = false;

// ===== Datenpaket =====
struct WasserDaten {
  uint8_t  peerID;
  uint32_t pulseCount;
};

// ===== Debouncing Parameter =====
#define MIN_PULSE_INTERVAL_MS 300  // Mindestverzögerung zwischen Impulsen
#define STABLE_TIME_MS 20          // Signal muss mind. 20 ms stabil sein

// ===== Impulszähler =====
volatile uint32_t pulseCount = 0;
volatile unsigned long lastValidPulseTime = 0;

uint32_t lastSentCount = 0;
unsigned long lastSendTime = 0;

uint8_t findRouterChannel() {
  uint8_t channel = 0;
  int networkCount = WiFi.scanNetworks();

  for (int i = 0; i < networkCount; i++) {
    if (WiFi.SSID(i) == routerSSID) {
      channel = WiFi.channel(i);
      break;
    }
  }

  WiFi.scanDelete();
  return channel;
}

bool applyEspNowChannel(uint8_t channel) {
  if (channel == 0) {
    Serial.println("Router nicht gefunden, Kanal bleibt unveraendert");
    return false;
  }

  if (channel == currentChannel) {
    return true;
  }

  esp_now_del_peer(masterMAC);
  esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);

  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, masterMAC, 6);
  peerInfo.channel = channel;
  peerInfo.encrypt = false;

  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("ESP-NOW Peer Update Fehler");
    return false;
  }

  currentChannel = channel;
  Serial.printf("Kanal gesetzt: %u\n", currentChannel);
  return true;
}

bool refreshChannel(bool forceScan = false) {
  if (!forceScan && (millis() - lastChannelCheckTime) < CHANNEL_RECHECK_INTERVAL_MS) {
    return false;
  }

  lastChannelCheckTime = millis();

  uint8_t foundChannel = findRouterChannel();
  if (foundChannel == 0) {
    return false;
  }

  return applyEspNowChannel(foundChannel);
}

// ===== Interrupt =====
void IRAM_ATTR onPulse() {
  unsigned long now = millis();

  // Nur zählen wenn Mindestverzögerung eingehalten ist
  // 300 ms ist lang genug, dass 20 ms Stabilität implizit erfüllt ist
  if (now - lastValidPulseTime >= MIN_PULSE_INTERVAL_MS) {
    pulseCount++;
    lastValidPulseTime = now;
  }
}

// ===== Empfang =====
void onDataRecv(const uint8_t *mac, const uint8_t *data, int len) {
  digitalWrite(LED_PIN, HIGH);
  delay(20);
  digitalWrite(LED_PIN, LOW);
}

void onDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  if (status == ESP_NOW_SEND_SUCCESS) {
    consecutiveSendFailures = 0;
    return;
  }

  if (consecutiveSendFailures < 255) {
    consecutiveSendFailures++;
  }

  if (consecutiveSendFailures >= SEND_FAILURE_THRESHOLD) {
    forceChannelRescan = true;
  }
}

// ===== Senden =====
void sendeZaehler(uint32_t count) {
  WasserDaten daten;
  daten.peerID = myPeerID;
  daten.pulseCount = count;

  esp_err_t result = esp_now_send(masterMAC, (uint8_t*)&daten, sizeof(daten));
  if (result != ESP_OK) {
    Serial.printf("ESP-NOW Sendefehler: %d\n", result);
  }

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
  uint8_t channel = findRouterChannel();
  if (channel == 0) {
    channel = 1;
  }
  currentChannel = channel;
  esp_wifi_set_channel(currentChannel, WIFI_SECOND_CHAN_NONE);

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
  peerInfo.channel = currentChannel;
  peerInfo.encrypt = false;

  esp_now_add_peer(&peerInfo);

  esp_now_register_recv_cb(onDataRecv);
  esp_now_register_send_cb(onDataSent);

  Serial.println("Bereit für echte Impulse");
}

// ===== LOOP =====
void loop() {

  if (forceChannelRescan) {
    forceChannelRescan = false;
    refreshChannel(true);
  } else {
    refreshChannel(false);
  }

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