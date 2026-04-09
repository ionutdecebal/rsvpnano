#pragma once

#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <SPI.h>
#include <Wire.h>
#include "Types.h"

class BoardSupport {
 public:
  bool begin();
  void update();

  bool bootPressed() const;
  TouchEvent readTouch();

  Adafruit_GFX* gfx() { return tft_; }
  uint32_t nowMs() const { return millis(); }

  void setBacklight(uint8_t level);
  void powerDownDisplay();

 private:
  bool initDisplay();
  bool initTouch();

  SPIClass spi_{FSPI};
  Adafruit_ST7789* tft_ = nullptr;

  bool touchReady_ = false;
  TouchEvent lastTouch_;
  bool pwmReady_ = false;
  int pwmChannel_ = 1;
};
