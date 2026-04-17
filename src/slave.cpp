#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>

#define LED_PIN 2           // Onboard-LED
#define IMPULS_PIN 27       // GPIO für Wasseruhr-Impuls (später Reed-Kontakt)

// WLAN-SSID nur zum Kanal ermitteln (kein Passwort nötig)
const char* routerSSID = "6360Achalmstr";

// ===== MAC des Masters =====
uint8_t masterMAC[] = {0xB0, 0xCB, 0xD8, 0xDB, 0x06, 0x04};

// ===== MAC-Adressen der Slaves (zur automatischen PeerID-Erkennung) =====
// ESP32 #2 Hauptwasseruhr: E0:8C:FE:58:5D:7C -> peerID 2
// ESP32 #3 Entkalkeruhr:    E0:8C:FE:59:59:94 -> peerID 3
// ESP32 #4 (offen):         E0:8C:FE:B6:EB:34 -> peerID 4
uint8_t slaveMacs[][6] = {
  {0xE0, 0x8C, 0xFE, 0x58, 0x5D, 0x7C},  // peerID 2
  {0xE0, 0x8C, 0xFE, 0x59, 0x59, 0x94},  // peerID 3
  {0xE0, 0x8C, 0xFE, 0xB6, 0xEB, 0x34}   // peerID 4
};
uint8_t myPeerID = 0;  // wird im Setup automatisch ermittelt

// ===== Datenpaket (muss auf Master identisch sein) =====
struct WasserDaten {
  uint8_t  peerID;      // 2=Hauptwasseruhr, 3=Entkalker, 4=offen
  uint32_t zaehler;     // Liter-Zählerstand
};

volatile uint32_t impulseCount = 0;
uint32_t lastSentCount = 0;

// Simulation: 30 Impulse erzeugen
bool simAktiv = true;
uint32_t simZaehler = 0;
const uint32_t SIM_MAX = 30;
unsigned long letzterImpuls = 0;

// ===== Empfangs-Callback (Ping vom Master) =====
void onDataRecv(const uint8_t *mac, const uint8_t *data, int len) {
  digitalWrite(LED_PIN, HIGH);
  delay(50);
  digitalWrite(LED_PIN, LOW);
}

// ===== Sende-Callback =====
void onDataSent(const uint8_t *mac, esp_now_send_status_t status) {
  Serial.printf("Sende-Status: %s\n", status == ESP_NOW_SEND_SUCCESS ? "OK" : "FEHLER");
}

// ===== Zählerstand an Master senden =====
void sendeZaehler() {
  WasserDaten daten;
  daten.peerID = myPeerID;
  daten.zaehler = impulseCount;

  esp_now_send(masterMAC, (uint8_t*)&daten, sizeof(daten));
  Serial.printf("Gesendet: PeerID=%d, Zaehler=%lu Liter\n", daten.peerID, daten.zaehler);
}

// ===== SETUP =====
void setup() {
  Serial.begin(115200);
  delay(1000);

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  pinMode(IMPULS_PIN, INPUT_PULLUP);

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  // Router-Kanal automatisch ermitteln per WiFi-Scan
  uint8_t espNowChannel = 1; // Fallback
  int n = WiFi.scanNetworks();
  for (int i = 0; i < n; i++) {
    if (WiFi.SSID(i) == routerSSID) {
      espNowChannel = WiFi.channel(i);
      Serial.printf("Router '%s' gefunden auf Kanal %d\n", routerSSID, espNowChannel);
      break;
    }
  }
  WiFi.scanDelete();
  esp_wifi_set_channel(espNowChannel, WIFI_SECOND_CHAN_NONE);

  Serial.println("\n=== ESP-NOW Slave (Wasseruhr) ===");
  Serial.print("MAC: ");
  Serial.println(WiFi.macAddress());

  // PeerID automatisch anhand eigener MAC-Adresse ermitteln
  uint8_t mac[6];
  WiFi.macAddress(mac);
  for (int i = 0; i < 3; i++) {
    if (memcmp(mac, slaveMacs[i], 6) == 0) {
      myPeerID = i + 2;  // Index 0=peerID 2, Index 1=peerID 3, Index 2=peerID 4
      break;
    }
  }
  if (myPeerID == 0) {
    Serial.println("WARNUNG: MAC nicht erkannt! Verwende peerID=2");
    myPeerID = 2;
  }

  const char* peerNames[] = {"Hauptwasseruhr", "Entkalkeruhr", "(offen)"};
  Serial.printf("Erkannt als: %s (peerID=%d)\n", peerNames[myPeerID - 2], myPeerID);
  Serial.printf("Impuls-Pin: GPIO %d\n", IMPULS_PIN);
  Serial.printf("ESP-NOW Kanal: %d\n", espNowChannel);
  Serial.println("Simulation: 30 Impulse (1 pro Sekunde)");

  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW Init Fehler!");
    return;
  }

  // Master als Peer registrieren
  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, masterMAC, 6);
  peerInfo.channel = espNowChannel;
  peerInfo.encrypt = false;
  esp_now_add_peer(&peerInfo);

  esp_now_register_recv_cb(onDataRecv);
  esp_now_register_send_cb(onDataSent);

  Serial.println("Bereit, starte Simulation...");

  // LED kurz blinken = bereit
  for (int i = 0; i < 3; i++) {
    digitalWrite(LED_PIN, HIGH);
    delay(100);
    digitalWrite(LED_PIN, LOW);
    delay(100);
  }
}

// ===== LOOP =====
void loop() {
  // Simulation: 1 Impuls pro Sekunde, max 30
  if (simAktiv && millis() - letzterImpuls >= 1000) {
    letzterImpuls = millis();
    simZaehler++;
    impulseCount = simZaehler;

    Serial.printf("SIM Impuls #%lu\n", impulseCount);
    digitalWrite(LED_PIN, HIGH);
    delay(50);
    digitalWrite(LED_PIN, LOW);

    if (simZaehler >= SIM_MAX) {
      simAktiv = false;
      Serial.println("Simulation beendet (30 Impulse)");
    }
  }

  // Zählerstand senden wenn sich was geändert hat
  if (impulseCount != lastSentCount) {
    sendeZaehler();
    lastSentCount = impulseCount;
  }

  delay(100);
}
