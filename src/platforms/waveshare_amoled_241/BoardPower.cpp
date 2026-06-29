#include "board/BoardPower.h"

#include <driver/gpio.h>

#include "drivers/power/BatteryCurve.h"
#include "platforms/waveshare_amoled_241/WaveshareAmoled241.h"

namespace {

struct PowerContext {
  bool batteryPowerHoldEnabled = false;
};

PowerContext gPower;

}  // namespace

namespace Board::Power {

void begin() {
  if constexpr (WaveshareAmoled241::Power::kBatteryHoldPin >= 0) {
    constexpr gpio_num_t batteryHoldGpio = WaveshareAmoled241::Power::kBatteryHoldGpio;
    gpio_deep_sleep_hold_dis();
    gpio_hold_dis(batteryHoldGpio);
    pinMode(WaveshareAmoled241::Power::kBatteryHoldPin, OUTPUT);
    digitalWrite(WaveshareAmoled241::Power::kBatteryHoldPin, HIGH);
    gPower.batteryPowerHoldEnabled = true;
  }

  if constexpr (WaveshareAmoled241::Power::kBatteryAdcPin >= 0) {
    pinMode(WaveshareAmoled241::Power::kBatteryAdcPin, INPUT);
    analogReadResolution(12);
    analogSetPinAttenuation(WaveshareAmoled241::Power::kBatteryAdcPin, ADC_11db);
  }
}

void prepareDeepSleepPowerHold() {
  if constexpr (WaveshareAmoled241::Power::kBatteryHoldPin < 0) {
    return;
  }

  constexpr gpio_num_t batteryHoldGpio = WaveshareAmoled241::Power::kBatteryHoldGpio;
  pinMode(WaveshareAmoled241::Power::kBatteryHoldPin, OUTPUT);
  digitalWrite(WaveshareAmoled241::Power::kBatteryHoldPin, HIGH);
  gpio_set_direction(batteryHoldGpio, GPIO_MODE_OUTPUT);
  gpio_set_level(batteryHoldGpio, 1);
  gpio_hold_en(batteryHoldGpio);
  gpio_deep_sleep_hold_en();
}

bool enableAudioPowerIfAvailable() { return false; }

bool readBatteryStatus(BatteryStatus &status) {
  status = BatteryStatus{};
  if constexpr (WaveshareAmoled241::Power::kBatteryAdcPin < 0) {
    return false;
  }

  constexpr float kBatteryDividerScale = 0.003f;
  uint32_t millivoltsTotal = 0;
  uint8_t samples = 0;
  for (uint8_t i = 0; i < 8; ++i) {
    const uint32_t sample = analogReadMilliVolts(WaveshareAmoled241::Power::kBatteryAdcPin);
    if (sample > 0) {
      millivoltsTotal += sample;
      ++samples;
    }
    delayMicroseconds(250);
  }

  if (samples == 0) {
    uint32_t rawTotal = 0;
    for (uint8_t i = 0; i < 8; ++i) {
      rawTotal += analogRead(WaveshareAmoled241::Power::kBatteryAdcPin);
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
  if constexpr (WaveshareAmoled241::Power::kBatteryHoldPin < 0) {
    return false;
  }

  digitalWrite(WaveshareAmoled241::Power::kBatteryHoldPin, LOW);
  gPower.batteryPowerHoldEnabled = false;
  Serial.println("[board] Battery power hold released");
  return true;
}

bool supportsSoftwarePowerOff() { return true; }

bool powerOffUsesControllerWake() { return false; }

bool shouldRequestShutdownOnPowerOff() {
  return WaveshareAmoled241::Power::kRequestPmuShutdownOnPowerOff;
}

bool shouldReleaseBatteryPowerBeforeDeepSleep() {
  return WaveshareAmoled241::Power::kReleaseBatteryHoldBeforeDeepSleep;
}

}  // namespace Board::Power
