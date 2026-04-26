# WasserKontrolleNeu - Funktionsdokumentation

Diese Doku beschreibt die wichtigsten Funktionen im Projekt, mit Fokus auf den Ablauf in `src/main.cpp` und die aufgerufenen Module.

## 1) Ablauf in `src/main.cpp`

### setup()
Startet und initialisiert das komplette System in dieser Reihenfolge:
1. Serielle Schnittstelle starten
2. Display und Touch initialisieren (`initDisplayAndTouch`)
3. SD-Karte initialisieren (`initSDCard`)
4. WLAN und Uhrzeit initialisieren (`initWiFiAndTime`)
5. ESP-NOW initialisieren (`initEspNow`)
6. Webserver initialisieren (`initWebServer`)
7. Hauptseite auf dem TFT zeichnen (`drawMainPage`)

### loop()
Die Endlosschleife führt zyklisch diese Aufgaben aus:
1. Webserver-Anfragen bedienen (`server.handleClient`)
2. Uhrzeit-Validität alle 10 Sekunden prüfen (`checkTimeValid`)
3. Alarme prüfen:
   - Dauerlauf-Alarm (`checkDauerlaufAlarm`)
   - Entkalker-Alarm (`checkEntkalkerAlarm`)
4. Online-Status der Peers aktualisieren (`peerIsOnline`)
5. Touch-Eingaben lesen und je nach Seite den passenden Handler aufrufen
6. Bei Seitenwechsel die neue Seite zeichnen
7. Auf der Hauptseite alle 2 Sekunden Peer-Status aktualisieren

## 2) Logikmodul `src/logic.cpp`

### Initialisierung
- `initDisplayAndTouch()`
  - TFT starten, Touch initialisieren, SPI-Pins konfigurieren
- `initSDCard()`
  - SD starten, Logdateien/Zählerdateien sicherstellen und laden
  - Entkalker-Status von SD laden
- `initWiFiAndTime()`
  - WLAN verbinden und NTP-Zeit konfigurieren
- `initEspNow()`
  - ESP-NOW starten, Peers anmelden, Callback-Funktionen setzen
- `initWebServer()`
  - HTTP-Routen registrieren und Server starten

### Daten und Persistenz
- `saveCounterState()` / `loadCounterState()`
  - Speichern/Laden der 3 Gesamtzähler
- `saveEntkalkerState()` / `loadEntkalkerState()`
  - Speichern/Laden von Entkalker-Verbrauch, Grenzwert und Alarmstatus
- `resetEntkalker()`
  - Setzt Entkalker-Verbrauch und Alarm zurück
  - Setzt zusätzlich den Entkalker-Peerzähler auf 0
  - Speichert Entkalker- und Zählerzustand auf SD
- `logToSD(...)`
  - Schreibt Messereignisse in `wasserlog.csv`

### ESP-NOW Callbacks
- `onDataRecv(...)`
  - Empfängt Impulsblöcke von Slaves
  - Addiert Impulse auf Peer-Zähler
  - Entkalker-Verbrauch wird bei Peer 1 separat geführt
  - Nachtalarm/Dauerlauf-Tracking wird aktualisiert
  - SD-Log und Zählerstände werden gespeichert
- `onDataSent(...)`
  - Markiert Peer bei erfolgreichem Senden als online

### Alarmfunktionen
- `checkDauerlaufAlarm()`
  - Erkennt durchgehenden Wasserfluss an der Hauptwasseruhr
  - Auslösung nach definierter Dauer mit Cooldown
- `checkEntkalkerAlarm()`
  - Prüft, ob Entkalker-Grenzwert erreicht ist
  - Optional Pushover-Versand mit 1x-pro-Tag-Begrenzung

## 3) Displaymodul `src/display.cpp`

### Seiten zeichnen
- `drawMainPage()`
  - Hauptansicht mit Peers, Status, Uhrzeit und Navigation
- `drawOffsetPage()`
  - Offset/Setzen eines Zählerstandes
- `drawKeyboardPage()` / `drawNumberPage()`
  - Texteingabe/Oberfläche
- `drawFilesPage()` / `drawViewPage()`
  - Dateiliste und Dateiansicht
- `drawSDLogPage()`
  - Letzte SD-Logeinträge
- `drawEntkalkerPage()`
  - Entkalker-Einstellungen, Grenzwerte, Reset

### Seiten-Handler
- `handleMainPage(tx, ty)`
- `handleOffsetPage(tx, ty)`
- `handleKeyboardPage(tx, ty)`
- `handleFilesPage(tx, ty)`
- `handleViewPage(tx, ty)`
- `handleSDLogPage(tx, ty)`
- `handleEntkalkerPage(tx, ty)`

Jeder Handler verarbeitet Touch-Klicks für genau eine Seite und setzt bei Bedarf `currentPage` auf die nächste Ansicht.

## 4) Webmodul `src/web.cpp`

- `handleRoot()`
  - Startseite mit Live-Status
- `handleLogPage()`
  - Anzeige des Wasserlogs
- `handleDownloadCSV()`
  - CSV-Download
- `handleClearLog()`
  - Löschen des Logs

## 5) Slave-Firmware `src/slave.cpp`

### Kernfunktion
- Liest Impulse am Eingangspin (Interrupt)
- Zählt Impulse mit Entprellung
- Sendet Differenzen zyklisch per ESP-NOW an den Master

### Wichtige Punkte
- Impuls-Pin: `GPIO 27`
- Entprellung: `5 ms`
- Sendeintervall: `1 Sekunde`
- Peer-Zuordnung: über MAC-Adresse auf PeerID

## 6) Seiten-/Zustandsmodell

### Page-Enum (`src/app_state.h`)
- `PAGE_MAIN`
- `PAGE_KEYBOARD`
- `PAGE_FILES`
- `PAGE_VIEW`
- `PAGE_SDLOG`
- `PAGE_OFFSET`
- `PAGE_ENTKALKER`

### Zentrale globale Zustände
- `peers[]` mit Online-Status und Zählerständen
- `currentPage` für Navigation
- `entkalkerVerbrauch`, `entkalkerGrenzwert`, `entkalkerAlarm`
- `sdOK`, `timeValid`

---

Stand: 26.04.2026
