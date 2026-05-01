#pragma once
#include "app_state.h"

// Touch / UI
bool getTouch(int &tx, int &ty);
bool btnHit(int bx, int by, int bw, int bh, int tx, int ty);
void drawBtn(int x, int y, int w, int h, const char* label, uint16_t color);
void drawBtnBig(int x, int y, int w, int h, const char* label, uint16_t color);
void drawWrappedText(String text, int x, int y, int maxW, uint16_t color);

// Seiten
void drawMainPage();
void updatePeerStatus();
void drawKeyboardPage();
void drawNumberPage();
void drawFilesPage();
void drawViewPage();
void drawSDLogPage();
void drawOffsetPage();
void drawEntkalkerPage();
void loadFileList();

// Handler
void handleMainPage(int tx, int ty);
void handleKeyboardPage(int tx, int ty);
void handleFilesPage(int tx, int ty);
void handleViewPage(int tx, int ty);
void handleSDLogPage(int tx, int ty);
void handleOffsetPage(int tx, int ty);
void handleEntkalkerPage(int tx, int ty);