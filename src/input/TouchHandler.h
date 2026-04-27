#pragma once

#include <Arduino.h>
#include <sdkconfig.h>

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
#if CONFIG_IDF_TARGET_ESP32C6
  static constexpr uint8_t kAddress = 0x63;  // AXS5106L (Waveshare C6-Touch-LCD-1.47).
#else
  static constexpr uint8_t kAddress = 0x3B;  // AXS15231B (Waveshare S3-Touch-LCD-3.49).
#endif
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
