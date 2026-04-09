#include "input/TouchHandler.h"

#include <Wire.h>

bool TouchHandler::begin() {
  Wire.beginTransmission(kAddress);
  const uint8_t error = Wire.endTransmission();
  initialized_ = (error == 0);

  if (!initialized_) {
    Serial.println("[touch] Controller not detected at 0x15");
  } else {
    Serial.println("[touch] Initialized (CST816S-compatible)");
  }

  return initialized_;
}

bool TouchHandler::readRegister(uint8_t reg, uint8_t *buffer, size_t len) {
  Wire.beginTransmission(kAddress);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) {
    return false;
  }

  const size_t readLen = Wire.requestFrom(static_cast<int>(kAddress), static_cast<int>(len));
  if (readLen != len) {
    return false;
  }

  for (size_t i = 0; i < len; ++i) {
    buffer[i] = Wire.read();
  }
  return true;
}

bool TouchHandler::poll(TouchEvent &event) {
  event = TouchEvent{};

  if (!initialized_) {
    return false;
  }

  const uint32_t now = millis();
  if (now - lastPollMs_ < 20) {
    return false;
  }
  lastPollMs_ = now;

  uint8_t data[6] = {0};
  if (!readRegister(0x01, data, sizeof(data))) {
    return false;
  }

  // CST816S packet: [gesture, points, xh, xl, yh, yl]
  const uint8_t points = data[1] & 0x0F;
  if (points == 0) {
    return false;
  }

  event.touched = true;
  event.gesture = data[0];
  event.x = static_cast<uint16_t>(((data[2] & 0x0F) << 8) | data[3]);
  event.y = static_cast<uint16_t>(((data[4] & 0x0F) << 8) | data[5]);

  return true;
}
