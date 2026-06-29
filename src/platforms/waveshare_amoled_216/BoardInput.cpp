#include "board/BoardInput.h"

#include <array>

#include <Wire.h>

#include "drivers/power/axp2101/Axp2101.h"
#include "drivers/touch/cst92xx/cst92xx.h"
#include "platforms/waveshare_amoled_216/WaveshareAmoled216.h"

namespace {

TwoWire &touchWire() { return Wire; }

void resetTouchHardware() {
  if constexpr (WaveshareAmoled216::System::kTouchResetPin >= 0) {
    pinMode(WaveshareAmoled216::System::kTouchResetPin, OUTPUT);
    digitalWrite(WaveshareAmoled216::System::kTouchResetPin, LOW);
    delay(12);
    digitalWrite(WaveshareAmoled216::System::kTouchResetPin, HIGH);
    delay(12);
  }
}

bool primaryPressedRaw() {
  if constexpr (WaveshareAmoled216::Buttons::kBootPin < 0) {
    return false;
  }
  return !digitalRead(WaveshareAmoled216::Buttons::kBootPin);
}

bool powerPressedRaw() { return BoardDrivers::Axp2101::isPowerButtonHeld(); }

bool keyPressedRaw() {
  if constexpr (WaveshareAmoled216::Buttons::kKeyPin < 0) {
    return false;
  }
  return !digitalRead(WaveshareAmoled216::Buttons::kKeyPin);
}

void configureButtonPins() {
  if constexpr (WaveshareAmoled216::Buttons::kBootPin >= 0) {
    pinMode(WaveshareAmoled216::Buttons::kBootPin, INPUT_PULLUP);
  }
  if constexpr (WaveshareAmoled216::Buttons::kKeyPin >= 0) {
    pinMode(WaveshareAmoled216::Buttons::kKeyPin, INPUT_PULLUP);
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
  return {WaveshareAmoled216::Buttons::kDebounceMs,
          WaveshareAmoled216::Buttons::kShortPressMaxMs,
          WaveshareAmoled216::Buttons::kLongPressMs};
}

::Input::ControlMask currentControls() {
  ::Input::ControlMask controls = ::Input::InputNone;
  if (primaryPressedRaw()) {
    controls |= ::Input::InputPrimary;
  }
  if (powerPressedRaw()) {
    controls |= ::Input::InputPower;
  }
  if (keyPressedRaw()) {
    controls |= ::Input::InputKey;
  }
  return controls;
}

::Input::TouchSurface touchSurface() {
  return {WaveshareAmoled216::DisplayWiring::kPanelWidth,
          WaveshareAmoled216::DisplayWiring::kPanelHeight};
}

::Input::TouchTiming touchTiming() {
  ::Input::TouchTiming timing = {};
  timing.releaseConfirmSamples = WaveshareAmoled216::TouchWiring::kReleaseConfirmSamples;
  timing.maxConsecutiveReadFailures =
      WaveshareAmoled216::TouchWiring::kMaxConsecutiveReadFailures;
  timing.pollIntervalMs = WaveshareAmoled216::TouchWiring::kPollIntervalMs;
  timing.failureBackoffMs = WaveshareAmoled216::TouchWiring::kFailureBackoffMs;
  timing.recoveryRetryMs = WaveshareAmoled216::TouchWiring::kRecoveryRetryMs;
  timing.recoveryEventIgnoreMs = WaveshareAmoled216::TouchWiring::kRecoveryEventIgnoreMs;
  return timing;
}

bool beginTouch() {
  resetTouchHardware();
  TwoWire &wire = touchWire();
  return Cst92xxTouch::probe(wire, WaveshareAmoled216::TouchWiring::kAddress) &&
         Cst92xxTouch::configureMonitorMode(wire, WaveshareAmoled216::TouchWiring::kAddress);
}

bool touchReady() {
  if constexpr (WaveshareAmoled216::System::kTouchIrqPin < 0) {
    return true;
  }
  return !digitalRead(WaveshareAmoled216::System::kTouchIrqPin);
}

bool readTouch(::Input::TouchContact &contact) {
  std::array<uint8_t, Cst92xxTouch::kPacketLength> data = {};
  if (!Cst92xxTouch::readPacket(touchWire(), WaveshareAmoled216::TouchWiring::kAddress,
                                data.data(), data.size())) {
    return false;
  }

  BoardDrivers::Touch::Sample decoded = {};
  if (!Cst92xxTouch::decodePacket(data.data(), data.size(),
                                  WaveshareAmoled216::DisplayWiring::kPanelWidth,
                                  WaveshareAmoled216::DisplayWiring::kPanelHeight, decoded)) {
    return false;
  }

  contact = {decoded.touched, decoded.physicalX, decoded.physicalY};
  return true;
}

}  // namespace Board::Input
