// User_Setup.h fuer ESP32-2432S028 (CYD - Cheap Yellow Display)
//
// Diese Datei in den TFT_eSPI Library-Ordner kopieren:
// C:\Users\Thomas\Documents\Arduino\libraries\TFT_eSPI\User_Setup.h
// (vorhandene User_Setup.h vorher umbenennen/sichern!)

#define USER_SETUP_LOADED 1

// Display Treiber
#define ILI9341_DRIVER

// Display Groesse
#define TFT_WIDTH  240
#define TFT_HEIGHT 320

// SPI Pins Display
#define TFT_MISO 12
#define TFT_MOSI 13
#define TFT_SCLK 14
#define TFT_CS   15
#define TFT_DC    2
#define TFT_RST   4

// Fonts
#define LOAD_GLCD
#define LOAD_FONT2
#define LOAD_FONT4
#define LOAD_GFXFF

// SPI Geschwindigkeit
#define SPI_FREQUENCY       27000000
#define SPI_READ_FREQUENCY  20000000

// Touch NICHT ueber TFT_eSPI (wird separat via XPT2046_Touchscreen Library gesteuert)
#define TOUCH_CS -1
