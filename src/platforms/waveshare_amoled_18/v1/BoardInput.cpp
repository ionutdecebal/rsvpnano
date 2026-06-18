#include "board/BoardInput.h"

#include <array>

#include <Wire.h>

#include "drivers/gpio/tca9554/Tca9554.h"
#include "drivers/touch/ft6336/ft6336.h"
#include "platforms/waveshare_amoled_18/WaveshareAmoled18.h"

namespace {

TwoWire &touchWire() { return Wire; }

bool tcaPinHeld(uint8_t pin) {
  bool levelHigh = false;
  return BoardDrivers::Tca9554::readInputPin(
             Wire1, WaveshareAmoled18::Tca9554Wiring::kAddress, pin, levelHigh,
             WaveshareAmoled18::Tca9554Wiring::kReleaseBusBeforeRead) &&
         levelHigh;
}

void resetTouchHardware() {
  if constexpr (WaveshareAmoled18::System::kTouchResetPin >= 0) {
    pinMode(WaveshareAmoled18::System::kTouchResetPin, OUTPUT);
    digitalWrite(WaveshareAmoled18::System::kTouchResetPin, LOW);
    delay(12);
    digitalWrite(WaveshareAmoled18::System::kTouchResetPin, HIGH);
    delay(12);
  }
}

bool primaryPressedRaw() {
  if constexpr (WaveshareAmoled18::Buttons::kBootPin < 0) {
    return false;
  }
  return !digitalRead(WaveshareAmoled18::Buttons::kBootPin);
}

bool powerPressedRaw() {
  return tcaPinHeld(WaveshareAmoled18::Tca9554Wiring::kPowerButtonPin);
}

void configureButtonPins() {
  if constexpr (WaveshareAmoled18::Buttons::kBootPin >= 0) {
    pinMode(WaveshareAmoled18::Buttons::kBootPin, INPUT_PULLUP);
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
  return {WaveshareAmoled18::Buttons::kDebounceMs,
          WaveshareAmoled18::Buttons::kShortPressMaxMs,
          WaveshareAmoled18::Buttons::kLongPressMs};
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
  return {WaveshareAmoled18::DisplayWiring::kPanelWidth,
          WaveshareAmoled18::DisplayWiring::kPanelHeight};
}

::Input::TouchTiming touchTiming() {
  ::Input::TouchTiming timing = {};
  timing.releaseConfirmSamples = WaveshareAmoled18::TouchWiring::kReleaseConfirmSamples;
  timing.maxConsecutiveReadFailures =
      WaveshareAmoled18::TouchWiring::kMaxConsecutiveReadFailures;
  timing.pollIntervalMs = WaveshareAmoled18::TouchWiring::kPollIntervalMs;
  timing.failureBackoffMs = WaveshareAmoled18::TouchWiring::kFailureBackoffMs;
  timing.recoveryRetryMs = WaveshareAmoled18::TouchWiring::kRecoveryRetryMs;
  timing.recoveryEventIgnoreMs = WaveshareAmoled18::TouchWiring::kRecoveryEventIgnoreMs;
  return timing;
}

bool beginTouch() {
  resetTouchHardware();
  TwoWire &wire = touchWire();
  return Ft6336Touch::probe(wire, WaveshareAmoled18::TouchWiring::kAddress) &&
         Ft6336Touch::configureMonitorMode(wire, WaveshareAmoled18::TouchWiring::kAddress);
}

bool touchReady() {
  if constexpr (WaveshareAmoled18::System::kTouchIrqPin < 0) {
    return true;
  }
  return !digitalRead(WaveshareAmoled18::System::kTouchIrqPin);
}

bool readTouch(::Input::TouchContact &contact) {
  std::array<uint8_t, Ft6336Touch::kPacketLength> data = {};
  if (!Ft6336Touch::readPacket(touchWire(), WaveshareAmoled18::TouchWiring::kAddress,
                               WaveshareAmoled18::TouchWiring::kReleaseBusBeforeRead, data.data(),
                               data.size())) {
    return false;
  }

  BoardDrivers::Touch::Sample decoded = {};
  if (!Ft6336Touch::decodePacket(data.data(), data.size(),
                                 WaveshareAmoled18::DisplayWiring::kPanelWidth,
                                 WaveshareAmoled18::DisplayWiring::kPanelHeight, decoded)) {
    return false;
  }

  contact = {decoded.touched, decoded.physicalX, decoded.physicalY};
  return true;
}

}  // namespace Board::Input
