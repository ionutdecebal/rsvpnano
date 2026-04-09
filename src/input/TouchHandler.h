#pragma once

#include <Arduino.h>

struct TouchEvent {
  bool touched = false;
  uint16_t x = 0;
  uint16_t y = 0;
  uint8_t gesture = 0;
};

class TouchHandler {
 public:
  bool begin();
  bool poll(TouchEvent &event);

 private:
  static constexpr uint8_t kAddress = 0x15;  // Common CST816S address.
  bool initialized_ = false;
  uint32_t lastPollMs_ = 0;

  bool readRegister(uint8_t reg, uint8_t *buffer, size_t len);
};
