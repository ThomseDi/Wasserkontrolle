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
- Entprellung: `MIN_PULSE_INTERVAL_MS = 300 ms`
- Sendeintervall: `1 Sekunde`
- Peer-Zuordnung: über MAC-Adresse auf PeerID
- Kanalquelle fuer ESP-NOW: SSID `MOSTKRUG2.4`
- Automatische Kanal-Nachfuehrung:
  - periodischer Re-Check alle 5 Minuten
  - zusaetzlicher Sofort-Check nach mehreren Sendefehlern

## 6) Zusammenfassung Gesamtfunktion (Main + Slave)

Das System besteht aus einem zentralen Master (`src/main.cpp`) und mehreren dezentralen Slaves (`src/slave.cpp`) an den Wasserzaehlern.

### Gesamtablauf
1. Jeder Slave erfasst Impulse lokal am Zaehler per Interrupt und entprellt diese.
2. Der Slave sendet die seit der letzten Uebertragung neu gezaehlten Impulse per ESP-NOW an den Master.
3. Der Master sammelt die Daten aller Slaves, rechnet in Liter, setzt Online/Offline-Status und prueft Alarme (z. B. Dauerlauf, Entkalker).
4. Der Master zeigt Live-Werte auf dem TFT, speichert Daten auf SD (CSV) und stellt sie ueber den Webserver bereit.
5. Updates am Master erfolgen per OTA; Slaves werden per USB geflasht.

### Rolle der Komponenten
- Master: Anzeige, Logik, Alarme, Weboberflaeche, SD-Logging, OTA.
- Slave: robuste Impulsaufnahme, Zaehlung und ESP-NOW-Uebertragung.

### WLAN-/Kanalverhalten
- ESP-NOW funktioniert nur stabil, wenn Master und Slave auf demselben WLAN-Kanal arbeiten.
- Slaves orientieren sich an der SSID `MOSTKRUG2.4` und koennen den Kanal bei Aenderungen automatisch nachziehen.

## 7) Seiten-/Zustandsmodell

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

## 8) Bedienablauf im Alltag

### Normalbetrieb
1. Router einschalten und stabil laufen lassen.
2. Master starten.
3. Slaves starten.
4. Auf dem Display pruefen, ob alle relevanten Peers ONLINE sind.
5. Optional im Browser den Live-Status pruefen.

### Funktionstest (kurz)
1. Kurz Wasser laufen lassen.
2. Pruefen, ob Literwerte auf dem Master hochzaehlen.
3. Pruefen, ob Eintrag im SD-Log erscheint.
4. Pruefen, ob Browseransicht aktualisiert wird.

### Fehlerfall: Slave OFFLINE
1. WLAN-Status am Master pruefen.
2. Master und Slave einmal neu starten.
3. Sicherstellen, dass die Kanalquelle `MOSTKRUG2.4` sichtbar ist.
4. Wenn weiter OFFLINE: Slave per USB neu flashen und erneut testen.

### Fehlerfall: Keine Logdaten auf SD
1. SD-Karte und `sdOK` pruefen.
2. SD-Karte neu einstecken und Master neu starten.
3. Speicherplatz/Dateizugriff pruefen (CSV vorhanden, nicht schreibgeschuetzt).

### Update-Empfehlung
1. Master-Updates per OTA einspielen.
2. Slave-Updates per USB je Geraet einspielen.
3. Nach Slave-Update kurzen Impulstest durchfuehren.

---

Stand: 18.05.2026
