#pragma once

#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <Wire.h>
#include "Types.h"

class BoardSupport {
 public:
  bool begin();
  void update();

  bool bootPressed() const;
  TouchEvent readTouch();

  Arduino_GFX* gfx() const { return gfx_; }
  uint32_t nowMs() const { return millis(); }

  void setBacklight(uint8_t level);
  void powerDownDisplay();

 private:
  bool initDisplay();
  bool initTouch();

  Arduino_DataBus* bus_ = nullptr;
  Arduino_GFX* gfx_ = nullptr;
  bool touchReady_ = false;
  TouchEvent lastTouch_;
};
