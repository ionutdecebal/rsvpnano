#include "input/TouchHandler.h"

#include <algorithm>
#include <Wire.h>
#include <sdkconfig.h>

#include "board/BoardConfig.h"

namespace {

constexpr uint32_t kPollIntervalMs = 20;
constexpr uint32_t kFailureBackoffMs = 250;
constexpr uint8_t kReleaseConfirmSamples = 2;

#if !CONFIG_IDF_TARGET_ESP32C6
constexpr uint8_t kReadTouchCommand[] = {
    0xB5, 0xAB, 0xA5, 0x5A, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00,
};
#endif

uint16_t clampDisplayX(uint16_t x) {
  return std::min<uint16_t>(x, static_cast<uint16_t>(BoardConfig::DISPLAY_WIDTH - 1));
}

uint16_t clampDisplayY(uint16_t y) {
  return std::min<uint16_t>(y, static_cast<uint16_t>(BoardConfig::DISPLAY_HEIGHT - 1));
}

}  // namespace

bool TouchHandler::begin() {
  lastPollMs_ = 0;
  backoffUntilMs_ = 0;
  lastTouchSampleMs_ = 0;
  consecutiveReadFailures_ = 0;
  emptyTouchSamples_ = 0;
  touchActive_ = false;
  lastX_ = 0;
  lastY_ = 0;

#if CONFIG_IDF_TARGET_ESP32C6
  if (BoardConfig::PIN_TOUCH_RST >= 0) {
    pinMode(BoardConfig::PIN_TOUCH_RST, OUTPUT);
    digitalWrite(BoardConfig::PIN_TOUCH_RST, LOW);
    delay(10);
    digitalWrite(BoardConfig::PIN_TOUCH_RST, HIGH);
    delay(50);
  }
  if (BoardConfig::PIN_TOUCH_INT >= 0) {
    pinMode(BoardConfig::PIN_TOUCH_INT, INPUT_PULLUP);
  }
#endif

  Wire.beginTransmission(kAddress);
  const uint8_t error = Wire.endTransmission();
  initialized_ = (error == 0);

  if (!initialized_) {
    Serial.printf("[touch] Controller not detected at 0x%02X\n", kAddress);
  } else {
#if CONFIG_IDF_TARGET_ESP32C6
    Serial.println("[touch] Initialized (AXS5106L)");
#else
    Serial.println("[touch] Initialized (AXS15231B)");
#endif
  }

  return initialized_;
}

void TouchHandler::end() {
  cancel();
  initialized_ = false;
  Wire.end();
}

void TouchHandler::cancel() {
  touchActive_ = false;
  lastPollMs_ = 0;
  backoffUntilMs_ = 0;
  lastTouchSampleMs_ = 0;
  consecutiveReadFailures_ = 0;
  emptyTouchSamples_ = 0;
}

bool TouchHandler::readTouchPacket(uint8_t *buffer, size_t len) {
#if CONFIG_IDF_TARGET_ESP32C6
  // AXS5106L: read N bytes starting at register 0x01. The controller does not
  // tolerate repeated-start, so issue a STOP after writing the register and let
  // requestFrom() begin a fresh transaction.
  Wire.beginTransmission(kAddress);
  Wire.write(static_cast<uint8_t>(0x01));
  if (Wire.endTransmission(true) != 0) {
    return false;
  }

  const size_t readLen =
      Wire.requestFrom(static_cast<uint8_t>(kAddress), static_cast<size_t>(len), true);
  if (readLen != len) {
    return false;
  }

  for (size_t i = 0; i < len; ++i) {
    buffer[i] = Wire.read();
  }
  return true;
#else
  Wire.beginTransmission(kAddress);
  Wire.write(kReadTouchCommand, sizeof(kReadTouchCommand));
  if (Wire.endTransmission(false) != 0) {
    return false;
  }

  const size_t readLen =
      Wire.requestFrom(static_cast<uint8_t>(kAddress), static_cast<size_t>(len), true);
  if (readLen != len) {
    return false;
  }

  for (size_t i = 0; i < len; ++i) {
    buffer[i] = Wire.read();
  }
  return true;
#endif
}

bool TouchHandler::poll(TouchEvent &event) {
  event = TouchEvent{};

  if (!initialized_) {
    return false;
  }

  const uint32_t now = millis();
  if (now < backoffUntilMs_) {
    return false;
  }

  if (now - lastPollMs_ < kPollIntervalMs) {
    return false;
  }
  lastPollMs_ = now;

#if CONFIG_IDF_TARGET_ESP32C6
  // The AXS5106L INT line is unreliable on the Waveshare 1.47" board for
  // touches near the panel edges (the controller doesn't always fire an
  // interrupt when the finger lands in the weaker-capacitance edge bands).
  // Polling unconditionally at kPollIntervalMs costs ~50 I2C transactions per
  // second but ensures edge touches register.

  uint8_t data[14] = {0};
  if (!readTouchPacket(data, sizeof(data))) {
    backoffUntilMs_ = now + kFailureBackoffMs;
    if (++consecutiveReadFailures_ >= 5) {
      initialized_ = false;
      Serial.println("[touch] Read failed repeatedly, disabling touch polling");
    }
    return false;
  }
  consecutiveReadFailures_ = 0;

  // AXS5106L packet layout (14 bytes starting at register 0x01):
  //   data[0] = status / header
  //   data[1] = number of touch points
  //   data[2] = (event << 4) | x_high(4 bits)
  //   data[3] = x_low
  //   data[4] = (id    << 4) | y_high(4 bits)
  //   data[5] = y_low
  const uint8_t points = data[1];
  if (points == 0 || points >= 5) {
    if (touchActive_) {
      ++emptyTouchSamples_;
      if (emptyTouchSamples_ < kReleaseConfirmSamples) {
        return false;
      }
      touchActive_ = false;
      emptyTouchSamples_ = 0;
      event.touched = false;
      event.x = lastX_;
      event.y = lastY_;
      event.phase = TouchPhase::End;
      return true;
    }
    return false;
  }

  const uint16_t rawX = static_cast<uint16_t>(((data[2] & 0x0F) << 8) | data[3]);
  const uint16_t rawY = static_cast<uint16_t>(((data[4] & 0x0F) << 8) | data[5]);

  // Panel native is 172x320 portrait, app uses 320x172 landscape. Empirically:
  //   * rawY runs along the long (320) axis but decreases as the finger moves
  //     right, so out.x = (W-1) - rawY.
  //   * rawX runs along the short (172) axis but increases as the finger moves
  //     up, so out.y = (H-1) - rawX.
  const uint16_t clampedRawY = std::min<uint16_t>(rawY, BoardConfig::DISPLAY_WIDTH - 1);
  const uint16_t clampedRawX = std::min<uint16_t>(rawX, BoardConfig::DISPLAY_HEIGHT - 1);
  const uint16_t mappedX =
      static_cast<uint16_t>(BoardConfig::DISPLAY_WIDTH - 1 - clampedRawY);
  const uint16_t mappedY =
      static_cast<uint16_t>(BoardConfig::DISPLAY_HEIGHT - 1 - clampedRawX);

  event.touched = true;
  event.gesture = static_cast<uint8_t>((data[2] >> 4) & 0x0F);
  event.phase = touchActive_ ? TouchPhase::Move : TouchPhase::Start;
  if (BoardConfig::UI_ROTATED_180) {
    event.x = static_cast<uint16_t>(BoardConfig::DISPLAY_WIDTH - 1 - mappedX);
    event.y = static_cast<uint16_t>(BoardConfig::DISPLAY_HEIGHT - 1 - mappedY);
  } else {
    event.x = mappedX;
    event.y = mappedY;
  }

  backoffUntilMs_ = 0;
  emptyTouchSamples_ = 0;
  lastTouchSampleMs_ = now;
  touchActive_ = true;
  lastX_ = event.x;
  lastY_ = event.y;
  return true;
#else
  uint8_t data[8] = {0};
  if (!readTouchPacket(data, sizeof(data))) {
    backoffUntilMs_ = now + kFailureBackoffMs;
    if (++consecutiveReadFailures_ >= 5) {
      initialized_ = false;
      Serial.println("[touch] Read failed repeatedly, disabling touch polling");
    }
    return false;
  }
  consecutiveReadFailures_ = 0;

  const uint8_t points = data[1];
  if (points == 0 || points >= 5) {
    if (touchActive_) {
      ++emptyTouchSamples_;
      if (emptyTouchSamples_ < kReleaseConfirmSamples) {
        return false;
      }

      touchActive_ = false;
      emptyTouchSamples_ = 0;
      event.touched = false;
      event.x = lastX_;
      event.y = lastY_;
      event.phase = TouchPhase::End;
      return true;
    }
    return false;
  }

  backoffUntilMs_ = 0;
  consecutiveReadFailures_ = 0;
  emptyTouchSamples_ = 0;
  lastTouchSampleMs_ = now;

  event.touched = true;
  event.gesture = 0;
  event.phase = touchActive_ ? TouchPhase::Move : TouchPhase::Start;
  const uint16_t rawLongAxis = static_cast<uint16_t>(((data[2] & 0x0F) << 8) | data[3]);
  const uint16_t rawShortAxis = static_cast<uint16_t>(((data[4] & 0x0F) << 8) | data[5]);
  const uint16_t mappedX = clampDisplayX(rawLongAxis);
  const uint16_t mappedY = clampDisplayY(rawShortAxis);
  if (BoardConfig::UI_ROTATED_180) {
    event.x = static_cast<uint16_t>(BoardConfig::DISPLAY_WIDTH - 1 - mappedX);
    event.y = static_cast<uint16_t>(BoardConfig::DISPLAY_HEIGHT - 1 - mappedY);
  } else {
    event.x = mappedX;
    event.y = mappedY;
  }
  touchActive_ = true;
  lastX_ = event.x;
  lastY_ = event.y;

  return true;
#endif
}
