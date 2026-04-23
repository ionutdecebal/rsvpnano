#include "board/BoardConfig.h"

#include <Wire.h>
#include <algorithm>
#include <driver/gpio.h>
#include <esp_sleep.h>

namespace BoardConfig {

namespace {

constexpr uint8_t kTca9554OutputReg = 0x01;
constexpr uint8_t kTca9554ConfigReg = 0x03;
bool gBatteryPowerHoldEnabled = false;
bool gBatteryAdcPathEnabled = false;

bool tca9554Read(uint8_t reg, uint8_t &value) {
  Wire1.beginTransmission(TCA9554_ADDRESS);
  Wire1.write(reg);
  if (Wire1.endTransmission(false) != 0) {
    return false;
  }

  if (Wire1.requestFrom(static_cast<uint8_t>(TCA9554_ADDRESS), static_cast<uint8_t>(1)) != 1) {
    return false;
  }

  value = Wire1.read();
  return true;
}

bool tca9554Write(uint8_t reg, uint8_t value) {
  Wire1.beginTransmission(TCA9554_ADDRESS);
  Wire1.write(reg);
  Wire1.write(value);
  return Wire1.endTransmission(true) == 0;
}

bool configureTca9554OutputPin(uint8_t pin, bool high) {
  uint8_t output = 0;
  if (!tca9554Read(kTca9554OutputReg, output)) {
    return false;
  }

  const uint8_t mask = static_cast<uint8_t>(1U << pin);
  if (high) {
    output |= mask;
  } else {
    output &= static_cast<uint8_t>(~mask);
  }
  if (!tca9554Write(kTca9554OutputReg, output)) {
    return false;
  }

  uint8_t config = 0xFF;
  if (!tca9554Read(kTca9554ConfigReg, config)) {
    return false;
  }

  config &= static_cast<uint8_t>(~mask);
  return tca9554Write(kTca9554ConfigReg, config);
}

void holdBatteryPowerIfAvailable() {
  if (gBatteryPowerHoldEnabled) {
    return;
  }

  if (!configureTca9554OutputPin(TCA9554_PIN_SYS_EN, true)) {
    Serial.println("[board] TCA9554 not detected; battery power hold not configured");
    return;
  }

  gBatteryPowerHoldEnabled = true;
  Serial.println("[board] Battery power hold enabled");
}

void enableBatteryAdcPathIfAvailable() {
  if (gBatteryAdcPathEnabled) {
    return;
  }

  if (!configureTca9554OutputPin(TCA9554_PIN_BATTERY_ADC_ENABLE, false)) {
    Serial.println("[board] TCA9554 battery ADC gate not configured");
    return;
  }

  gBatteryAdcPathEnabled = true;
  Serial.println("[board] Battery ADC path enabled");
}

void disableBatteryAdcPathIfAvailable() {
  // Keep the battery divider gate off outside short samples; it shares the board expander.
  if (!configureTca9554OutputPin(TCA9554_PIN_BATTERY_ADC_ENABLE, true)) {
    if (gBatteryAdcPathEnabled) {
      Serial.println("[board] TCA9554 battery ADC gate disable failed");
    }
    return;
  }

  gBatteryAdcPathEnabled = false;
}

uint8_t batteryPercentForVoltage(float voltage) {
  struct Point {
    float voltage;
    uint8_t percent;
  };

  constexpr Point kCurve[] = {
      {3.30f, 0},  {3.50f, 5},  {3.60f, 10}, {3.70f, 20},
      {3.75f, 30}, {3.80f, 40}, {3.85f, 50}, {3.90f, 60},
      {3.95f, 70}, {4.00f, 80}, {4.10f, 90}, {4.20f, 100},
  };

  if (voltage <= kCurve[0].voltage) {
    return kCurve[0].percent;
  }
  constexpr size_t curveSize = sizeof(kCurve) / sizeof(kCurve[0]);
  if (voltage >= kCurve[curveSize - 1].voltage) {
    return kCurve[curveSize - 1].percent;
  }

  for (size_t i = 1; i < curveSize; ++i) {
    const Point &upper = kCurve[i];
    const Point &lower = kCurve[i - 1];
    if (voltage > upper.voltage) {
      continue;
    }

    const float span = upper.voltage - lower.voltage;
    const float ratio = span <= 0.0f ? 0.0f : (voltage - lower.voltage) / span;
    const int percent =
        static_cast<int>(lower.percent + (upper.percent - lower.percent) * ratio + 0.5f);
    return static_cast<uint8_t>(std::max(0, std::min(100, percent)));
  }

  return 0;
}

}  // namespace

void begin() {
  pinMode(PIN_BOOT_BUTTON, INPUT_PULLUP);
  pinMode(PIN_PWR_BUTTON, INPUT_PULLUP);
  pinMode(PIN_LCD_BACKLIGHT, OUTPUT);
  digitalWrite(PIN_LCD_BACKLIGHT, LOW);

  Wire.begin(PIN_TOUCH_SDA, PIN_TOUCH_SCL);
  Wire.setClock(300000);
  Wire.setTimeOut(10);

  Wire1.begin(PIN_I2C_SDA, PIN_I2C_SCL);
  Wire1.setClock(300000);
  Wire1.setTimeOut(10);
  holdBatteryPowerIfAvailable();
  disableBatteryAdcPathIfAvailable();

  pinMode(PIN_BATTERY_ADC, INPUT);
  analogReadResolution(12);
  analogSetPinAttenuation(PIN_BATTERY_ADC, ADC_11db);
}

void lightSleepUntilBootButton() {
  pinMode(PIN_BOOT_BUTTON, INPUT_PULLUP);
  gpio_wakeup_enable(static_cast<gpio_num_t>(PIN_BOOT_BUTTON), GPIO_INTR_LOW_LEVEL);
  esp_sleep_enable_gpio_wakeup();
  Serial.flush();
  esp_light_sleep_start();
  gpio_wakeup_disable(static_cast<gpio_num_t>(PIN_BOOT_BUTTON));
  esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_GPIO);
}

bool readBatteryStatus(BatteryStatus &status) {
  status = BatteryStatus{};
  enableBatteryAdcPathIfAvailable();
  delay(3);

  uint32_t millivoltsTotal = 0;
  uint8_t samples = 0;
  for (uint8_t i = 0; i < 8; ++i) {
    const uint32_t sample = analogReadMilliVolts(PIN_BATTERY_ADC);
    if (sample > 0) {
      millivoltsTotal += sample;
      ++samples;
    }
    delayMicroseconds(250);
  }

  if (samples == 0) {
    uint32_t rawTotal = 0;
    for (uint8_t i = 0; i < 8; ++i) {
      rawTotal += analogRead(PIN_BATTERY_ADC);
      delayMicroseconds(250);
    }
    const float pinMillivolts = (static_cast<float>(rawTotal) / 8.0f) * 3300.0f / 4095.0f;
    status.voltage = pinMillivolts * 0.003f;
  } else {
    const float pinMillivolts = static_cast<float>(millivoltsTotal) / samples;
    status.voltage = pinMillivolts * 0.003f;
  }
  disableBatteryAdcPathIfAvailable();

  status.present = status.voltage >= 2.5f && status.voltage <= 4.6f;
  if (!status.present) {
    status.percent = 0;
    return false;
  }

  status.percent = batteryPercentForVoltage(status.voltage);
  return true;
}

bool releaseBatteryPowerHold() {
  if (!configureTca9554OutputPin(TCA9554_PIN_SYS_EN, false)) {
    Serial.println("[board] Battery power hold release failed");
    return false;
  }

  gBatteryPowerHoldEnabled = false;
  Serial.println("[board] Battery power hold released");
  return true;
}

}  // namespace BoardConfig
