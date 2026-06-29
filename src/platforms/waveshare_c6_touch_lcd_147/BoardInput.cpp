#include "board/BoardInput.h"

#include <array>

#include <Wire.h>

#include "drivers/touch/axs5106/axs5106.h"
#include "platforms/waveshare_c6_touch_lcd_147/WaveshareC6TouchLcd147.h"

namespace {

TwoWire &touchWire() { return Wire; }

void resetTouchHardware() {
  if constexpr (WaveshareC6TouchLcd147::System::kTouchResetPin >= 0) {
    pinMode(WaveshareC6TouchLcd147::System::kTouchResetPin, OUTPUT);
    digitalWrite(WaveshareC6TouchLcd147::System::kTouchResetPin, LOW);
    delay(10);
    digitalWrite(WaveshareC6TouchLcd147::System::kTouchResetPin, HIGH);
    delay(10);
  }
}

bool primaryPressedRaw() {
  if constexpr (WaveshareC6TouchLcd147::Buttons::kBootPin < 0) {
    return false;
  }
  return !digitalRead(WaveshareC6TouchLcd147::Buttons::kBootPin);
}

void configureButtonPins() {
  if constexpr (WaveshareC6TouchLcd147::Buttons::kBootPin >= 0) {
    pinMode(WaveshareC6TouchLcd147::Buttons::kBootPin, INPUT_PULLUP);
  }
}

}  // namespace

namespace Board::Input {

bool begin() {
  configureButtonPins();
  return true;
}

void end() {}

void cancel() {}

::Input::ControlTiming controlTiming() {
  return {WaveshareC6TouchLcd147::Buttons::kDebounceMs,
          WaveshareC6TouchLcd147::Buttons::kShortPressMaxMs,
          WaveshareC6TouchLcd147::Buttons::kLongPressMs};
}

::Input::ControlMask currentControls() {
  ::Input::ControlMask controls = ::Input::InputNone;
  if (primaryPressedRaw()) {
    controls |= ::Input::InputPrimary;
  }
  return controls;
}

::Input::TouchSurface touchSurface() {
  return {WaveshareC6TouchLcd147::DisplayWiring::kPanelWidth,
          WaveshareC6TouchLcd147::DisplayWiring::kPanelHeight};
}

::Input::TouchTiming touchTiming() {
  ::Input::TouchTiming timing = {};
  timing.releaseConfirmSamples = WaveshareC6TouchLcd147::TouchWiring::kReleaseConfirmSamples;
  timing.maxConsecutiveReadFailures =
      WaveshareC6TouchLcd147::TouchWiring::kMaxConsecutiveReadFailures;
  timing.pollIntervalMs = WaveshareC6TouchLcd147::TouchWiring::kPollIntervalMs;
  timing.failureBackoffMs = WaveshareC6TouchLcd147::TouchWiring::kFailureBackoffMs;
  timing.recoveryRetryMs = WaveshareC6TouchLcd147::TouchWiring::kRecoveryRetryMs;
  timing.recoveryEventIgnoreMs = WaveshareC6TouchLcd147::TouchWiring::kRecoveryEventIgnoreMs;
  return timing;
}

bool beginTouch() {
  resetTouchHardware();
  return Axs5106Touch::probe(touchWire(), WaveshareC6TouchLcd147::TouchWiring::kAddress);
}

bool touchReady() {
  if constexpr (WaveshareC6TouchLcd147::System::kTouchIrqPin < 0) {
    return true;
  }
  return !digitalRead(WaveshareC6TouchLcd147::System::kTouchIrqPin);
}

bool readTouch(::Input::TouchContact &contact) {
  std::array<uint8_t, Axs5106Touch::kPacketLength> data = {};
  if (!Axs5106Touch::readPacket(touchWire(), WaveshareC6TouchLcd147::TouchWiring::kAddress,
                                WaveshareC6TouchLcd147::TouchWiring::kReleaseBusBeforeRead,
                                data.data(), data.size())) {
    return false;
  }

  BoardDrivers::Touch::Sample decoded = {};
  if (!Axs5106Touch::decodePacket(data.data(), data.size(),
                                  WaveshareC6TouchLcd147::DisplayWiring::kPanelWidth,
                                  WaveshareC6TouchLcd147::DisplayWiring::kPanelHeight, decoded)) {
    return false;
  }

  contact = {decoded.touched, decoded.physicalX, decoded.physicalY};
  return true;
}

}  // namespace Board::Input
