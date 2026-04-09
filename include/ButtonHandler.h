#pragma once

#include <Arduino.h>
#include "BoardConfig.h"

struct ButtonEvents {
  bool holdActive = false;
  bool pauseByRelease = false;
  bool triplePress = false;
};

class ButtonHandler {
 public:
  void begin();
  ButtonEvents update(bool rawPressed, bool pausedState, uint32_t nowMs);

 private:
  bool stablePressed_ = false;
  bool lastRaw_ = false;
  uint32_t lastBounceMs_ = 0;

  uint32_t releaseBufferedUntilMs_ = 0;

  uint8_t clickCount_ = 0;
  uint32_t firstClickMs_ = 0;
  bool wasHeld_ = false;
};
