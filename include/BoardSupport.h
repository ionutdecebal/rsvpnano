#pragma once

#include <Arduino.h>
<<<<<<< codex/build-mvp-firmware-for-waveshare-esp32-s3-7afq49
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <SPI.h>
=======
#include <Arduino_GFX_Library.h>
>>>>>>> main
#include <Wire.h>
#include "Types.h"

class BoardSupport {
 public:
  bool begin();
  void update();

  bool bootPressed() const;
  TouchEvent readTouch();

<<<<<<< codex/build-mvp-firmware-for-waveshare-esp32-s3-7afq49
  Adafruit_GFX* gfx() { return tft_; }
=======
  Arduino_GFX* gfx() const { return gfx_; }
>>>>>>> main
  uint32_t nowMs() const { return millis(); }

  void setBacklight(uint8_t level);
  void powerDownDisplay();

 private:
  bool initDisplay();
  bool initTouch();

<<<<<<< codex/build-mvp-firmware-for-waveshare-esp32-s3-7afq49
  SPIClass spi_{FSPI};
  Adafruit_ST7789* tft_ = nullptr;

  bool touchReady_ = false;
  TouchEvent lastTouch_;
  bool pwmReady_ = false;
  int pwmChannel_ = 1;
=======
  Arduino_DataBus* bus_ = nullptr;
  Arduino_GFX* gfx_ = nullptr;
  bool touchReady_ = false;
  TouchEvent lastTouch_;
>>>>>>> main
};
