#pragma once
// ============================================================================
// Display configuration — edit to match your board, then reflash.
// Defaults are for the LilyGO T-Display-S3 (ESP32-S3, ST7789, 8-bit parallel).
//
// Supported buses:   BUS_PARALLEL8, BUS_SPI
// Supported drivers: DRIVER_ST7789, DRIVER_ILI9341
// To add another Arduino_GFX driver, add an #elif block in main.ino where the
// bus/driver objects are constructed.
// ============================================================================

// ---- Bus: pick exactly one ----
#define BUS_PARALLEL8
// #define BUS_SPI

// ---- Driver: pick exactly one ----
#define DRIVER_ST7789
// #define DRIVER_ILI9341

// ---- Panel geometry (native/portrait dimensions) ----
#define TFT_WIDTH      170     // ILI9341 is fixed 240x320; set these to match anyway
#define TFT_HEIGHT     320
#define TFT_ROTATION   3       // 0..3  (1 or 3 = landscape)
#define TFT_IPS        true
#define TFT_COL_OFFSET 35      // panel pixel offset (usually 0; 35 on T-Display-S3)
#define TFT_ROW_OFFSET 0

// ---- Common control pins (use -1 if the board doesn't have one) ----
#define PIN_RST 5
#define PIN_BL  38             // backlight enable
#define PIN_PWR 15             // panel power enable, driven HIGH at boot

// ---- Bus pins ----
#ifdef BUS_PARALLEL8
  #define PIN_DC 7
  #define PIN_CS 6
  #define PIN_WR 8
  #define PIN_RD 9
  #define PAR_D0 39
  #define PAR_D1 40
  #define PAR_D2 41
  #define PAR_D3 42
  #define PAR_D4 45
  #define PAR_D5 46
  #define PAR_D6 47
  #define PAR_D7 48
#endif
#ifdef BUS_SPI
  #define PIN_DC   7
  #define PIN_CS   10
  #define PIN_SCK  12
  #define PIN_MOSI 11
  #define PIN_MISO -1          // -1 if unused
#endif
