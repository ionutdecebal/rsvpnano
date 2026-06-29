#include "board/BoardInput.h"

#include <array>

#include <Wire.h>

#include "drivers/touch/axs15231b_touch/axs15231b_touch.h"
#include "platforms/waveshare_lcd_349/WaveshareLcd349.h"

namespace {

TwoWire &touchWire() { return Wire; }

void resetTouchHardware() {
  if constexpr (WaveshareLcd349::System::kTouchResetPin >= 0) {
    pinMode(WaveshareLcd349::System::kTouchResetPin, OUTPUT);
    digitalWrite(WaveshareLcd349::System::kTouchResetPin, LOW);
    delay(12);
    digitalWrite(WaveshareLcd349::System::kTouchResetPin, HIGH);
    delay(12);
  }
}

bool primaryPressedRaw() {
  if constexpr (WaveshareLcd349::Buttons::kBootPin < 0) {
    return false;
  }
  return !digitalRead(WaveshareLcd349::Buttons::kBootPin);
}

bool powerPressedRaw() {
  if constexpr (WaveshareLcd349::Buttons::kPowerPin < 0) {
    return false;
  }
  return !digitalRead(WaveshareLcd349::Buttons::kPowerPin);
}

void configureButtonPins() {
  if constexpr (WaveshareLcd349::Buttons::kBootPin >= 0) {
    pinMode(WaveshareLcd349::Buttons::kBootPin, INPUT_PULLUP);
  }
  if constexpr (WaveshareLcd349::Buttons::kPowerPin >= 0) {
    pinMode(WaveshareLcd349::Buttons::kPowerPin, INPUT_PULLUP);
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
  return {WaveshareLcd349::Buttons::kDebounceMs, WaveshareLcd349::Buttons::kShortPressMaxMs,
          WaveshareLcd349::Buttons::kLongPressMs};
}

::Input::ControlMask currentControls() {
  ::Input::ControlMask controls = ::Input::InputNone;
  if (primaryPressedRaw()) {
    controls |= ::Input::InputPrimary;
  }
  if (powerPressedRaw()) {
    controls |= ::Input::InputPower;
  }
  return controls;
}

::Input::TouchSurface touchSurface() {
  return {WaveshareLcd349::DisplayWiring::kPanelWidth,
          WaveshareLcd349::DisplayWiring::kPanelHeight};
}

::Input::TouchTiming touchTiming() {
  ::Input::TouchTiming timing = {};
  timing.releaseConfirmSamples = WaveshareLcd349::TouchWiring::kReleaseConfirmSamples;
  timing.maxConsecutiveReadFailures = WaveshareLcd349::TouchWiring::kMaxConsecutiveReadFailures;
  timing.pollIntervalMs = WaveshareLcd349::TouchWiring::kPollIntervalMs;
  timing.failureBackoffMs = WaveshareLcd349::TouchWiring::kFailureBackoffMs;
  timing.recoveryRetryMs = WaveshareLcd349::TouchWiring::kRecoveryRetryMs;
  timing.recoveryEventIgnoreMs = WaveshareLcd349::TouchWiring::kRecoveryEventIgnoreMs;
  return timing;
}

bool beginTouch() {
  resetTouchHardware();
  return Axs15231bTouch::probe(touchWire(), WaveshareLcd349::TouchWiring::kAddress);
}

bool touchReady() {
  if constexpr (WaveshareLcd349::System::kTouchIrqPin < 0) {
    return true;
  }
  return !digitalRead(WaveshareLcd349::System::kTouchIrqPin);
}

bool readTouch(::Input::TouchContact &contact) {
  std::array<uint8_t, Axs15231bTouch::kPacketLength> data = {};
  if (!Axs15231bTouch::readPacket(touchWire(), WaveshareLcd349::TouchWiring::kAddress,
                                  data.data(), data.size())) {
    return false;
  }

  BoardDrivers::Touch::Sample decoded = {};
  if (!Axs15231bTouch::decodePacket(data.data(), data.size(),
                                    WaveshareLcd349::DisplayWiring::kPanelWidth,
                                    WaveshareLcd349::DisplayWiring::kPanelHeight, decoded)) {
    return false;
  }

  contact = {decoded.touched, decoded.physicalX, decoded.physicalY};
  return true;
}

}  // namespace Board::Input
