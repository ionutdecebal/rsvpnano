#include "board/BoardPower.h"

#include "drivers/power/BatteryCurve.h"
#include "platforms/waveshare_c6_touch_lcd_147/WaveshareC6TouchLcd147.h"

namespace Board::Power {

void begin() {
  if constexpr (WaveshareC6TouchLcd147::Power::kBatteryAdcPin >= 0) {
    pinMode(WaveshareC6TouchLcd147::Power::kBatteryAdcPin, INPUT);
    analogReadResolution(12);
    analogSetPinAttenuation(WaveshareC6TouchLcd147::Power::kBatteryAdcPin, ADC_11db);
  }
}

void prepareDeepSleepPowerHold() {}

void resetWakePeripherals() {}

bool enableAudioPowerIfAvailable() { return false; }

bool readBatteryStatus(BatteryStatus &status) {
  status = BatteryStatus{};
  if constexpr (WaveshareC6TouchLcd147::Power::kBatteryAdcPin < 0) {
    return false;
  }

  uint32_t millivoltsTotal = 0;
  uint8_t samples = 0;
  for (uint8_t i = 0; i < 8; ++i) {
    const uint32_t sample = analogReadMilliVolts(WaveshareC6TouchLcd147::Power::kBatteryAdcPin);
    if (sample > 0) {
      millivoltsTotal += sample;
      ++samples;
    }
    delayMicroseconds(250);
  }

  if (samples == 0) {
    uint32_t rawTotal = 0;
    for (uint8_t i = 0; i < 8; ++i) {
      rawTotal += analogRead(WaveshareC6TouchLcd147::Power::kBatteryAdcPin);
      delayMicroseconds(250);
    }
    const float pinMillivolts = (static_cast<float>(rawTotal) / 8.0f) * 3300.0f / 4095.0f;
    status.voltage = pinMillivolts * WaveshareC6TouchLcd147::Power::kBatteryDividerScale;
  } else {
    const float pinMillivolts = static_cast<float>(millivoltsTotal) / samples;
    status.voltage = pinMillivolts * WaveshareC6TouchLcd147::Power::kBatteryDividerScale;
  }

  status.present = status.voltage >= 2.5f && status.voltage <= 4.6f;
  if (!status.present) {
    status.percent = 0;
    return false;
  }

  status.percent = BoardDrivers::BatteryCurve::percentForVoltage(status.voltage);
  return true;
}

DiagnosticSnapshot diagnosticSnapshot() { return PowerDiagnosticSnapshot{}; }

bool externalPowerPresent() { return false; }

bool releaseBatteryPowerHold() { return false; }

bool supportsSoftwarePowerOff() { return false; }

bool powerOffUsesControllerWake() { return false; }

bool shouldRequestShutdownOnPowerOff() {
  return WaveshareC6TouchLcd147::Power::kRequestPmuShutdownOnPowerOff;
}

bool shouldReleaseBatteryPowerBeforeDeepSleep() {
  return WaveshareC6TouchLcd147::Power::kReleaseBatteryHoldBeforeDeepSleep;
}

bool usesRecoverableSoftOff() { return WaveshareC6TouchLcd147::Power::kUsesRecoverableSoftOff; }

bool softOffWakeUsesPowerButton() {
  return WaveshareC6TouchLcd147::Power::kSoftOffWakeUsesPowerButton;
}

bool softOffWakeUsesBootButton() { return WaveshareC6TouchLcd147::Power::kSoftOffWakeUsesBootButton; }

uint32_t softOffWakeConfirmMs() { return WaveshareC6TouchLcd147::Power::kSoftOffWakeConfirmMs; }

}  // namespace Board::Power
