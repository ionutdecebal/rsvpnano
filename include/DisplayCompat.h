#pragma once

#include <Arduino.h>

#if __has_include(<Adafruit_GFX.h>) && __has_include(<Adafruit_ST7789.h>)
#define RSVP_HAS_ADAFRUIT_DISPLAY 1
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <Adafruit_ST77xx.h>
using DisplaySurface = Adafruit_GFX;
using DisplayPanel = Adafruit_ST7789;

inline uint16_t rsvpColorBlack() { return ST77XX_BLACK; }
inline uint16_t rsvpColorWhite() { return ST77XX_WHITE; }
inline uint16_t rsvpColor565(uint8_t r, uint8_t g, uint8_t b) {
  return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

#else
#define RSVP_HAS_ADAFRUIT_DISPLAY 0

class DisplaySurface {
 public:
  virtual ~DisplaySurface() = default;
  virtual void fillScreen(uint16_t) {}
  virtual void setTextColor(uint16_t) {}
  virtual void setTextSize(uint8_t) {}
  virtual void setCursor(int16_t, int16_t) {}
  virtual void println(const String&) {}
  virtual void print(const String&) {}
  virtual void print(const char*) {}
  virtual void printf(const char*, ...) {}
  virtual void fillRoundRect(int16_t, int16_t, int16_t, int16_t, int16_t, uint16_t) {}
  virtual void drawRoundRect(int16_t, int16_t, int16_t, int16_t, int16_t, uint16_t) {}
  virtual void getTextBounds(const String& s, int16_t x, int16_t y, int16_t* x1, int16_t* y1, uint16_t* w, uint16_t* h) {
    *x1 = x;
    *y1 = y;
    *w = s.length() * 6;
    *h = 8;
  }
};

class DisplayPanel : public DisplaySurface {
 public:
  DisplayPanel(int8_t, int8_t, int8_t) {}
  DisplayPanel(void*, int8_t, int8_t, int8_t) {}
  void init(uint16_t, uint16_t) {}
  void setRotation(uint8_t) {}
};

inline uint16_t rsvpColorBlack() { return 0x0000; }
inline uint16_t rsvpColorWhite() { return 0xFFFF; }
inline uint16_t rsvpColor565(uint8_t r, uint8_t g, uint8_t b) {
  return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

#endif
