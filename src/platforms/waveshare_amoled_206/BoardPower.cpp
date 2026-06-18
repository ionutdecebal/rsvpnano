#include "board/BoardPower.h"

#include "drivers/power/axp2101/Axp2101.h"
#include "platforms/waveshare_amoled_206/WaveshareAmoled206.h"

namespace {

constexpr BoardDrivers::Axp2101::Config kAxp2101Config = {
    WaveshareAmoled206::Axp2101Wiring::kReleaseBusBeforeRead,
    WaveshareAmoled206::Axp2101Wiring::kEnablePowerKeyIrqs,
    WaveshareAmoled206::Axp2101Wiring::kRequiresPowerKeyConfig,
    WaveshareAmoled206::Axp2101Wiring::kPowerKeyOnTimeValue,
    WaveshareAmoled206::Axp2101Wiring::kPowerKeyOffTimeValue,
};

}  // namespace

namespace Board::Power {

void begin() { BoardDrivers::Axp2101::begin(kAxp2101Config); }

void prepareDeepSleepPowerHold() {}

void resetWakePeripherals() {}

bool enableAudioPowerIfAvailable() {
  pinMode(WaveshareAmoled206::AudioWiring::kAudioEnablePin, OUTPUT);
  digitalWrite(WaveshareAmoled206::AudioWiring::kAudioEnablePin, HIGH);
  return true;
}

bool readBatteryStatus(BatteryStatus &status) {
  return BoardDrivers::Axp2101::readBatteryStatus(status);
}

DiagnosticSnapshot diagnosticSnapshot() { return BoardDrivers::Axp2101::diagnosticSnapshot(); }

bool externalPowerPresent() { return BoardDrivers::Axp2101::externalPowerPresent(); }

bool releaseBatteryPowerHold() { return BoardDrivers::Axp2101::releasePower(); }

bool supportsSoftwarePowerOff() { return true; }

bool powerOffUsesControllerWake() {
  return WaveshareAmoled206::Power::kRequestPmuShutdownOnPowerOff;
}

bool shouldRequestShutdownOnPowerOff() {
  return WaveshareAmoled206::Power::kRequestPmuShutdownOnPowerOff;
}

bool shouldReleaseBatteryPowerBeforeDeepSleep() {
  return WaveshareAmoled206::Power::kReleaseBatteryHoldBeforeDeepSleep;
}

bool usesRecoverableSoftOff() { return WaveshareAmoled206::Power::kUsesRecoverableSoftOff; }

bool softOffWakeUsesPowerButton() {
  return WaveshareAmoled206::Power::kSoftOffWakeUsesPowerButton;
}

bool softOffWakeUsesBootButton() { return WaveshareAmoled206::Power::kSoftOffWakeUsesBootButton; }

uint32_t softOffWakeConfirmMs() { return WaveshareAmoled206::Power::kSoftOffWakeConfirmMs; }

}  // namespace Board::Power
