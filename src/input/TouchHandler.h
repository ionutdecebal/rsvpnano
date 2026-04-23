#pragma once

#include <Arduino.h>

enum class TouchPhase {
  Start,
  Move,
  End,
};

struct TouchEvent {
  bool touched = false;
  uint16_t x = 0;
  uint16_t y = 0;
  uint8_t gesture = 0;
  TouchPhase phase = TouchPhase::Move;
};

class TouchHandler {
 public:
  bool begin();
  void end();
  bool poll(TouchEvent &event);
  void cancel();

 private:
  static constexpr uint8_t kAddress = 0x3B;  // AXS15231B touch endpoint on the 3.49" board.
  bool initialized_ = false;
  uint32_t lastPollMs_ = 0;
  uint32_t backoffUntilMs_ = 0;
  uint32_t lastTouchSampleMs_ = 0;
  uint8_t consecutiveReadFailures_ = 0;
  uint8_t emptyTouchSamples_ = 0;
  bool touchActive_ = false;
  uint16_t lastX_ = 0;
  uint16_t lastY_ = 0;

  bool readTouchPacket(uint8_t *buffer, size_t len);
};
