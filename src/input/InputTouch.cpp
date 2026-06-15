#include "input/InputTouch.h"

#include <algorithm>

#include "board/BoardTouch.h"

namespace Input::Touch {
namespace {

constexpr uint8_t kReleaseConfirmSamples = 2;
constexpr uint8_t kMaxConsecutiveReadFailures = 5;
constexpr size_t kMaxTouchPacketBytes = 15;

struct TouchState {
  bool initialized = false;
  uint32_t lastPollMs = 0;
  uint32_t backoffUntilMs = 0;
  uint32_t ignoreEventsUntilMs = 0;
  uint32_t lastSampleMs = 0;
  uint8_t consecutiveReadFailures = 0;
  uint8_t emptySamples = 0;
  bool active = false;
  Board::Config::UiOrientation uiOrientation = Board::Config::DEFAULT_UI_ORIENTATION;
  uint16_t lastX = 0;
  uint16_t lastY = 0;
};

TouchState gTouch;

void resetRuntimeState() {
  gTouch.lastPollMs = 0;
  gTouch.backoffUntilMs = 0;
  gTouch.ignoreEventsUntilMs = 0;
  gTouch.lastSampleMs = 0;
  gTouch.consecutiveReadFailures = 0;
  gTouch.emptySamples = 0;
  gTouch.active = false;
  gTouch.lastX = 0;
  gTouch.lastY = 0;
}

uint16_t clampDisplayX(uint16_t x) {
  return std::min<uint16_t>(x, static_cast<uint16_t>(Board::Config::DISPLAY_WIDTH - 1));
}

uint16_t clampDisplayY(uint16_t y) {
  return std::min<uint16_t>(y, static_cast<uint16_t>(Board::Config::DISPLAY_HEIGHT - 1));
}

bool releaseTouchIfNeeded(Event &event) {
  if (!gTouch.active) {
    return false;
  }

  ++gTouch.emptySamples;
  if (gTouch.emptySamples < kReleaseConfirmSamples) {
    return false;
  }

  gTouch.active = false;
  gTouch.emptySamples = 0;
  event.touched = false;
  event.x = gTouch.lastX;
  event.y = gTouch.lastY;
  event.phase = Phase::End;
  return true;
}

void mapSampleToEvent(const BoardDrivers::Touch::Sample &sample, Event &event) {
  const uint16_t physicalX = sample.physicalX;
  const uint16_t physicalY = sample.physicalY;

  switch (gTouch.uiOrientation) {
    case Board::Config::UiOrientation::Portrait:
      event.x = physicalX;
      event.y = physicalY;
      break;
    case Board::Config::UiOrientation::PortraitFlipped:
      event.x = static_cast<uint16_t>(Board::Config::PANEL_NATIVE_WIDTH - 1 - physicalX);
      event.y = static_cast<uint16_t>(Board::Config::PANEL_NATIVE_HEIGHT - 1 - physicalY);
      break;
    case Board::Config::UiOrientation::Landscape:
      event.x =
          clampDisplayX(static_cast<uint16_t>(Board::Config::PANEL_NATIVE_HEIGHT - 1 - physicalY));
      event.y = clampDisplayY(physicalX);
      break;
    case Board::Config::UiOrientation::LandscapeFlipped:
    default:
      event.x = clampDisplayX(physicalY);
      event.y =
          clampDisplayY(static_cast<uint16_t>(Board::Config::PANEL_NATIVE_WIDTH - 1 - physicalX));
      break;
  }
}

}  // namespace

bool begin() {
  resetRuntimeState();

  Board::Touch::resetController();

  const size_t touchPacketLength = Board::Touch::packetLength();
  if (touchPacketLength == 0 || touchPacketLength > kMaxTouchPacketBytes) {
    gTouch.initialized = false;
    Serial.println("[touch] Hardware provider is incomplete");
    return false;
  }

  TwoWire &wire = Board::Touch::wire();
  wire.beginTransmission(Board::Config::TOUCH_I2C_ADDRESS);
  const uint8_t error = wire.endTransmission();
  gTouch.initialized = (error == 0);

  if (!gTouch.initialized) {
    Serial.printf("[touch] Controller not detected at 0x%02X\n", Board::Config::TOUCH_I2C_ADDRESS);
    return false;
  }

  Serial.printf("[touch] Initialized controller=0x%02X\n", Board::Config::TOUCH_I2C_ADDRESS);
  if (Board::Config::TOUCH_RECOVERY_EVENT_IGNORE_MS > 0) {
    gTouch.ignoreEventsUntilMs = millis() + Board::Config::TOUCH_RECOVERY_EVENT_IGNORE_MS;
    Serial.printf("[touch] Ignoring events for %lu ms after init\n",
                  static_cast<unsigned long>(Board::Config::TOUCH_RECOVERY_EVENT_IGNORE_MS));
  }

  if (Board::Config::TOUCH_REQUIRES_MONITOR_MODE) {
    const bool configured = Board::Touch::configure();
    if (configured) {
      Serial.printf("[touch] Applied monitor mode reg=0x%02X value=0x%02X\n",
                    Board::Config::TOUCH_MONITOR_MODE_REGISTER,
                    Board::Config::TOUCH_MONITOR_MODE_VALUE);
    } else {
      Serial.println("[touch] Failed to apply board-specific monitor mode");
    }
  }

  return true;
}

void end() {
  cancel();
  gTouch.initialized = false;
}

void cancel() { resetRuntimeState(); }

bool readEvent(Event &event) {
  event = Event{};

  const size_t touchPacketLength = Board::Touch::packetLength();
  const uint32_t now = millis();

  if (!gTouch.initialized) {
    if (now < gTouch.backoffUntilMs) {
      return false;
    }

    Serial.println("[touch] Attempting controller recovery");
    if (!begin()) {
      gTouch.backoffUntilMs = now + Board::Config::TOUCH_RECOVERY_RETRY_MS;
    }
    return false;
  }

  if (now < gTouch.backoffUntilMs) {
    return false;
  }

  if (now - gTouch.lastPollMs < Board::Config::TOUCH_POLL_INTERVAL_MS) {
    return false;
  }
  gTouch.lastPollMs = now;

  if (!Board::Touch::ready()) {
    return releaseTouchIfNeeded(event);
  }

  uint8_t data[kMaxTouchPacketBytes] = {};
  if (!Board::Touch::readPacket(data, touchPacketLength)) {
    gTouch.backoffUntilMs = now + Board::Config::TOUCH_FAILURE_BACKOFF_MS;
    if (++gTouch.consecutiveReadFailures >= kMaxConsecutiveReadFailures) {
      gTouch.initialized = false;
      gTouch.active = false;
      gTouch.emptySamples = 0;
      gTouch.backoffUntilMs = now + Board::Config::TOUCH_RECOVERY_RETRY_MS;
      Serial.println("[touch] Read failed repeatedly, scheduling controller recovery");
    }
    return false;
  }

  gTouch.consecutiveReadFailures = 0;

  BoardDrivers::Touch::Sample sample;
  if (!Board::Touch::decodePacket(data, touchPacketLength, sample)) {
    gTouch.backoffUntilMs = now + Board::Config::TOUCH_FAILURE_BACKOFF_MS;
    return false;
  }
  if (!sample.touched) {
    return releaseTouchIfNeeded(event);
  }

  if (now < gTouch.ignoreEventsUntilMs) {
    gTouch.active = false;
    gTouch.emptySamples = 0;
    return false;
  }

  gTouch.backoffUntilMs = 0;
  gTouch.consecutiveReadFailures = 0;
  gTouch.emptySamples = 0;
  gTouch.lastSampleMs = now;

  event.touched = true;
  event.gesture = 0;
  event.phase = gTouch.active ? Phase::Move : Phase::Start;
  mapSampleToEvent(sample, event);

  gTouch.active = true;
  gTouch.lastX = event.x;
  gTouch.lastY = event.y;

  return true;
}

void setUiOrientation(Board::Config::UiOrientation orientation) {
  if (gTouch.uiOrientation == orientation) {
    return;
  }

  gTouch.uiOrientation = orientation;
  cancel();
}

void setUiRotated180(bool rotated180) {
  setUiOrientation(rotated180 ? Board::Config::ROTATED_UI_ORIENTATION
                              : Board::Config::DEFAULT_UI_ORIENTATION);
}

}  // namespace Input::Touch
