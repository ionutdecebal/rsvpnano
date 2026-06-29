#include "board/BoardInput.h"

#include <array>

#include <Wire.h>

#include "drivers/touch/ft6336/ft6336.h"
#include "platforms/waveshare_amoled_241/WaveshareAmoled241.h"

namespace {

TwoWire &touchWire() { return Wire1; }

void resetTouchHardware() {
  if constexpr (WaveshareAmoled241::System::kTouchResetPin >= 0) {
    pinMode(WaveshareAmoled241::System::kTouchResetPin, OUTPUT);
    digitalWrite(WaveshareAmoled241::System::kTouchResetPin, LOW);
    delay(12);
    digitalWrite(WaveshareAmoled241::System::kTouchResetPin, HIGH);
    delay(12);
  }
}

bool primaryPressedRaw() {
  if constexpr (WaveshareAmoled241::Buttons::kBootPin < 0) {
    return false;
  }
  return !digitalRead(WaveshareAmoled241::Buttons::kBootPin);
}

bool powerPressedRaw() {
  if constexpr (WaveshareAmoled241::Buttons::kPowerPin < 0) {
    return false;
  }
  return !digitalRead(WaveshareAmoled241::Buttons::kPowerPin);
}

void configureButtonPins() {
  if constexpr (WaveshareAmoled241::Buttons::kBootPin >= 0) {
    pinMode(WaveshareAmoled241::Buttons::kBootPin, INPUT_PULLUP);
  }
  if constexpr (WaveshareAmoled241::Buttons::kPowerPin >= 0) {
    pinMode(WaveshareAmoled241::Buttons::kPowerPin, INPUT_PULLUP);
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
  return {WaveshareAmoled241::Buttons::kDebounceMs,
          WaveshareAmoled241::Buttons::kShortPressMaxMs,
          WaveshareAmoled241::Buttons::kLongPressMs};
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
  return {WaveshareAmoled241::DisplayWiring::kPanelWidth,
          WaveshareAmoled241::DisplayWiring::kPanelHeight};
}

::Input::TouchTiming touchTiming() {
  ::Input::TouchTiming timing = {};
  timing.releaseConfirmSamples = WaveshareAmoled241::TouchWiring::kReleaseConfirmSamples;
  timing.maxConsecutiveReadFailures =
      WaveshareAmoled241::TouchWiring::kMaxConsecutiveReadFailures;
  timing.pollIntervalMs = WaveshareAmoled241::TouchWiring::kPollIntervalMs;
  timing.failureBackoffMs = WaveshareAmoled241::TouchWiring::kFailureBackoffMs;
  timing.recoveryRetryMs = WaveshareAmoled241::TouchWiring::kRecoveryRetryMs;
  timing.recoveryEventIgnoreMs = WaveshareAmoled241::TouchWiring::kRecoveryEventIgnoreMs;
  return timing;
}

bool beginTouch() {
  resetTouchHardware();
  return Ft6336Touch::probe(touchWire(), WaveshareAmoled241::TouchWiring::kAddress);
}

bool touchReady() {
  if constexpr (WaveshareAmoled241::System::kTouchIrqPin < 0) {
    return true;
  }
  return !digitalRead(WaveshareAmoled241::System::kTouchIrqPin);
}

bool readTouch(::Input::TouchContact &contact) {
  std::array<uint8_t, Ft6336Touch::kPacketLength> data = {};
  if (!Ft6336Touch::readPacket(touchWire(), WaveshareAmoled241::TouchWiring::kAddress,
                               WaveshareAmoled241::TouchWiring::kReleaseBusBeforeRead, data.data(),
                               data.size())) {
    return false;
  }

  BoardDrivers::Touch::Sample decoded = {};
  if (!Ft6336Touch::decodePacket(data.data(), data.size(),
                                 WaveshareAmoled241::DisplayWiring::kPanelWidth,
                                 WaveshareAmoled241::DisplayWiring::kPanelHeight, decoded)) {
    return false;
  }

  contact = {decoded.touched, decoded.physicalX, decoded.physicalY};
  return true;
}

}  // namespace Board::Input
