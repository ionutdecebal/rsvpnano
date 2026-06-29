#include "board/BoardPower.h"

#include <Wire.h>
#include <algorithm>

#include "drivers/gpio/tca9554/Tca9554.h"
#include "drivers/power/BatteryCurve.h"
#include "platforms/waveshare_lcd_349/WaveshareLcd349.h"

namespace {

struct PowerContext {
  bool batteryPowerHoldEnabled = false;
};

PowerContext gPower;

}  // namespace

namespace Board::Power {

void begin() {
  if constexpr (WaveshareLcd349::Power::kBatteryAdcPin >= 0) {
    pinMode(WaveshareLcd349::Power::kBatteryAdcPin, INPUT);
    analogReadResolution(12);
    analogSetPinAttenuation(WaveshareLcd349::Power::kBatteryAdcPin, ADC_11db);
  }

  if (!gPower.batteryPowerHoldEnabled &&
      BoardDrivers::Tca9554::configureOutputPin(
          Wire1, WaveshareLcd349::Tca9554Wiring::kAddress,
          WaveshareLcd349::Tca9554Wiring::kSysEnablePin, true,
          WaveshareLcd349::Tca9554Wiring::kReleaseBusBeforeRead)) {
    gPower.batteryPowerHoldEnabled = true;
    Serial.println("[board] Battery power hold enabled");
  }
}

void prepareDeepSleepPowerHold() {}

bool enableAudioPowerIfAvailable() {
  return BoardDrivers::Tca9554::configureOutputPin(
      Wire1, WaveshareLcd349::Tca9554Wiring::kAddress,
      WaveshareLcd349::Tca9554Wiring::kAudioEnablePin, true,
      WaveshareLcd349::Tca9554Wiring::kReleaseBusBeforeRead);
}

bool readBatteryStatus(BatteryStatus &status) {
  status = BatteryStatus{};
  if constexpr (WaveshareLcd349::Power::kBatteryAdcPin < 0) {
    return false;
  }

  constexpr uint8_t kMaxSamples = 24;
  constexpr uint8_t kRawSamples = 16;
  constexpr float kBatteryDividerRatio = 3.0f;
  constexpr float kBatteryVoltageOffset = 0.0f;

  delay(12);
  uint32_t millivolts[kMaxSamples];
  uint8_t samples = 0;
  for (uint8_t i = 0; i < kMaxSamples + 2; ++i) {
    const uint32_t sample = analogReadMilliVolts(WaveshareLcd349::Power::kBatteryAdcPin);
    if (i >= 2 && sample > 0 && samples < kMaxSamples) {
      millivolts[samples++] = sample;
    }
    delayMicroseconds(500);
  }

  if (samples == 0) {
    uint32_t rawTotal = 0;
    for (uint8_t i = 0; i < kRawSamples; ++i) {
      rawTotal += analogRead(WaveshareLcd349::Power::kBatteryAdcPin);
      delayMicroseconds(500);
    }
    const float pinMillivolts =
        (static_cast<float>(rawTotal) / static_cast<float>(kRawSamples)) * 3300.0f / 4095.0f;
    status.voltage = (pinMillivolts * kBatteryDividerRatio / 1000.0f) + kBatteryVoltageOffset;
  } else {
    std::sort(millivolts, millivolts + samples);
    const uint8_t trim = samples >= 10 ? 2 : 0;
    uint32_t trimmedTotal = 0;
    uint8_t trimmedSamples = 0;
    for (uint8_t i = trim; i < samples - trim; ++i) {
      trimmedTotal += millivolts[i];
      ++trimmedSamples;
    }
    const float pinMillivolts =
        static_cast<float>(trimmedTotal) / static_cast<float>(std::max<uint8_t>(1, trimmedSamples));
    status.voltage = (pinMillivolts * kBatteryDividerRatio / 1000.0f) + kBatteryVoltageOffset;
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

bool releaseBatteryPowerHold() {
  if (!BoardDrivers::Tca9554::configureOutputPin(
          Wire1, WaveshareLcd349::Tca9554Wiring::kAddress,
          WaveshareLcd349::Tca9554Wiring::kSysEnablePin, false,
          WaveshareLcd349::Tca9554Wiring::kReleaseBusBeforeRead)) {
    Serial.println("[board] Battery power hold release failed");
    return false;
  }

  gPower.batteryPowerHoldEnabled = false;
  Serial.println("[board] Battery power hold released");
  return true;
}

bool supportsSoftwarePowerOff() { return true; }

bool powerOffUsesControllerWake() { return false; }

bool shouldRequestShutdownOnPowerOff() {
  return WaveshareLcd349::Power::kRequestPmuShutdownOnPowerOff;
}

bool shouldReleaseBatteryPowerBeforeDeepSleep() {
  return WaveshareLcd349::Power::kReleaseBatteryHoldBeforeDeepSleep;
}

}  // namespace Board::Power
