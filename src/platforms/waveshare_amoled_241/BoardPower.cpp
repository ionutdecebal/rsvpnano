#include "board/BoardPower.h"

#include <driver/gpio.h>

#include "drivers/power/BatteryCurve.h"

namespace {

struct PowerContext {
  bool batteryPowerHoldEnabled = false;
};

PowerContext gPower;

}  // namespace

namespace Board::Power {

void begin() {
  if (Config::PIN_BATTERY_HOLD >= 0) {
    const gpio_num_t batteryHoldPin = static_cast<gpio_num_t>(Config::PIN_BATTERY_HOLD);
    gpio_deep_sleep_hold_dis();
    gpio_hold_dis(batteryHoldPin);
    pinMode(Config::PIN_BATTERY_HOLD, OUTPUT);
    digitalWrite(Config::PIN_BATTERY_HOLD, HIGH);
    gPower.batteryPowerHoldEnabled = true;
  }

  if (Config::PIN_BATTERY_ADC >= 0) {
    pinMode(Config::PIN_BATTERY_ADC, INPUT);
    analogReadResolution(12);
    analogSetPinAttenuation(Config::PIN_BATTERY_ADC, ADC_11db);
  }
}

void prepareDeepSleepPowerHold() {
  if (Config::PIN_BATTERY_HOLD < 0) {
    return;
  }

  const gpio_num_t batteryHoldPin = static_cast<gpio_num_t>(Config::PIN_BATTERY_HOLD);
  pinMode(Config::PIN_BATTERY_HOLD, OUTPUT);
  digitalWrite(Config::PIN_BATTERY_HOLD, HIGH);
  gpio_set_direction(batteryHoldPin, GPIO_MODE_OUTPUT);
  gpio_set_level(batteryHoldPin, 1);
  gpio_hold_en(batteryHoldPin);
  gpio_deep_sleep_hold_en();
}

void resetWakePeripherals() {}

bool enableAudioPowerIfAvailable() { return false; }

bool readBatteryStatus(BatteryStatus &status) {
  status = BatteryStatus{};
  if (Config::PIN_BATTERY_ADC < 0) {
    return false;
  }

  constexpr float kBatteryDividerScale = 0.003f;
  uint32_t millivoltsTotal = 0;
  uint8_t samples = 0;
  for (uint8_t i = 0; i < 8; ++i) {
    const uint32_t sample = analogReadMilliVolts(Config::PIN_BATTERY_ADC);
    if (sample > 0) {
      millivoltsTotal += sample;
      ++samples;
    }
    delayMicroseconds(250);
  }

  if (samples == 0) {
    uint32_t rawTotal = 0;
    for (uint8_t i = 0; i < 8; ++i) {
      rawTotal += analogRead(Config::PIN_BATTERY_ADC);
      delayMicroseconds(250);
    }
    const float pinMillivolts = (static_cast<float>(rawTotal) / 8.0f) * 3300.0f / 4095.0f;
    status.voltage = pinMillivolts * kBatteryDividerScale;
  } else {
    const float pinMillivolts = static_cast<float>(millivoltsTotal) / samples;
    status.voltage = pinMillivolts * kBatteryDividerScale;
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
  if (Config::PIN_BATTERY_HOLD < 0) {
    return false;
  }

  digitalWrite(Config::PIN_BATTERY_HOLD, LOW);
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
