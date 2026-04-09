#pragma once

#include <Arduino.h>
#include "Types.h"
#include "BoardConfig.h"

class TouchHandler {
 public:
  void reset();
  GestureResult update(const TouchEvent& e, bool touchEnabled, uint32_t nowMs);

 private:
  bool tracking_ = false;
  bool longPressFired_ = false;
  int16_t startX_ = 0;
  int16_t startY_ = 0;
  int16_t lastX_ = 0;
  int16_t lastY_ = 0;
  uint32_t startMs_ = 0;
  uint32_t lastMs_ = 0;
};
