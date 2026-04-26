# Slave Kurz-Anleitung (ESP32)

Diese Kurz-Anleitung ist für den Wasser-Slave (z. B. Entkalker) in diesem Projekt.

## 1) Welche Firmware ist richtig?

- Environment: `slave`
- Upload-Port (aktuell): `COM4`
- Quellcode: `src/slave.cpp`

Hinweis:
- Das Environment `master` nutzt **nicht** den Slave-Code.
- Für den Slave immer `slave` wählen.

## 2) Verdrahtung (Impuls-Eingang)

- Impuls-Pin: `GPIO27`
- GND: gemeinsame Masse mit Sensor
- Interner Pullup ist aktiv (`INPUT_PULLUP`)

Typischer Anschluss bei potentialfreiem Kontakt/Reed:
- Ein Kontakt an `GPIO27`
- Anderer Kontakt an `GND`

Der Impuls wird bei fallender Flanke erkannt (`FALLING`).

## 3) Upload (PlatformIO)

Befehl:

```powershell
& "C:\Users\Thomas\.platformio\penv\Scripts\platformio.exe" run --target upload --environment slave --upload-port COM4
```

Wenn Fehler `Wrong boot mode detected` kommt:
1. `BOOT` gedrückt halten
2. kurz `EN/RESET` drücken
3. `BOOT` halten bis der Upload startet
4. dann loslassen

Wenn Fehler `COM4 busy` kommt:
1. alle seriellen Monitore schließen
2. USB kurz ab- und anstecken
3. Upload erneut starten

## 4) Live-Test im seriellen Monitor

Befehl:

```powershell
& "C:\Users\Thomas\.platformio\penv\Scripts\platformio.exe" device monitor -p COM4 -b 115200
```

Erwartete Meldungen:
- `PeerID: 3` (beim Entkalker-Slave, wenn MAC korrekt erkannt)
- `Bereit fuer echte Impulse`
- bei Impulsen: `Gesendet: PeerID=..., Impulse=...`

## 5) Schnelltest ohne Zaehler

Kurz Bruecke zwischen `GPIO27` und `GND` antippen:
- kurz verbinden, wieder loesen
- jeder Flankenwechsel kann Impulse erzeugen

Hinweise:
- Dauerhaftes Verbinden erzeugt nicht dauerhaft neue Impulse.
- Entprellung ist auf 5 ms gesetzt.

## 6) Zuordnung der Slaves

Die Peer-Zuordnung erfolgt im Slave ueber MAC-Adresse.
Wenn eine MAC nicht erkannt wird, faellt der Code auf PeerID 2 zurueck.

Pruefung:
- Im seriellen Monitor muss fuer Entkalker `PeerID: 3` erscheinen.

---
Stand: 26.04.2026
