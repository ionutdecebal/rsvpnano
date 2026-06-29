#include "board/BoardPower.h"

#include "drivers/power/axp2101/Axp2101.h"
#include "platforms/waveshare_amoled_216/WaveshareAmoled216.h"

namespace {

constexpr BoardDrivers::Axp2101::Config kAxp2101Config = {
    WaveshareAmoled216::Axp2101Wiring::kReleaseBusBeforeRead,
    WaveshareAmoled216::Axp2101Wiring::kEnablePowerKeyIrqs,
    WaveshareAmoled216::Axp2101Wiring::kRequiresPowerKeyConfig,
    WaveshareAmoled216::Axp2101Wiring::kPowerKeyOnTimeValue,
    WaveshareAmoled216::Axp2101Wiring::kPowerKeyOffTimeValue,
};

}  // namespace

namespace Board::Power {

void begin() { BoardDrivers::Axp2101::begin(kAxp2101Config); }

void prepareDeepSleepPowerHold() {}

bool enableAudioPowerIfAvailable() { return true; }

bool readBatteryStatus(BatteryStatus &status) {
  return BoardDrivers::Axp2101::readBatteryStatus(status);
}

DiagnosticSnapshot diagnosticSnapshot() { return BoardDrivers::Axp2101::diagnosticSnapshot(); }

bool externalPowerPresent() { return BoardDrivers::Axp2101::externalPowerPresent(); }

bool releaseBatteryPowerHold() { return BoardDrivers::Axp2101::releasePower(); }

bool supportsSoftwarePowerOff() { return true; }

bool powerOffUsesControllerWake() {
  return WaveshareAmoled216::Power::kRequestPmuShutdownOnPowerOff;
}

bool shouldRequestShutdownOnPowerOff() {
  return WaveshareAmoled216::Power::kRequestPmuShutdownOnPowerOff;
}

bool shouldReleaseBatteryPowerBeforeDeepSleep() {
  return WaveshareAmoled216::Power::kReleaseBatteryHoldBeforeDeepSleep;
}

}  // namespace Board::Power
