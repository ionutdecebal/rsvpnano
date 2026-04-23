#pragma once

#include <Arduino.h>

class ButtonHandler {
 public:
  explicit ButtonHandler(int pin);

  void begin();
  void update(uint32_t nowMs);

  bool isHeld() const;
  bool wasPressedEvent() const;
  bool wasReleasedEvent() const;
  uint32_t lastEdgeMs() const;
  uint32_t heldDurationMs(uint32_t nowMs) const;
  uint32_t lastHoldDurationMs() const;

 private:
  int pin_;
  bool held_ = false;
  bool pressedEvent_ = false;
  bool releasedEvent_ = false;
  uint32_t lastEdgeMs_ = 0;
  uint32_t pressStartedMs_ = 0;
  uint32_t lastHoldDurationMs_ = 0;
};
