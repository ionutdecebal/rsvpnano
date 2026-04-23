#include "input/TouchHandler.h"

#include <algorithm>
#include <Wire.h>

#include "board/BoardConfig.h"

namespace {

constexpr uint8_t kReadTouchCommand[] = {
    0xB5, 0xAB, 0xA5, 0x5A, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00,
};
constexpr uint32_t kPollIntervalMs = 20;
constexpr uint32_t kFailureBackoffMs = 250;
constexpr uint8_t kReleaseConfirmSamples = 2;

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
  Wire.beginTransmission(kAddress);
  const uint8_t error = Wire.endTransmission();
  initialized_ = (error == 0);

  if (!initialized_) {
    Serial.println("[touch] Controller not detected at 0x3B");
  } else {
    Serial.println("[touch] Initialized (AXS15231B)");
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
}
