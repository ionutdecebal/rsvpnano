#include "board/BoardPower.h"

#include <Wire.h>
#include <algorithm>

#include "drivers/power/BatteryCurve.h"
#include "drivers/gpio/Tca9554.h"

namespace {

struct PowerContext {
  bool batteryPowerHoldEnabled = false;
};

PowerContext gPower;

bool configureOutputPin(uint8_t pin, bool high) {
  return BoardDrivers::Tca9554::configureOutputPin(
      Wire1, static_cast<uint8_t>(Board::Config::TCA9554_ADDRESS), pin, high,
      Board::Config::TCA9554_RELEASE_BUS_BEFORE_READ);
}

}  // namespace

namespace Board::Power {

void begin() {
  if (Config::PIN_BATTERY_ADC >= 0) {
    pinMode(Config::PIN_BATTERY_ADC, INPUT);
    analogReadResolution(12);
    analogSetPinAttenuation(Config::PIN_BATTERY_ADC, ADC_11db);
  }

  if (!gPower.batteryPowerHoldEnabled &&
      configureOutputPin(Config::TCA9554_PIN_SYS_EN, true)) {
    gPower.batteryPowerHoldEnabled = true;
    Serial.println("[board] Battery power hold enabled");
  }
}

void prepareDeepSleepPowerHold() {}

void resetWakePeripherals() {}

bool enableAudioPowerIfAvailable() {
  return configureOutputPin(Config::TCA9554_PIN_AUDIO_ENABLE, true);
}

bool readBatteryStatus(BatteryStatus &status) {
  status = BatteryStatus{};
  if (Config::PIN_BATTERY_ADC < 0) {
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
    const uint32_t sample = analogReadMilliVolts(Config::PIN_BATTERY_ADC);
    if (i >= 2 && sample > 0 && samples < kMaxSamples) {
      millivolts[samples++] = sample;
    }
    delayMicroseconds(500);
  }

  if (samples == 0) {
    uint32_t rawTotal = 0;
    for (uint8_t i = 0; i < kRawSamples; ++i) {
      rawTotal += analogRead(Config::PIN_BATTERY_ADC);
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
  if (!configureOutputPin(Config::TCA9554_PIN_SYS_EN, false)) {
    Serial.println("[board] Battery power hold release failed");
    return false;
  }

  gPower.batteryPowerHoldEnabled = false;
  Serial.println("[board] Battery power hold released");
  return true;
}

bool powerOffUsesControllerWake() { return false; }

bool shouldRequestShutdownOnPowerOff() { return Config::REQUEST_PMU_SHUTDOWN_ON_POWEROFF; }

bool shouldReleaseBatteryPowerBeforeDeepSleep() {
  return Config::RELEASE_BATTERY_HOLD_BEFORE_DEEP_SLEEP;
}

}  // namespace Board::Power
