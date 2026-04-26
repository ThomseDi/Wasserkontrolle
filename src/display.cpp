#include "display.h"
#include "logic.h"

// ===== Touch lesen =====
bool getTouch(int &tx, int &ty) {
  TS_Point p = ts.getPoint();
  bool touched = (p.z >= 400);
  if (p.x >= 8000 || p.y >= 8000 || p.x <= 100 || p.y <= 100) touched = false;

  if (touched && !lastTouched) {
    tx = map(p.x, calX0, calX1, 0, 319);
    ty = map(p.y, calY0, calY1, 0, 239);
    tx = constrain(tx, 0, 319);
    ty = constrain(ty, 0, 239);
    lastTouched = true;
    return true;
  }

  if (!touched) lastTouched = false;
  return false;
}

bool btnHit(int bx, int by, int bw, int bh, int tx, int ty) {
  return (tx >= bx && tx <= bx + bw && ty >= by && ty <= by + bh);
}

void drawBtn(int x, int y, int w, int h, const char* label, uint16_t color) {
  tft.fillRoundRect(x, y, w, h, 4, color);
  tft.drawRoundRect(x, y, w, h, 4, TFT_WHITE);
  tft.setTextDatum(MC_DATUM);
  tft.setTextSize(1);
  tft.setTextColor(TFT_WHITE, color);
  tft.drawString(label, x + w/2, y + h/2);
}

void drawBtnBig(int x, int y, int w, int h, const char* label, uint16_t color) {
  tft.fillRoundRect(x, y, w, h, 5, color);
  tft.drawRoundRect(x, y, w, h, 5, TFT_WHITE);
  tft.setTextDatum(MC_DATUM);
  tft.setTextSize(1);
  tft.setTextColor(TFT_WHITE, color);
  tft.drawString(label, x + w/2, y + h/2);
}

void drawWrappedText(String text, int x, int y, int maxW, uint16_t color) {
  tft.setTextDatum(TL_DATUM);
  tft.setTextSize(1);
  tft.setTextColor(color, TFT_BLACK);

  int cx = x, cy = y;
  String word = "";

  for (unsigned int i = 0; i <= text.length(); i++) {
    char c = (i < text.length()) ? text[i] : ' ';
    if (c == '\n') {
      tft.drawString(word, cx, cy);
      word = "";
      cx = x;
      cy += 12;
    } else if (c == ' ' || i == text.length()) {
      int ww = tft.textWidth(word + " ");
      if (cx + ww > x + maxW) {
        cx = x;
        cy += 12;
      }
      tft.drawString(word + " ", cx, cy);
      cx += ww;
      word = "";
    } else {
      word += c;
    }
  }
}

// ===== Hauptseite TFT =====
void drawMainPage() {
  tft.fillScreen(TFT_BLACK);

  tft.setTextDatum(TC_DATUM);
  tft.setTextSize(1);
  tft.setTextFont(2);
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.drawString("WasserKontrolle", 160, 2);

  drawBtnBig(5,   20, 60, 28, "OFFSET",  TFT_PURPLE);
  drawBtnBig(70,  20, 60, 28, "NOTIZ",   0x0400);
  drawBtnBig(135, 20, 60, 28, "DATEIEN", 0x000A);
  drawBtnBig(200, 20, 60, 28, "SD LOG",  TFT_NAVY);
  drawBtnBig(265, 20, 50, 28, "DEL",     TFT_MAROON);

  tft.drawFastHLine(0, 52, 320, TFT_DARKGREY);

  for (int i = 0; i < 3; i++) {
    int y = 56 + i * 55;

    tft.setTextDatum(TL_DATUM);
    tft.setTextFont(2);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString(peers[i].name, 10, y);

    bool isOnline = peerIsOnline(i);
    uint16_t col = isOnline ? TFT_GREEN : TFT_RED;
    tft.fillCircle(300, y + 7, 7, col);

    tft.setTextFont(4);
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    char litBuf[20];
    snprintf(litBuf, sizeof(litBuf), "%lu L", peers[i].zaehler);
    tft.drawString(litBuf, 10, y + 22);

    tft.setTextFont(2);
    tft.setTextColor(col, TFT_BLACK);
    tft.drawString(isOnline ? "ON" : "OFF", 270, y + 26);

    if (i < 2) tft.drawFastHLine(10, y + 50, 300, TFT_DARKGREY);
  }

  tft.setTextDatum(TL_DATUM);
  tft.setTextFont(1);
  if (WiFi.status() == WL_CONNECTED) {
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.drawString("IP: " + WiFi.localIP().toString(), 5, 225);
  }
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.drawString(getTimeString(), 160, 225);
}

void updatePeerStatus() {
  for (int i = 0; i < 3; i++) {
    int y = 56 + i * 55;
    bool isOnline = peerIsOnline(i);
    uint16_t col = isOnline ? TFT_GREEN : TFT_RED;

    tft.fillCircle(300, y + 7, 7, col);

    tft.fillRect(10, y + 22, 250, 24, TFT_BLACK);
    tft.setTextDatum(TL_DATUM);
    tft.setTextFont(4);
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    char litBuf[20];
    snprintf(litBuf, sizeof(litBuf), "%lu L", peers[i].zaehler);
    tft.drawString(litBuf, 10, y + 22);

    tft.fillRect(265, y + 26, 35, 16, TFT_BLACK);
    tft.setTextFont(2);
    tft.setTextColor(col, TFT_BLACK);
    tft.drawString(isOnline ? "ON" : "OFF", 270, y + 26);
  }

  tft.fillRect(160, 225, 160, 10, TFT_BLACK);
  tft.setTextDatum(TL_DATUM);
  tft.setTextFont(1);
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.drawString(getTimeString(), 160, 225);
}

void drawOffsetPage() {
  tft.fillScreen(TFT_BLACK);

  tft.setTextDatum(TC_DATUM);
  tft.setTextFont(2);
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.drawString("Offset setzen", 160, 8);

  tft.setTextDatum(TL_DATUM);
  tft.setTextFont(2);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString(peers[selectedPeer].name, 10, 35);

  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.drawString("Aktuell: " + String(peers[selectedPeer].zaehler) + " L", 10, 55);

  tft.fillRect(10, 80, 300, 32, TFT_DARKGREY);
  tft.drawRect(10, 80, 300, 32, TFT_WHITE);
  tft.setTextColor(TFT_YELLOW, TFT_DARKGREY);
  tft.drawString(inputText + "_", 15, 90);

  drawBtn(10, 120, 70, 26, "OK", TFT_GREEN);
  drawBtn(90, 120, 90, 26, "ABBRUCH", TFT_RED);
  drawBtn(190, 120, 55, 26, "DEL", TFT_ORANGE);
  drawBtn(255, 120, 55, 26, "CLR", TFT_BLUE);

  const char* nums = "123456789";
  int startX = 60;
  int startY = 160;
  int w = 55;
  int h = 24;
  int gap = 8;

  int idx = 0;
  for (int row = 0; row < 3; row++) {
    for (int col = 0; col < 3; col++) {
      char key[2] = {nums[idx], 0};
      drawBtn(startX + col * (w + gap), startY + row * (h + gap), w, h, key, 0x0010);
      idx++;
    }
  }

  drawBtn(startX + (w + gap), startY + 3 * (h + gap), w, h, "0", 0x0010);
}

void handleMainPage(int tx, int ty) {
  if (btnHit(5, 20, 60, 28, tx, ty)) {
    selectedPeer = 0; // Hauptwasseruhr
    inputText = "";
    currentPage = PAGE_OFFSET;
    return;
  }

  if (btnHit(70, 20, 60, 28, tx, ty)) {
    inputText = "";
    currentPage = PAGE_KEYBOARD;
    return;
  }

  if (btnHit(135, 20, 60, 28, tx, ty)) {
    currentPage = PAGE_FILES;
    return;
  }

  if (btnHit(200, 20, 60, 28, tx, ty)) {
    currentPage = PAGE_SDLOG;
    return;
  }

  if (btnHit(265, 20, 50, 28, tx, ty)) {
    File root = SD.open("/");
    if (root) {
      File entry;
      while ((entry = root.openNextFile())) {
        String name = "/" + String(entry.name());
        entry.close();
        if (name.endsWith(".txt")) SD.remove(name.c_str());
      }
      root.close();
    }
    drawMainPage();
    return;
  }
}

void handleOffsetPage(int tx, int ty) {
  if (btnHit(10, 120, 70, 26, tx, ty)) {
    if (inputText.length() > 0) {
      peers[selectedPeer].zaehler = (uint32_t)inputText.toInt();
      saveCounterState();
    }
    currentPage = PAGE_MAIN;
    return;
  }

  if (btnHit(90, 120, 90, 26, tx, ty)) {
    currentPage = PAGE_MAIN;
    return;
  }

  if (btnHit(190, 120, 55, 26, tx, ty)) {
    if (inputText.length() > 0) {
      inputText.remove(inputText.length() - 1);
      drawOffsetPage();
    }
    return;
  }

  if (btnHit(255, 120, 55, 26, tx, ty)) {
    inputText = "";
    drawOffsetPage();
    return;
  }

  int startX = 60;
  int startY = 160;
  int w = 55;
  int h = 24;
  int gap = 8;

  int digit = 1;
  for (int row = 0; row < 3; row++) {
    for (int col = 0; col < 3; col++) {
      if (btnHit(startX + col * (w + gap), startY + row * (h + gap), w, h, tx, ty)) {
        if (inputText.length() < 10) {
          inputText += String(digit);
          drawOffsetPage();
        }
        return;
      }
      digit++;
    }
  }

  if (btnHit(startX + (w + gap), startY + 3 * (h + gap), w, h, tx, ty)) {
    if (inputText.length() < 10) {
      inputText += "0";
      drawOffsetPage();
    }
  }
}

// ===== SD-LOG TFT =====
void drawSDLogPage() {
  tft.fillScreen(TFT_BLACK);
  tft.setTextDatum(TC_DATUM);
  tft.setTextFont(2);
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.drawString("Wasser-Log (SD)", 160, 5);
  drawBtn(260, 5, 55, 18, "ZURUECK", TFT_RED);
  tft.drawFastHLine(0, 26, 320, TFT_DARKGREY);

  if (!sdOK) {
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TFT_RED, TFT_BLACK);
    tft.drawString("Keine SD-Karte!", 160, 120);
    return;
  }

  File f = SD.open(WATER_LOG_FILE);
  if (!f) {
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TFT_RED, TFT_BLACK);
    tft.drawString("Datei nicht gefunden!", 160, 120);
    return;
  }

  String zeilen[10];
  int count = 0;
  while (f.available()) {
    String line = f.readStringUntil('\n');
    line.trim();
    if (line.length() > 0) {
      zeilen[count % 10] = line;
      count++;
    }
  }
  f.close();

  tft.setTextDatum(TL_DATUM);
  tft.setTextFont(1);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);

  int startIdx = (count > 10) ? count - 10 : 0;
  int displayed = (count > 10) ? 10 : count;
  for (int i = 0; i < displayed; i++) {
    tft.drawString(zeilen[(startIdx + i) % 10], 5, 30 + i * 18);
  }

  tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
  char infoBuf[30];
  snprintf(infoBuf, sizeof(infoBuf), "%d Eintraege", count > 0 ? count - 1 : 0);
  tft.drawString(infoBuf, 5, 220);
}

void handleSDLogPage(int tx, int ty) {
  if (btnHit(260, 5, 55, 18, tx, ty)) {
    currentPage = PAGE_MAIN;
  }
}

// ===== Keyboard =====
void drawKeyboardPage() {
  tft.fillScreen(TFT_BLACK);
  tft.setTextDatum(TL_DATUM);
  tft.setTextSize(1);
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.drawString("Neue Notiz eingeben:", 5, 5);

  tft.fillRect(5, 20, 310, 30, TFT_DARKGREY);
  tft.drawRect(5, 20, 310, 30, TFT_WHITE);
  tft.setTextColor(TFT_WHITE, TFT_DARKGREY);

  String showText = inputText;
  if (showText.length() > 45) showText = ".." + showText.substring(showText.length() - 43);
  tft.drawString(showText + "_", 10, 30);

  drawBtn(5, 58, 50, 30, "OK", TFT_GREEN);
  drawBtn(60, 58, 60, 30, "ZURUECK", TFT_RED);
  drawBtn(125, 58, 40, 30, "DEL", TFT_ORANGE);
  drawBtn(170, 58, 50, 30, "LEER", TFT_BLUE);
  drawBtn(225, 58, 40, 30, "ABC", shiftOn ? TFT_YELLOW : 0x4208);
  drawBtn(270, 58, 45, 30, "123", 0x4208);

  int y1 = KB_Y_START;
  int x1 = (320 - 10 * KB_STEP) / 2;
  for (int i = 0; i < 10; i++) {
    char key[2] = {kbRow1[i], 0};
    if (!shiftOn) key[0] += 32;
    drawBtn(x1 + i * KB_STEP, y1, KB_KEY_W, KB_KEY_H, key, 0x2104);
  }

  int y2 = y1 + KB_KEY_H + KB_GAP;
  int x2 = (320 - 9 * KB_STEP) / 2;
  for (int i = 0; i < 9; i++) {
    char key[2] = {kbRow2[i], 0};
    if (!shiftOn) key[0] += 32;
    drawBtn(x2 + i * KB_STEP, y2, KB_KEY_W, KB_KEY_H, key, 0x2104);
  }

  int y3 = y2 + KB_KEY_H + KB_GAP;
  int x3 = (320 - 7 * KB_STEP) / 2;
  for (int i = 0; i < 7; i++) {
    char key[2] = {kbRow3[i], 0};
    if (!shiftOn) key[0] += 32;
    drawBtn(x3 + i * KB_STEP, y3, KB_KEY_W, KB_KEY_H, key, 0x2104);
  }

  drawBtn(x3 + 7 * KB_STEP, y3, KB_KEY_W, KB_KEY_H, ".", 0x2104);
  drawBtn(x3 + 8 * KB_STEP, y3, KB_KEY_W, KB_KEY_H, ",", 0x2104);
  drawBtn(x3 + 9 * KB_STEP, y3, KB_KEY_W, KB_KEY_H, "!", 0x2104);
}

void drawNumberPage() {
  tft.fillScreen(TFT_BLACK);
  tft.setTextDatum(TL_DATUM);
  tft.setTextSize(1);
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.drawString("Neue Notiz eingeben:", 5, 5);

  tft.fillRect(5, 20, 310, 30, TFT_DARKGREY);
  tft.drawRect(5, 20, 310, 30, TFT_WHITE);
  tft.setTextColor(TFT_WHITE, TFT_DARKGREY);

  String showText = inputText;
  if (showText.length() > 45) showText = ".." + showText.substring(showText.length() - 43);
  tft.drawString(showText + "_", 10, 30);

  drawBtn(5, 58, 50, 30, "OK", TFT_GREEN);
  drawBtn(60, 58, 60, 30, "ZURUECK", TFT_RED);
  drawBtn(125, 58, 40, 30, "DEL", TFT_ORANGE);
  drawBtn(170, 58, 50, 30, "LEER", TFT_BLUE);
  drawBtn(225, 58, 40, 30, "ABC", 0x4208);
  drawBtn(270, 58, 45, 30, "123", TFT_YELLOW);

  const char* nums = "1234567890";
  int y1 = KB_Y_START;
  int x1 = (320 - 10 * KB_STEP) / 2;
  for (int i = 0; i < 10; i++) {
    char key[2] = {nums[i], 0};
    drawBtn(x1 + i * KB_STEP, y1, KB_KEY_W, KB_KEY_H, key, 0x0010);
  }

  const char* syms1 = "+-*/=@#&()";
  int y2 = y1 + KB_KEY_H + KB_GAP;
  for (int i = 0; i < 10; i++) {
    char key[2] = {syms1[i], 0};
    drawBtn(x1 + i * KB_STEP, y2, KB_KEY_W, KB_KEY_H, key, 0x0010);
  }

  const char* syms2 = ".,;:!?\"'-_";
  int y3 = y2 + KB_KEY_H + KB_GAP;
  for (int i = 0; i < 10; i++) {
    char key[2] = {syms2[i], 0};
    drawBtn(x1 + i * KB_STEP, y3, KB_KEY_W, KB_KEY_H, key, 0x0010);
  }
}

void handleKeyboardPage(int tx, int ty) {
  if (btnHit(5, 58, 50, 30, tx, ty)) {
    if (inputText.length() > 0) {
      int num = nextFileNumber();
      char fname[20];
      sprintf(fname, "/notiz%03d.txt", num);
      delay(100);
      File f = SD.open(fname, FILE_WRITE);
      if (f) {
        f.print(inputText);
        f.close();
      }
    }
    currentPage = PAGE_MAIN;
    return;
  }

  if (btnHit(60, 58, 60, 30, tx, ty)) {
    currentPage = PAGE_MAIN;
    return;
  }

  if (btnHit(125, 58, 40, 30, tx, ty)) {
    if (inputText.length() > 0) inputText.remove(inputText.length() - 1);
    if (numMode) drawNumberPage(); else drawKeyboardPage();
    return;
  }

  if (btnHit(170, 58, 50, 30, tx, ty)) {
    if (inputText.length() < INPUT_MAX_LEN) inputText += " ";
    if (numMode) drawNumberPage(); else drawKeyboardPage();
    return;
  }

  if (btnHit(225, 58, 40, 30, tx, ty)) {
    if (numMode) {
      numMode = false;
      drawKeyboardPage();
    } else {
      shiftOn = !shiftOn;
      drawKeyboardPage();
    }
    return;
  }

  if (btnHit(270, 58, 45, 30, tx, ty)) {
    numMode = !numMode;
    if (numMode) drawNumberPage(); else drawKeyboardPage();
    return;
  }

  if (numMode) {
    const char* nums = "1234567890";
    const char* syms1 = "+-*/=@#&()";
    const char* syms2 = ".,;:!?\"'-_";
    int x1 = (320 - 10 * KB_STEP) / 2;
    int y1 = KB_Y_START, y2 = y1 + KB_KEY_H + KB_GAP, y3 = y2 + KB_KEY_H + KB_GAP;

    for (int i = 0; i < 10; i++) {
      if (btnHit(x1 + i * KB_STEP, y1, KB_KEY_W, KB_KEY_H, tx, ty)) {
        if (inputText.length() < INPUT_MAX_LEN) inputText += nums[i];
        drawNumberPage();
        return;
      }
      if (btnHit(x1 + i * KB_STEP, y2, KB_KEY_W, KB_KEY_H, tx, ty)) {
        if (inputText.length() < INPUT_MAX_LEN) inputText += syms1[i];
        drawNumberPage();
        return;
      }
      if (btnHit(x1 + i * KB_STEP, y3, KB_KEY_W, KB_KEY_H, tx, ty)) {
        if (inputText.length() < INPUT_MAX_LEN) inputText += syms2[i];
        drawNumberPage();
        return;
      }
    }
  } else {
    int y1 = KB_Y_START;
    int x1 = (320 - 10 * KB_STEP) / 2;
    for (int i = 0; i < 10; i++) {
      if (btnHit(x1 + i * KB_STEP, y1, KB_KEY_W, KB_KEY_H, tx, ty)) {
        char c = kbRow1[i];
        if (!shiftOn) c += 32;
        if (inputText.length() < INPUT_MAX_LEN) inputText += c;
        drawKeyboardPage();
        return;
      }
    }

    int y2 = y1 + KB_KEY_H + KB_GAP;
    int x2 = (320 - 9 * KB_STEP) / 2;
    for (int i = 0; i < 9; i++) {
      if (btnHit(x2 + i * KB_STEP, y2, KB_KEY_W, KB_KEY_H, tx, ty)) {
        char c = kbRow2[i];
        if (!shiftOn) c += 32;
        if (inputText.length() < INPUT_MAX_LEN) inputText += c;
        drawKeyboardPage();
        return;
      }
    }

    int y3 = y2 + KB_KEY_H + KB_GAP;
    int x3 = (320 - 7 * KB_STEP) / 2;
    for (int i = 0; i < 7; i++) {
      if (btnHit(x3 + i * KB_STEP, y3, KB_KEY_W, KB_KEY_H, tx, ty)) {
        char c = kbRow3[i];
        if (!shiftOn) c += 32;
        if (inputText.length() < INPUT_MAX_LEN) inputText += c;
        drawKeyboardPage();
        return;
      }
    }

    const char extras[] = {'.', ',', '!'};
    for (int i = 0; i < 3; i++) {
      if (btnHit(x3 + (7 + i) * KB_STEP, y3, KB_KEY_W, KB_KEY_H, tx, ty)) {
        if (inputText.length() < INPUT_MAX_LEN) inputText += extras[i];
        drawKeyboardPage();
        return;
      }
    }
  }
}

// ===== Dateiliste =====
void loadFileList() {
  fileCount = 0;
  File root = SD.open("/");
  if (!root) return;

  File entry;
  while ((entry = root.openNextFile()) && fileCount < MAX_FILES) {
    String name = String(entry.name());
    if (name.endsWith(".txt")) {
      fileNames[fileCount] = "/" + name;
      fileCount++;
    }
    entry.close();
  }
  root.close();
}

void drawFilesPage() {
  tft.fillScreen(TFT_BLACK);
  tft.setTextDatum(TC_DATUM);
  tft.setTextSize(2);
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.drawString("SD-Dateien", 160, 5);
  drawBtn(250, 5, 65, 22, "ZURUECK", TFT_RED);

  if (fileCount == 0) {
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString("Keine Dateien", 160, 120);
    return;
  }

  int perPage = 6, startY = 35;
  for (int i = 0; i < perPage && (i + fileScrollOffset) < fileCount; i++) {
    drawBtn(10, startY + i * 32, 300, 28, fileNames[i + fileScrollOffset].c_str(), 0x000A);
  }

  if (fileScrollOffset > 0) drawBtn(10, 225, 60, 15, "HOCH", 0x4208);
  if (fileScrollOffset + perPage < fileCount) drawBtn(250, 225, 60, 15, "RUNTER", 0x4208);
}

void handleFilesPage(int tx, int ty) {
  if (btnHit(250, 5, 65, 22, tx, ty)) {
    currentPage = PAGE_MAIN;
    return;
  }

  if (btnHit(10, 225, 60, 15, tx, ty) && fileScrollOffset > 0) {
    fileScrollOffset--;
    drawFilesPage();
    return;
  }

  if (btnHit(250, 225, 60, 15, tx, ty)) {
    fileScrollOffset++;
    drawFilesPage();
    return;
  }

  int perPage = 6, startY = 35;
  for (int i = 0; i < perPage && (i + fileScrollOffset) < fileCount; i++) {
    if (btnHit(10, startY + i * 32, 300, 28, tx, ty)) {
      int idx = i + fileScrollOffset;
      delay(100);
      File f = SD.open(fileNames[idx].c_str(), FILE_READ);
      if (f) {
        viewTitle = fileNames[idx];
        viewText = "";
        while (f.available()) viewText += (char)f.read();
        f.close();
        currentPage = PAGE_VIEW;
        return;
      }
    }
  }
}

// ===== Datei anzeigen =====
void drawViewPage() {
  tft.fillScreen(TFT_BLACK);
  tft.setTextDatum(TL_DATUM);
  tft.setTextSize(1);
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.drawString(viewTitle, 5, 5);
  drawBtn(200, 2, 55, 18, "ZURUECK", TFT_RED);
  drawBtn(260, 2, 55, 18, "LOESCHEN", TFT_MAROON);
  tft.drawLine(0, 22, 319, 22, TFT_DARKGREY);
  drawWrappedText(viewText, 5, 28, 310, TFT_WHITE);
}

void handleViewPage(int tx, int ty) {
  if (btnHit(200, 2, 55, 18, tx, ty)) {
    currentPage = PAGE_FILES;
    return;
  }

  if (btnHit(260, 2, 55, 18, tx, ty)) {
    SD.remove(viewTitle.c_str());
    currentPage = PAGE_MAIN;
    return;
  }
}