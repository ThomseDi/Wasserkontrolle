/*
 * SD Notiz-Manager fuer ESP32-2432S028 (CYD - Cheap Yellow Display)
 * 
 * VERIFIZIERT: Display + Touch + SD funktionieren!
 * 
 * WICHTIG fuer Arduino IDE:
 * 1. Board: "ESP32 Dev Module" auswaehlen
 * 2. Libraries installieren: TFT_eSPI, XPT2046_Touchscreen
 * 3. Die Datei User_Setup.h aus diesem Ordner in den TFT_eSPI Library-Ordner kopieren:
 *    C:\Users\<USER>\Documents\Arduino\libraries\TFT_eSPI\User_Setup.h
 *    (vorhandene User_Setup.h vorher sichern!)
 * 4. Upload Speed: 115200
 * 5. Port: COM12
 */

#include <Arduino.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <SD.h>

#define TFT_BL     21
#define SD_CS      5
#define SD_MOSI    23
#define SD_MISO    19
#define SD_SCK     18

// Touch Pins (VSPI)
#define TOUCH_CS   33
#define TOUCH_IRQ  36
#define TOUCH_MOSI 32
#define TOUCH_MISO 39
#define TOUCH_CLK  25

// SPI-Trick: XPT2046 auf VSPI umleiten
SPIClass touchSPI(VSPI);
#define SPI touchSPI
#include <XPT2046_Touchscreen.h>
#undef SPI

TFT_eSPI tft = TFT_eSPI();
SPIClass sdSPI(HSPI);
XPT2046_Touchscreen ts(TOUCH_CS, 255);

bool sdOK = false;
bool lastTouched = false;

// Kalibrierung
int16_t calX0 = 300, calY0 = 300;
int16_t calX1 = 3800, calY1 = 3800;

// ===== Seiten =====
enum Page { PAGE_MAIN, PAGE_KEYBOARD, PAGE_FILES, PAGE_VIEW };
Page currentPage = PAGE_MAIN;

// ===== Texteingabe =====
String inputText = "";
const int INPUT_MAX_LEN = 80;

// ===== Dateianzeige =====
String viewText = "";
String viewTitle = "";

// ===== Dateiliste =====
#define MAX_FILES 10
String fileNames[MAX_FILES];
int fileCount = 0;
int fileScrollOffset = 0;

// ===== Keyboard =====
bool shiftOn = false;
const char* kbRow1 = "QWERTZUIOP";
const char* kbRow2 = "ASDFGHJKL";
const char* kbRow3 = "YXCVBNM";

#define KB_KEY_W  28
#define KB_KEY_H  32
#define KB_GAP    2
#define KB_STEP   (KB_KEY_W + KB_GAP)
#define KB_Y_START 100

// ===== Statustext =====
String statusMsg = "";
bool numMode = false;

// ===== Touch lesen =====
bool getTouch(int &tx, int &ty) {
  TS_Point p = ts.getPoint();
  bool touched = (p.z >= 400);
  
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

// ===== Button zeichnen =====
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
  tft.setTextSize(2);
  tft.setTextColor(TFT_WHITE, color);
  tft.drawString(label, x + w/2, y + h/2);
}

bool btnHit(int bx, int by, int bw, int bh, int tx, int ty) {
  return (tx >= bx && tx <= bx + bw && ty >= by && ty <= by + bh);
}

// ===== Text mehrzeilig anzeigen =====
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

// ===== Naechste freie Dateinummer finden =====
int nextFileNumber() {
  for (int i = 1; i <= 999; i++) {
    char fname[20];
    sprintf(fname, "/notiz%03d.txt", i);
    if (!SD.exists(fname)) return i;
  }
  return 999;
}

// ===== HAUPTMENUE =====
void drawMainPage() {
  tft.fillScreen(TFT_BLACK);
  
  tft.setTextDatum(TC_DATUM);
  tft.setTextSize(2);
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.drawString("SD Notiz-Manager", 160, 8);
  
  drawBtnBig(20,  50, 130, 50, "NEUE NOTIZ", 0x0400);
  drawBtnBig(170, 50, 130, 50, "DATEIEN",    0x000A);
  drawBtnBig(20, 120, 130, 50, "LETZTE",     0x4808);
  drawBtnBig(170, 120, 130, 50, "ALLE DEL",  TFT_MAROON);
  
  if (statusMsg.length() > 0) {
    drawWrappedText(statusMsg, 10, 185, 300, TFT_CYAN);
  }
}

void handleMainPage(int tx, int ty) {
  if (btnHit(20, 50, 130, 50, tx, ty)) {
    inputText = "";
    currentPage = PAGE_KEYBOARD;
    return;
  }
  if (btnHit(170, 50, 130, 50, tx, ty)) {
    currentPage = PAGE_FILES;
    return;
  }
  if (btnHit(20, 120, 130, 50, tx, ty)) {
    int num = nextFileNumber() - 1;
    if (num >= 1) {
      char fname[20];
      sprintf(fname, "/notiz%03d.txt", num);
      delay(100);
      File f = SD.open(fname, FILE_READ);
      if (f) {
        viewTitle = String(fname);
        viewText = "";
        while (f.available()) viewText += (char)f.read();
        f.close();
        currentPage = PAGE_VIEW;
        return;
      }
    }
    statusMsg = "Keine Notizen vorhanden!";
    drawMainPage();
    return;
  }
  if (btnHit(170, 120, 130, 50, tx, ty)) {
    File root = SD.open("/");
    if (root) {
      File entry;
      while ((entry = root.openNextFile())) {
        String name = "/" + String(entry.name());
        entry.close();
        if (name.endsWith(".txt")) {
          SD.remove(name.c_str());
        }
      }
      root.close();
    }
    statusMsg = "Alle Dateien geloescht!";
    drawMainPage();
    return;
  }
}

// ===== KEYBOARD =====
void drawKeyboardPage() {
  tft.fillScreen(TFT_BLACK);
  
  tft.setTextDatum(TL_DATUM);
  tft.setTextSize(1);
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.drawString("Neue Notiz eingeben:", 5, 5);
  
  tft.fillRect(5, 20, 310, 30, TFT_DARKGREY);
  tft.drawRect(5, 20, 310, 30, TFT_WHITE);
  tft.setTextDatum(TL_DATUM);
  tft.setTextSize(1);
  tft.setTextColor(TFT_WHITE, TFT_DARKGREY);
  
  String showText = inputText;
  if (showText.length() > 45) {
    showText = ".." + showText.substring(showText.length() - 43);
  }
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
    if (!shiftOn) key[0] = key[0] + 32;
    drawBtn(x1 + i * KB_STEP, y1, KB_KEY_W, KB_KEY_H, key, 0x2104);
  }
  
  int y2 = y1 + KB_KEY_H + KB_GAP;
  int x2 = (320 - 9 * KB_STEP) / 2;
  for (int i = 0; i < 9; i++) {
    char key[2] = {kbRow2[i], 0};
    if (!shiftOn) key[0] = key[0] + 32;
    drawBtn(x2 + i * KB_STEP, y2, KB_KEY_W, KB_KEY_H, key, 0x2104);
  }
  
  int y3 = y2 + KB_KEY_H + KB_GAP;
  int x3 = (320 - 7 * KB_STEP) / 2;
  for (int i = 0; i < 7; i++) {
    char key[2] = {kbRow3[i], 0};
    if (!shiftOn) key[0] = key[0] + 32;
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
  tft.setTextDatum(TL_DATUM);
  tft.setTextSize(1);
  tft.setTextColor(TFT_WHITE, TFT_DARKGREY);
  String showText = inputText;
  if (showText.length() > 45) {
    showText = ".." + showText.substring(showText.length() - 43);
  }
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
        statusMsg = "Gespeichert: " + String(fname);
      } else {
        statusMsg = "Fehler beim Speichern!";
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
    if (inputText.length() > 0) {
      inputText.remove(inputText.length() - 1);
    }
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
    int y1 = KB_Y_START;
    int y2 = y1 + KB_KEY_H + KB_GAP;
    int y3 = y2 + KB_KEY_H + KB_GAP;
    
    for (int i = 0; i < 10; i++) {
      if (btnHit(x1 + i * KB_STEP, y1, KB_KEY_W, KB_KEY_H, tx, ty)) {
        if (inputText.length() < INPUT_MAX_LEN) inputText += nums[i];
        drawNumberPage(); return;
      }
      if (btnHit(x1 + i * KB_STEP, y2, KB_KEY_W, KB_KEY_H, tx, ty)) {
        if (inputText.length() < INPUT_MAX_LEN) inputText += syms1[i];
        drawNumberPage(); return;
      }
      if (btnHit(x1 + i * KB_STEP, y3, KB_KEY_W, KB_KEY_H, tx, ty)) {
        if (inputText.length() < INPUT_MAX_LEN) inputText += syms2[i];
        drawNumberPage(); return;
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
        drawKeyboardPage(); return;
      }
    }
    
    int y2 = y1 + KB_KEY_H + KB_GAP;
    int x2 = (320 - 9 * KB_STEP) / 2;
    for (int i = 0; i < 9; i++) {
      if (btnHit(x2 + i * KB_STEP, y2, KB_KEY_W, KB_KEY_H, tx, ty)) {
        char c = kbRow2[i];
        if (!shiftOn) c += 32;
        if (inputText.length() < INPUT_MAX_LEN) inputText += c;
        drawKeyboardPage(); return;
      }
    }
    
    int y3 = y2 + KB_KEY_H + KB_GAP;
    int x3 = (320 - 7 * KB_STEP) / 2;
    for (int i = 0; i < 7; i++) {
      if (btnHit(x3 + i * KB_STEP, y3, KB_KEY_W, KB_KEY_H, tx, ty)) {
        char c = kbRow3[i];
        if (!shiftOn) c += 32;
        if (inputText.length() < INPUT_MAX_LEN) inputText += c;
        drawKeyboardPage(); return;
      }
    }
    const char extras[] = {'.', ',', '!'};
    for (int i = 0; i < 3; i++) {
      if (btnHit(x3 + (7+i) * KB_STEP, y3, KB_KEY_W, KB_KEY_H, tx, ty)) {
        if (inputText.length() < INPUT_MAX_LEN) inputText += extras[i];
        drawKeyboardPage(); return;
      }
    }
  }
}

// ===== DATEILISTE =====
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
    tft.setTextSize(2);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString("Keine Dateien", 160, 120);
    return;
  }
  
  int perPage = 6;
  int startY = 35;
  
  for (int i = 0; i < perPage && (i + fileScrollOffset) < fileCount; i++) {
    int idx = i + fileScrollOffset;
    drawBtn(10, startY + i * 32, 300, 28, fileNames[idx].c_str(), 0x000A);
  }
  
  if (fileScrollOffset > 0) {
    drawBtn(10, 225, 60, 15, "HOCH", 0x4208);
  }
  if (fileScrollOffset + perPage < fileCount) {
    drawBtn(250, 225, 60, 15, "RUNTER", 0x4208);
  }
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
  
  int perPage = 6;
  int startY = 35;
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

// ===== DATEI ANZEIGEN =====
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
    statusMsg = viewTitle + " geloescht!";
    currentPage = PAGE_MAIN;
    return;
  }
}

// ===== SETUP =====
void setup() {
  delay(2000);
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n=== SD Notiz-Manager ===\n");
  
  // Display
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);
  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);
  Serial.println("Display OK");
  
  // Touch
  SPI.begin(TOUCH_CLK, TOUCH_MISO, TOUCH_MOSI, TOUCH_CS);
  ts.begin();
  ts.setRotation(1);
  Serial.println("Touch OK");
  
  // SD
  sdSPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
  if (SD.begin(SD_CS, sdSPI)) {
    sdOK = true;
    Serial.println("SD-Karte OK");
  } else {
    Serial.println("SD-Karte Fehler!");
    tft.setTextSize(2);
    tft.setTextColor(TFT_RED, TFT_BLACK);
    tft.drawString("SD-Fehler!", 160, 100);
    while(1) delay(1000);
  }
  
  statusMsg = "Bereit!";
  drawMainPage();
  Serial.println("Setup fertig!\n");
}

// ===== LOOP =====
void loop() {
  int tx, ty;
  
  if (getTouch(tx, ty)) {
    switch (currentPage) {
      case PAGE_MAIN:
        handleMainPage(tx, ty);
        break;
      case PAGE_KEYBOARD:
        handleKeyboardPage(tx, ty);
        break;
      case PAGE_FILES:
        handleFilesPage(tx, ty);
        break;
      case PAGE_VIEW:
        handleViewPage(tx, ty);
        break;
    }
    
    static Page lastPage = PAGE_MAIN;
    if (currentPage != lastPage) {
      switch (currentPage) {
        case PAGE_MAIN:     drawMainPage(); break;
        case PAGE_KEYBOARD: numMode = false; drawKeyboardPage(); break;
        case PAGE_FILES:    fileScrollOffset = 0; loadFileList(); drawFilesPage(); break;
        case PAGE_VIEW:     drawViewPage(); break;
      }
      lastPage = currentPage;
    }
  }
  
  delay(50);
}
