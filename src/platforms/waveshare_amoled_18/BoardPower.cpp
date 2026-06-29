#include "board/BoardPower.h"

#include <Wire.h>

#include "drivers/power/axp2101/Axp2101.h"
#include "drivers/gpio/tca9554/Tca9554.h"
#include "platforms/waveshare_amoled_18/WaveshareAmoled18.h"

namespace {

constexpr BoardDrivers::Axp2101::Config kAxp2101Config = {
    WaveshareAmoled18::Axp2101Wiring::kReleaseBusBeforeRead,
    WaveshareAmoled18::Axp2101Wiring::kEnablePowerKeyIrqs,
    WaveshareAmoled18::Axp2101Wiring::kRequiresPowerKeyConfig,
    WaveshareAmoled18::Axp2101Wiring::kPowerKeyOnTimeValue,
    WaveshareAmoled18::Axp2101Wiring::kPowerKeyOffTimeValue,
};

void configureIoExpander() {
  BoardDrivers::Tca9554::PortState state = {};
  if (!BoardDrivers::Tca9554::readPortState(
          Wire1, WaveshareAmoled18::Tca9554Wiring::kAddress, state,
          WaveshareAmoled18::Tca9554Wiring::kReleaseBusBeforeRead)) {
    Serial.println("[board] TCA9554 not detected");
    return;
  }

  state.output &= WaveshareAmoled18::Tca9554Wiring::kDisplayClearMask;
  state.output |= WaveshareAmoled18::Tca9554Wiring::kSdEnableMask;
  state.config &= WaveshareAmoled18::Tca9554Wiring::kOutputClearMask;
  state.config |= WaveshareAmoled18::Tca9554Wiring::kInputMask;

  if (!BoardDrivers::Tca9554::writePortState(
          Wire1, WaveshareAmoled18::Tca9554Wiring::kAddress, state)) {
    Serial.println("[board] TCA9554 output setup failed");
    return;
  }
}

}  // namespace

namespace Board::Power {

void begin() {
  configureIoExpander();
  BoardDrivers::Axp2101::begin(kAxp2101Config);
}

void prepareDeepSleepPowerHold() {}

bool enableAudioPowerIfAvailable() { return true; }

bool readBatteryStatus(BatteryStatus &status) {
  return BoardDrivers::Axp2101::readBatteryStatus(status);
}

DiagnosticSnapshot diagnosticSnapshot() { return BoardDrivers::Axp2101::diagnosticSnapshot(); }

bool externalPowerPresent() { return BoardDrivers::Axp2101::externalPowerPresent(); }

bool releaseBatteryPowerHold() { return BoardDrivers::Axp2101::releasePower(); }

bool supportsSoftwarePowerOff() { return true; }

bool powerOffUsesControllerWake() { return WaveshareAmoled18::Power::kRequestPmuShutdownOnPowerOff; }

bool shouldRequestShutdownOnPowerOff() {
  return WaveshareAmoled18::Power::kRequestPmuShutdownOnPowerOff;
}

bool shouldReleaseBatteryPowerBeforeDeepSleep() {
  return WaveshareAmoled18::Power::kReleaseBatteryHoldBeforeDeepSleep;
}

}  // namespace Board::Power
