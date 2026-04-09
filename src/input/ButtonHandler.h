#pragma once

#include <Arduino.h>

class ButtonHandler {
 public:
  explicit ButtonHandler(int pin);

  void begin();
  void update(uint32_t nowMs);

  bool isHeld() const;
  bool wasPressedEvent() const;
  uint32_t lastEdgeMs() const;

 private:
  int pin_;
  bool held_ = false;
  bool pressedEvent_ = false;
  uint32_t lastEdgeMs_ = 0;
};
