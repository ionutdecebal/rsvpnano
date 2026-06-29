#pragma once

#include <Arduino.h>

#include "board/BoardTypes.h"

namespace Input {

using ControlMask = uint8_t;

enum Control : ControlMask {
  InputNone = 0,
  InputPrimary = 1U << 0,
  InputPower = 1U << 1,
  InputKey = 1U << 2,
  InputTouch = 1U << 3,
};

enum class Gesture : uint8_t {
  None,

  ShortPressed,
  LongPressed,
  TriplePressed,

  TouchStart,
  TouchMove,
  TouchEnd,
  Tapped,
  TopEdgeSwiped,
  BottomEdgeSwiped,
};

struct Event {
  ControlMask controls = InputNone;
  Gesture gesture = Gesture::None;
  uint16_t x = 0;
  uint16_t y = 0;
};

struct ControlTiming {
  uint16_t debounceMs = 25;
  uint16_t shortPressMaxMs = 700;
  uint16_t longPressMs = 900;
};

struct TouchContact {
  bool touched = false;
  uint16_t x = 0;
  uint16_t y = 0;
};

struct TouchSurface {
  uint16_t width = 0;
  uint16_t height = 0;
};

struct TouchTiming {
  uint8_t releaseConfirmSamples = 2;
  uint8_t maxConsecutiveReadFailures = 5;
  uint16_t edgeSizePx = 32;
  uint16_t swipeMinDistancePx = 48;
  uint16_t tapMoveTolerancePx = 20;
  uint16_t tapMaxDurationMs = 300;
  uint32_t pollIntervalMs = 20;
  uint32_t failureBackoffMs = 250;
  uint32_t recoveryRetryMs = 1000;
  uint32_t recoveryEventIgnoreMs = 0;
};

constexpr bool hasControl(ControlMask controls, ControlMask control) {
  return (controls & control) != 0;
}

constexpr bool hasControls(ControlMask controls, ControlMask required) {
  return (controls & required) == required;
}

constexpr bool isTouchEvent(const Event &event) {
  return hasControl(event.controls, InputTouch);
}

constexpr bool isTouchReleaseGesture(Gesture gesture) {
  return gesture == Gesture::TouchEnd || gesture == Gesture::Tapped ||
         gesture == Gesture::TopEdgeSwiped || gesture == Gesture::BottomEdgeSwiped;
}

bool begin();
void end();
void cancel();
bool poll(Event &event, uint32_t nowMs);

}  // namespace Input
