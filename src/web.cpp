#include "web.h"
#include "logic.h"

void handleRoot() {
  String zeitstempel = getTimeString();

  String html = "<!DOCTYPE html><html><head><meta charset='UTF-8'>"
    "<meta http-equiv='refresh' content='5'>"
    "<meta name='viewport' content='width=device-width, initial-scale=1'>"
    "<title>WasserKontrolle</title>"
    "<style>"
    "body{font-family:Arial;background:#1a1a2e;color:#eee;padding:20px;margin:0}"
    "h1{text-align:center;color:#00d4ff;margin-top:10px}"
    ".sub{text-align:center;color:#aaa;font-size:0.9em;margin-bottom:20px}"
    ".wrap{max-width:800px;margin:0 auto}"
    ".card{background:#16213e;border-radius:12px;padding:16px;margin:14px 0;display:flex;justify-content:space-between;align-items:center}"
    ".on{border-left:5px solid #00cc88}.off{border-left:5px solid #cc3344}"
    ".name{font-weight:bold;font-size:1.05em}"
    ".liter{color:#00d4ff;font-size:1.4em;margin-top:8px}"
    ".status{padding:8px 14px;border-radius:18px;font-weight:bold;min-width:90px;text-align:center}"
    ".statusOn{background:#00ff8833;color:#00ff99}"
    ".statusOff{background:#ff444433;color:#ff6666}"
    ".btnbar{text-align:center;margin:30px 0}"
    ".btn{display:inline-block;margin:8px;padding:12px 18px;border-radius:10px;text-decoration:none;font-weight:bold}"
    ".btn1{background:#1f6feb;color:white}"
    ".btn2{background:#0f766e;color:white}"
    ".btn3{background:#b91c1c;color:white}"
    ".foot{text-align:center;color:#666;font-size:0.85em;margin-top:30px}"
    "</style></head><body>";

  html += "<div class='wrap'>";
  html += "<h1>&#128167; WasserKontrolle</h1>";
  html += "<div class='sub'>Stand: " + zeitstempel + "</div>";

  for (int i = 0; i < 3; i++) {
    bool isOnline = peerIsOnline(i);
    const char* cardCls   = isOnline ? "on" : "off";
    const char* statusCls = isOnline ? "status statusOn" : "status statusOff";
    const char* statusTxt = isOnline ? "ONLINE" : "OFFLINE";

    html += "<div class='card " + String(cardCls) + "'>";
    html += "<div>";
    html += "<div class='name'>" + String(peers[i].name) + "</div>";
    html += "<div class='liter'>" + String(peers[i].zaehler) + " Liter</div>";
    html += "</div>";
    html += "<div class='" + String(statusCls) + "'>" + String(statusTxt) + "</div>";
    html += "</div>";
  }

  html += "<div class='btnbar'>";
  html += "<a class='btn btn1' href='/log'>CSV anzeigen</a>";
  html += "<a class='btn btn2' href='/download'>CSV herunterladen</a>";
  html += "<a class='btn btn3' href='/clearlog' onclick=\"return confirm('Wasserlog wirklich loeschen?');\">Log loeschen</a>";
  html += "</div>";

  html += "<div class='foot'>Auto-Refresh 5s | IP: " + WiFi.localIP().toString() + "</div>";
  html += "</div></body></html>";

  server.send(200, "text/html", html);
}

void handleLogPage() {
  if (!sdOK) {
    server.send(200, "text/html",
      "<html><head><meta charset='UTF-8'></head><body>"
      "<h2>Keine SD-Karte verfügbar</h2>"
      "<p><a href='/'>Zurück</a></p>"
      "</body></html>");
    return;
  }

  File f = SD.open(WATER_LOG_FILE, FILE_READ);
  if (!f) {
    server.send(200, "text/html",
      "<html><head><meta charset='UTF-8'></head><body>"
      "<h2>Logdatei nicht gefunden</h2>"
      "<p><a href='/'>Zurück</a></p>"
      "</body></html>");
    return;
  }

  String html = "<!DOCTYPE html><html><head><meta charset='UTF-8'>"
                "<meta name='viewport' content='width=device-width, initial-scale=1'>"
                "<title>Wasserlog CSV</title>"
                "<style>"
                "body{font-family:Arial;background:#1a1a2e;color:#eee;padding:20px}"
                "h1{color:#00d4ff}"
                "pre{background:#111;padding:15px;border-radius:10px;white-space:pre-wrap;word-wrap:break-word}"
                "a{color:#8fd3ff}"
                "</style></head><body>";

  html += "<h1>Wasserlog CSV</h1>";
  html += "<p><a href='/'>Zurück zur Hauptseite</a> | <a href='/download'>CSV herunterladen</a></p>";
  html += "<pre>";

  while (f.available()) {
    String line = f.readStringUntil('\n');
    line.trim();
    html += htmlEscape(line);
    html += "\n";
  }

  html += "</pre></body></html>";
  f.close();

  server.send(200, "text/html", html);
}

void handleDownloadCSV() {
  if (!sdOK) {
    server.send(404, "text/plain", "Keine SD-Karte verfügbar");
    return;
  }

  File f = SD.open(WATER_LOG_FILE, FILE_READ);
  if (!f) {
    server.send(404, "text/plain", "Logdatei nicht gefunden");
    return;
  }

  server.sendHeader("Content-Type", "text/csv; charset=utf-8");
  server.sendHeader("Content-Disposition", "attachment; filename=wasserlog.csv");
  server.streamFile(f, "text/csv");
  f.close();
}

void handleClearLog() {
  if (sdOK) {
    clearWaterLog();
  }
  server.sendHeader("Location", "/", true);
  server.send(302, "text/plain", "");
}