#pragma once

#include <Arduino.h>

namespace Input::Buttons {

class Button {
 public:
  explicit Button(int pin);

  void begin();
  void beginWithState(bool held);
  void update(uint32_t nowMs);
  void updateFromState(bool currentHeld, uint32_t nowMs);

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

  void resetState(bool held, uint32_t nowMs);
};

}  // namespace Input::Buttons
