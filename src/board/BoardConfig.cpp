#include "board/BoardConfig.h"

#include <Wire.h>
#include <algorithm>
#include <driver/gpio.h>
#include <esp_sleep.h>

namespace BoardConfig {

namespace {

uint8_t batteryPercentForVoltage(float voltage) {
  struct Point {
    float voltage;
    uint8_t percent;
  };

  constexpr Point kCurve[] = {
      {3.30f, 0},  {3.50f, 5},  {3.60f, 10}, {3.65f, 20},
      {3.70f, 30}, {3.75f, 40}, {3.79f, 50}, {3.85f, 60},
      {3.92f, 70}, {4.00f, 80}, {4.10f, 90}, {4.20f, 100},
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

void pulseDirectTouchResetPin(uint32_t lowDelayMs, uint32_t highDelayMs) {
  if (PIN_TOUCH_RST < 0) {
    return;
  }

  pinMode(PIN_TOUCH_RST, OUTPUT);
  digitalWrite(PIN_TOUCH_RST, LOW);
  delay(lowDelayMs);
  digitalWrite(PIN_TOUCH_RST, HIGH);
  delay(highDelayMs);
}

#if defined(RSVP_BOARD_WAVESHARE_ESP32S3_TOUCH_AMOLED_216) || \
    defined(RSVP_BOARD_WAVESHARE_ESP32S3_TOUCH_AMOLED_18)

constexpr uint8_t kAxp2101Address = 0x34;
constexpr uint8_t kAxp2101Status1Reg = 0x00;
constexpr uint8_t kAxp2101Status2Reg = 0x01;
constexpr uint8_t kAxp2101CommonConfigReg = 0x10;
constexpr uint8_t kAxp2101AdcChannelCtrlReg = 0x30;
constexpr uint8_t kAxp2101PowerOffEnableReg = 0x22;
constexpr uint8_t kAxp2101IrqOffOnLevelCtrlReg = 0x27;
constexpr uint8_t kAxp2101IrqEnable2Reg = 0x41;
constexpr uint8_t kAxp2101IrqStatus2Reg = 0x49;
constexpr uint8_t kAxp2101BatteryVoltageHighReg = 0x34;
constexpr uint8_t kAxp2101BatteryVoltageLowReg = 0x35;
constexpr uint8_t kAxp2101BatteryDetectCtrlReg = 0x68;
constexpr uint8_t kAxp2101BatteryPercentReg = 0xA4;
constexpr uint8_t kAxp2101PowerKeyIrqMask = 0x0F;
constexpr uint8_t kAxp2101PowerKeyPositiveIrqMask = 0x01;
constexpr uint8_t kAxp2101PowerKeyNegativeIrqMask = 0x02;
constexpr uint8_t kAxp2101PowerKeyLongIrqMask = 0x04;
constexpr uint8_t kAxp2101PowerKeyShortIrqMask = 0x08;
constexpr uint8_t kAxp2101LongPressShutdownMask = 0x02;
constexpr uint8_t kAxp2101LongPressRestartMask = 0x01;
constexpr uint8_t kAxp2101PowerKeyTimingMask = 0x0F;
constexpr uint8_t kAxp2101PowerKeyIrqLevelMask = 0x30;
constexpr uint32_t kAxp2101PowerKeyPollIntervalMs = 20;
bool gAxp2101Ready = false;
bool gAxp2101PowerButtonHeld = false;
bool gAxp2101PowerButtonShortPressPending = false;
bool gAxp2101PowerButtonLongPressPending = false;
uint32_t gAxp2101LastPowerKeyPollMs = 0;
PowerDiagnosticSnapshot gPowerDiagnosticSnapshot;

#if defined(RSVP_BOARD_WAVESHARE_ESP32S3_TOUCH_AMOLED_18)
constexpr uint8_t kTca9554InputReg = 0x00;
constexpr uint8_t kTca9554OutputReg = 0x01;
constexpr uint8_t kTca9554ConfigReg = 0x03;
constexpr uint32_t kExpanderPowerButtonPollIntervalMs = 25;
constexpr uint32_t kExpanderPowerButtonDebounceMs = 70;
bool gTca9554Sequenced = false;
bool gExpanderPowerButtonStableHeld = false;
bool gExpanderPowerButtonRawHeld = false;
uint32_t gExpanderPowerButtonRawChangedMs = 0;
uint32_t gExpanderPowerButtonLastPollMs = 0;
bool gExpanderPowerButtonSeen = false;

bool tca9554Read(uint8_t reg, uint8_t &value) {
  Wire.beginTransmission(TCA9554_ADDRESS);
  Wire.write(reg);
  if (Wire.endTransmission(true) != 0) {
    return false;
  }
  delayMicroseconds(50);

  if (Wire.requestFrom(static_cast<uint8_t>(TCA9554_ADDRESS), static_cast<uint8_t>(1)) != 1) {
    return false;
  }

  value = Wire.read();
  return true;
}

bool tca9554Write(uint8_t reg, uint8_t value) {
  Wire.beginTransmission(TCA9554_ADDRESS);
  Wire.write(reg);
  Wire.write(value);
  return Wire.endTransmission(true) == 0;
}

bool readExpanderPin(uint8_t pin, bool &high) {
  uint8_t input = 0;
  if (!tca9554Read(kTca9554InputReg, input)) {
    return false;
  }

  high = (input & static_cast<uint8_t>(1U << pin)) != 0;
  return true;
}

bool configureTca9554OutputPin(uint8_t pin, bool high) {
  uint8_t output = 0xFF;
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

void configureIoExpanderIfAvailable(bool forceDisplaySequence = false) {
  if (TCA9554_ADDRESS < 0) {
    return;
  }

  uint8_t output = 0xFF;
  uint8_t config = 0xFF;
  if (!tca9554Read(kTca9554OutputReg, output) || !tca9554Read(kTca9554ConfigReg, config)) {
    Serial.println("[board] TCA9554 not detected");
    return;
  }

  const uint8_t displayMask =
      static_cast<uint8_t>((1U << TCA9554_PIN_TOUCH_RESET) | (1U << TCA9554_PIN_LCD_RESET) |
                           (1U << TCA9554_PIN_DISPLAY_ENABLE));
  const uint8_t outputMask =
      static_cast<uint8_t>(displayMask | (1U << TCA9554_PIN_SD_ENABLE));
  const bool runDisplaySequence = forceDisplaySequence || !gTca9554Sequenced;

  if (runDisplaySequence) {
    output &= static_cast<uint8_t>(~displayMask);
  } else {
    output |= displayMask;
  }
  output |= static_cast<uint8_t>(1U << TCA9554_PIN_SD_ENABLE);
  config &= static_cast<uint8_t>(~outputMask);
  config |= static_cast<uint8_t>((1U << TCA9554_PIN_PWR_BUTTON) |
                                 (1U << TCA9554_PIN_PMU_IRQ));

  if (!tca9554Write(kTca9554OutputReg, output) || !tca9554Write(kTca9554ConfigReg, config)) {
    Serial.println("[board] TCA9554 output setup failed");
    return;
  }
  if (!runDisplaySequence) {
    return;
  }

  // Pulse the expander-controlled rails and resets exactly the way Waveshare's demos do:
  // drive low briefly, then release high so the display and touch controller both come up.
  if (forceDisplaySequence) {
    Serial.println("[board] TCA9554 display/touch wake sequence");
  }
  delay(20);
  output |= displayMask;
  if (!tca9554Write(kTca9554OutputReg, output)) {
    Serial.println("[board] TCA9554 display release failed");
    return;
  }
  delay(50);
  gTca9554Sequenced = true;
}
#endif

bool axp2101ReadRegister(uint8_t reg, uint8_t &value) {
  Wire.beginTransmission(kAxp2101Address);
  Wire.write(reg);
#if defined(RSVP_BOARD_WAVESHARE_ESP32S3_TOUCH_AMOLED_18)
  constexpr bool releaseBusBeforeRead = true;
#else
  constexpr bool releaseBusBeforeRead = false;
#endif
  if (Wire.endTransmission(releaseBusBeforeRead) != 0) {
    return false;
  }
  if (releaseBusBeforeRead) {
    delayMicroseconds(50);
  }

  if (Wire.requestFrom(static_cast<uint8_t>(kAxp2101Address), static_cast<uint8_t>(1)) != 1) {
    return false;
  }

  value = Wire.read();
  return true;
}

bool axp2101WriteRegister(uint8_t reg, uint8_t value) {
  Wire.beginTransmission(kAxp2101Address);
  Wire.write(reg);
  Wire.write(value);
  return Wire.endTransmission(true) == 0;
}

bool axp2101UpdateRegisterBits(uint8_t reg, uint8_t mask, uint8_t value) {
  uint8_t current = 0;
  if (!axp2101ReadRegister(reg, current)) {
    return false;
  }

  current = static_cast<uint8_t>((current & static_cast<uint8_t>(~mask)) | (value & mask));
  return axp2101WriteRegister(reg, current);
}

uint16_t axp2101ReadH5L8(uint8_t highReg, uint8_t lowReg) {
  uint8_t high = 0;
  uint8_t low = 0;
  if (!axp2101ReadRegister(highReg, high) || !axp2101ReadRegister(lowReg, low)) {
    return 0;
  }

  return static_cast<uint16_t>(((high & 0x1F) << 8) | low);
}

bool axp2101BatteryConnected() {
  uint8_t status1 = 0;
  if (!axp2101ReadRegister(kAxp2101Status1Reg, status1)) {
    return false;
  }

  return (status1 & 0x08) != 0;
}

void captureAxp2101PowerDiagnostics(uint8_t status1) {
  if (gPowerDiagnosticSnapshot.available) {
    return;
  }

  PowerDiagnosticSnapshot snapshot;
  snapshot.available = true;
  snapshot.status1 = status1;

  uint8_t status2 = 0;
  if (axp2101ReadRegister(kAxp2101Status2Reg, status2)) {
    snapshot.status2 = status2;
    snapshot.externalPowerPresent = (status1 & 0x20) != 0 && (status2 & 0x08) == 0;
  }

  uint8_t irqStatus2 = 0;
  if (axp2101ReadRegister(kAxp2101IrqStatus2Reg, irqStatus2)) {
    snapshot.powerKeyIrqStatus = irqStatus2;
  }

  gPowerDiagnosticSnapshot = snapshot;
}

void configureTouchResetIfAvailable() {
  if (PIN_TOUCH_RST >= 0) {
    pulseDirectTouchResetPin(30, 50);
  } else {
    Serial.println("[board] touch reset pin unavailable; skipping hardware reset");
  }
}

bool configureAxp2101IfAvailable() {
  uint8_t value = 0;
  if (!axp2101ReadRegister(kAxp2101Status1Reg, value)) {
    if (gAxp2101Ready) {
      Serial.println("[board] AXP2101 no longer responding");
    }
    gAxp2101Ready = false;
    return false;
  }

  gAxp2101Ready = true;
  captureAxp2101PowerDiagnostics(value);
  if (!axp2101UpdateRegisterBits(kAxp2101AdcChannelCtrlReg, 0x01, 0x01)) {
    Serial.println("[board] AXP2101 battery voltage ADC enable failed");
  }
  if (!axp2101UpdateRegisterBits(kAxp2101BatteryDetectCtrlReg, 0x01, 0x01)) {
    Serial.println("[board] AXP2101 battery detect enable failed");
  }
#if !defined(RSVP_BOARD_WAVESHARE_ESP32S3_TOUCH_AMOLED_18)
  if (!axp2101UpdateRegisterBits(kAxp2101IrqEnable2Reg, kAxp2101PowerKeyIrqMask,
                                 kAxp2101PowerKeyIrqMask)) {
    Serial.println("[board] AXP2101 power-key IRQ enable failed");
  }
#else
  axp2101UpdateRegisterBits(kAxp2101IrqEnable2Reg, kAxp2101PowerKeyIrqMask, 0x00);
#endif
  if (PMU_REQUIRES_POWER_KEY_CONFIG) {
    const uint8_t powerKeyTiming =
        static_cast<uint8_t>((PMU_POWER_KEY_ON_TIME_VALUE & 0x03) |
                             ((PMU_POWER_KEY_OFF_TIME_VALUE & 0x03) << 2));
    if (!axp2101UpdateRegisterBits(kAxp2101IrqOffOnLevelCtrlReg, kAxp2101PowerKeyTimingMask,
                                   powerKeyTiming)) {
      Serial.println("[board] AXP2101 power-key timing config failed");
    }

    // Use long press as a hardware shutdown fallback and keep the PMU in shutdown mode rather
    // than restart mode. This makes software-triggered shutdowns and manual long-press power
    // cycles behave much closer to the original board.
    if (!axp2101UpdateRegisterBits(kAxp2101PowerOffEnableReg,
                                   static_cast<uint8_t>(kAxp2101LongPressShutdownMask |
                                                        kAxp2101LongPressRestartMask),
                                   kAxp2101LongPressShutdownMask)) {
      Serial.println("[board] AXP2101 long-press shutdown enable failed");
    }

    Serial.printf("[board] AXP2101 power key configured on=%u off=%u\n",
                  static_cast<unsigned>(PMU_POWER_KEY_ON_TIME_VALUE),
                  static_cast<unsigned>(PMU_POWER_KEY_OFF_TIME_VALUE));
  }

  gAxp2101PowerButtonHeld = false;
  gAxp2101PowerButtonShortPressPending = false;
  gAxp2101PowerButtonLongPressPending = false;
  gAxp2101LastPowerKeyPollMs = 0;
  axp2101WriteRegister(kAxp2101IrqStatus2Reg, 0xFF);
  return true;
}

void pollAxp2101PowerKeyIfDue(bool force = false) {
  const uint32_t nowMs = millis();
  if (!force && (nowMs - gAxp2101LastPowerKeyPollMs) < kAxp2101PowerKeyPollIntervalMs) {
    return;
  }
  gAxp2101LastPowerKeyPollMs = nowMs;

  if (!gAxp2101Ready && !configureAxp2101IfAvailable()) {
    return;
  }

  uint8_t status2 = 0;
  if (!axp2101ReadRegister(kAxp2101IrqStatus2Reg, status2)) {
    return;
  }

  if ((status2 & kAxp2101PowerKeyNegativeIrqMask) != 0) {
    gAxp2101PowerButtonHeld = true;
  }
  if ((status2 & kAxp2101PowerKeyPositiveIrqMask) != 0) {
    gAxp2101PowerButtonHeld = false;
  }
  if ((status2 & kAxp2101PowerKeyLongIrqMask) != 0) {
    gAxp2101PowerButtonLongPressPending = true;
  }
  if ((status2 & kAxp2101PowerKeyShortIrqMask) != 0) {
    gAxp2101PowerButtonShortPressPending = true;
  }
  if (status2 != 0) {
    axp2101WriteRegister(kAxp2101IrqStatus2Reg, status2);
  }
}

#elif defined(RSVP_BOARD_WAVESHARE_ESP32S3_TOUCH_AMOLED_241)

constexpr float kBatteryDividerScale = 0.003f;
bool gBatteryPowerHoldEnabled = false;

void configureBatteryHoldIfAvailable() {
  if (PIN_BATTERY_HOLD < 0) {
    return;
  }

  pinMode(PIN_BATTERY_HOLD, OUTPUT);
  digitalWrite(PIN_BATTERY_HOLD, HIGH);
  gBatteryPowerHoldEnabled = true;
}

void configureTouchResetIfAvailable() {
  pulseDirectTouchResetPin(12, 12);
}

void configureTouchWire() {
  if (TOUCH_USES_WIRE1) {
    Wire1.begin(PIN_TOUCH_SDA, PIN_TOUCH_SCL);
    Wire1.setClock(400000);
    Wire1.setTimeOut(10);
    return;
  }

  Wire.begin(PIN_TOUCH_SDA, PIN_TOUCH_SCL);
  Wire.setClock(400000);
  Wire.setTimeOut(10);
}

void configureSystemWire() {
  if (PIN_I2C_SDA < 0 || PIN_I2C_SCL < 0 || TOUCH_USES_WIRE1) {
    return;
  }

  Wire1.begin(PIN_I2C_SDA, PIN_I2C_SCL);
  Wire1.setClock(400000);
  Wire1.setTimeOut(10);
}

#elif defined(RSVP_BOARD_WAVESHARE_ESP32S3_TOUCH_LCD_349)

constexpr uint8_t kTca9554OutputReg = 0x01;
constexpr uint8_t kTca9554ConfigReg = 0x03;
bool gBatteryPowerHoldEnabled = false;
bool gBacklightEnableConfigured = false;
constexpr float kBatteryDividerRatio = 3.0f;
constexpr float kBatteryVoltageOffset = 0.0f;

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

void enableBacklightIfAvailable() {
  if (gBacklightEnableConfigured) {
    return;
  }

  if (!configureTca9554OutputPin(TCA9554_PIN_BACKLIGHT_ENABLE, true)) {
    Serial.println("[board] TCA9554 backlight enable not configured");
    return;
  }

  gBacklightEnableConfigured = true;
  Serial.println("[board] Backlight enable configured");
}

void configureTouchResetIfAvailable() { pulseDirectTouchResetPin(12, 12); }

#else
#error "Unsupported board target"
#endif

}  // namespace

void begin() {
  if (PIN_BOOT_BUTTON >= 0) {
    pinMode(PIN_BOOT_BUTTON, INPUT_PULLUP);
  }
  if (PIN_PWR_BUTTON >= 0) {
    pinMode(PIN_PWR_BUTTON, INPUT_PULLUP);
  }
  if (PIN_TOUCH_IRQ >= 0) {
    pinMode(PIN_TOUCH_IRQ, INPUT_PULLUP);
  }
  if (POWER_MANAGER == PowerManagerKind::DirectGpioBatteryHold && PIN_BATTERY_HOLD >= 0) {
    const gpio_num_t batteryHoldPin = static_cast<gpio_num_t>(PIN_BATTERY_HOLD);
    gpio_deep_sleep_hold_dis();
    gpio_hold_dis(batteryHoldPin);
  }
  if (HAS_LCD_BACKLIGHT && PIN_LCD_BACKLIGHT >= 0) {
    gpio_deep_sleep_hold_dis();
    gpio_hold_dis(static_cast<gpio_num_t>(PIN_LCD_BACKLIGHT));
    pinMode(PIN_LCD_BACKLIGHT, OUTPUT);
    digitalWrite(PIN_LCD_BACKLIGHT, LOW);
  }

#if defined(RSVP_BOARD_WAVESHARE_ESP32S3_TOUCH_AMOLED_216)
  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
  Wire.setClock(400000);
  Wire.setTimeOut(10);
  configureTouchResetIfAvailable();
  configureAxp2101IfAvailable();

#elif defined(RSVP_BOARD_WAVESHARE_ESP32S3_TOUCH_AMOLED_18)
  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
  Wire.setClock(200000);
  Wire.setTimeOut(20);
  configureIoExpanderIfAvailable();
  configureTouchResetIfAvailable();
  configureAxp2101IfAvailable();

#elif defined(RSVP_BOARD_WAVESHARE_ESP32S3_TOUCH_AMOLED_241)
  configureBatteryHoldIfAvailable();
  configureTouchWire();
  configureSystemWire();
  configureTouchResetIfAvailable();

  pinMode(PIN_BATTERY_ADC, INPUT);
  analogReadResolution(12);
  analogSetPinAttenuation(PIN_BATTERY_ADC, ADC_11db);

#elif defined(RSVP_BOARD_WAVESHARE_ESP32S3_TOUCH_LCD_349)
  Wire.begin(PIN_TOUCH_SDA, PIN_TOUCH_SCL);
  Wire.setClock(300000);
  Wire.setTimeOut(10);

  Wire1.begin(PIN_I2C_SDA, PIN_I2C_SCL);
  Wire1.setClock(300000);
  Wire1.setTimeOut(10);
  holdBatteryPowerIfAvailable();
  enableBacklightIfAvailable();

  pinMode(PIN_BATTERY_ADC, INPUT);
  analogReadResolution(12);
  analogSetPinAttenuation(PIN_BATTERY_ADC, ADC_11db);
#endif
}

void lightSleepUntilBootButton() {
  const int wakePin =
      PIN_DEEP_SLEEP_WAKE >= 0 ? PIN_DEEP_SLEEP_WAKE
                               : (PIN_BOOT_BUTTON >= 0 ? PIN_BOOT_BUTTON : PIN_PWR_BUTTON);
  if (wakePin < 0) {
    return;
  }

  pinMode(wakePin, INPUT_PULLUP);
  gpio_wakeup_enable(static_cast<gpio_num_t>(wakePin), GPIO_INTR_LOW_LEVEL);
  esp_sleep_enable_gpio_wakeup();
  Serial.flush();
  esp_light_sleep_start();
  gpio_wakeup_disable(static_cast<gpio_num_t>(wakePin));
  esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_GPIO);
}

void holdBacklightOffForDeepSleep() {
  if (POWER_MANAGER == PowerManagerKind::DirectGpioBatteryHold && PIN_BATTERY_HOLD >= 0) {
    const gpio_num_t batteryHoldPin = static_cast<gpio_num_t>(PIN_BATTERY_HOLD);
    pinMode(PIN_BATTERY_HOLD, OUTPUT);
    digitalWrite(PIN_BATTERY_HOLD, HIGH);
    gpio_set_direction(batteryHoldPin, GPIO_MODE_OUTPUT);
    gpio_set_level(batteryHoldPin, 1);
    gpio_hold_en(batteryHoldPin);
    gpio_deep_sleep_hold_en();
  }

  if (!HAS_LCD_BACKLIGHT || PIN_LCD_BACKLIGHT < 0) {
    return;
  }

  const gpio_num_t backlightPin = static_cast<gpio_num_t>(PIN_LCD_BACKLIGHT);

  // The LCD backlight is active-low. Hold the inactive level while the ESP32 is in deep sleep,
  // because PWM output stops there and can otherwise leave the backlight pin floating.
  analogWrite(PIN_LCD_BACKLIGHT, 255);
  pinMode(PIN_LCD_BACKLIGHT, OUTPUT);
  digitalWrite(PIN_LCD_BACKLIGHT, HIGH);
  gpio_set_direction(backlightPin, GPIO_MODE_OUTPUT);
  gpio_set_level(backlightPin, 1);
  gpio_hold_en(backlightPin);
  gpio_deep_sleep_hold_en();
}

void resetWakePeripherals() {
#if defined(RSVP_BOARD_WAVESHARE_ESP32S3_TOUCH_AMOLED_18)
  configureIoExpanderIfAvailable(true);
#endif
}

void resetTouchController() { configureTouchResetIfAvailable(); }

bool readBatteryStatus(BatteryStatus &status) {
  status = BatteryStatus{};

#if defined(RSVP_BOARD_WAVESHARE_ESP32S3_TOUCH_AMOLED_216) || \
    defined(RSVP_BOARD_WAVESHARE_ESP32S3_TOUCH_AMOLED_18)
  if (!gAxp2101Ready && !configureAxp2101IfAvailable()) {
    return false;
  }

  status.present = axp2101BatteryConnected();
  if (!status.present) {
    return false;
  }

  const uint16_t millivolts =
      axp2101ReadH5L8(kAxp2101BatteryVoltageHighReg, kAxp2101BatteryVoltageLowReg);
  if (millivolts == 0) {
    return false;
  }

  status.voltage = static_cast<float>(millivolts) / 1000.0f;

  uint8_t percent = 0;
  if (axp2101ReadRegister(kAxp2101BatteryPercentReg, percent) && percent <= 100) {
    status.percent = percent;
  } else {
    status.percent = batteryPercentForVoltage(status.voltage);
  }
  return true;

#elif defined(RSVP_BOARD_WAVESHARE_ESP32S3_TOUCH_AMOLED_241)
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

  status.percent = batteryPercentForVoltage(status.voltage);
  return true;

#elif defined(RSVP_BOARD_WAVESHARE_ESP32S3_TOUCH_LCD_349)
  delay(12);

  constexpr uint8_t kMaxSamples = 24;
  uint32_t millivolts[kMaxSamples];
  uint8_t samples = 0;
  for (uint8_t i = 0; i < kMaxSamples + 2; ++i) {
    const uint32_t sample = analogReadMilliVolts(PIN_BATTERY_ADC);
    if (i >= 2 && sample > 0 && samples < kMaxSamples) {
      millivolts[samples] = sample;
      ++samples;
    }
    delayMicroseconds(500);
  }

  if (samples == 0) {
    uint32_t rawTotal = 0;
    constexpr uint8_t kRawSamples = 16;
    for (uint8_t i = 0; i < kRawSamples; ++i) {
      rawTotal += analogRead(PIN_BATTERY_ADC);
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

  status.percent = batteryPercentForVoltage(status.voltage);
  return true;
  #endif
}

PowerDiagnosticSnapshot powerDiagnosticSnapshot() {
#if defined(RSVP_BOARD_WAVESHARE_ESP32S3_TOUCH_AMOLED_216) || \
    defined(RSVP_BOARD_WAVESHARE_ESP32S3_TOUCH_AMOLED_18)
  return gPowerDiagnosticSnapshot;
#else
  return PowerDiagnosticSnapshot{};
#endif
}

bool externalPowerPresent() {
#if defined(RSVP_BOARD_WAVESHARE_ESP32S3_TOUCH_AMOLED_216) || \
    defined(RSVP_BOARD_WAVESHARE_ESP32S3_TOUCH_AMOLED_18)
  if (!gAxp2101Ready && !configureAxp2101IfAvailable()) {
    return false;
  }

  uint8_t status1 = 0;
  uint8_t status2 = 0;
  if (!axp2101ReadRegister(kAxp2101Status1Reg, status1) ||
      !axp2101ReadRegister(kAxp2101Status2Reg, status2)) {
    return false;
  }

  const bool vbusGood = (status1 & 0x20) != 0;
  const bool vbusAbsent = (status2 & 0x08) != 0;
  return vbusGood && !vbusAbsent;
#else
  return false;
#endif
}

bool releaseBatteryPowerHold() {
#if defined(RSVP_BOARD_WAVESHARE_ESP32S3_TOUCH_AMOLED_216) || \
    defined(RSVP_BOARD_WAVESHARE_ESP32S3_TOUCH_AMOLED_18)
  if (!gAxp2101Ready && !configureAxp2101IfAvailable()) {
    return false;
  }

  if (!axp2101UpdateRegisterBits(kAxp2101CommonConfigReg, 0x01, 0x01)) {
    Serial.println("[board] AXP2101 shutdown request failed");
    return false;
  }

  Serial.println("[board] AXP2101 shutdown requested");
  return true;

#elif defined(RSVP_BOARD_WAVESHARE_ESP32S3_TOUCH_AMOLED_241)
  if (PIN_BATTERY_HOLD < 0) {
    return false;
  }

  digitalWrite(PIN_BATTERY_HOLD, LOW);
  gBatteryPowerHoldEnabled = false;
  Serial.println("[board] Battery power hold released");
  return true;

#elif defined(RSVP_BOARD_WAVESHARE_ESP32S3_TOUCH_LCD_349)
  if (!configureTca9554OutputPin(TCA9554_PIN_SYS_EN, false)) {
    Serial.println("[board] Battery power hold release failed");
    return false;
  }

  gBatteryPowerHoldEnabled = false;
  Serial.println("[board] Battery power hold released");
  return true;
#endif
}

bool readVirtualBootButtonHeld() {
  return false;
}

bool readVirtualPowerButtonHeld() {
#if defined(RSVP_BOARD_WAVESHARE_ESP32S3_TOUCH_AMOLED_18)
  const uint32_t nowMs = millis();
  if (gExpanderPowerButtonSeen &&
      nowMs - gExpanderPowerButtonLastPollMs < kExpanderPowerButtonPollIntervalMs) {
    return gExpanderPowerButtonStableHeld;
  }
  gExpanderPowerButtonLastPollMs = nowMs;

  bool held = false;
  if (!readExpanderPin(TCA9554_PIN_PWR_BUTTON, held)) {
    return gExpanderPowerButtonStableHeld;
  }

  if (!gExpanderPowerButtonSeen) {
    gExpanderPowerButtonSeen = true;
    gExpanderPowerButtonRawHeld = held;
    gExpanderPowerButtonStableHeld = held;
    gExpanderPowerButtonRawChangedMs = nowMs;
    return gExpanderPowerButtonStableHeld;
  }

  if (held != gExpanderPowerButtonRawHeld) {
    gExpanderPowerButtonRawHeld = held;
    gExpanderPowerButtonRawChangedMs = nowMs;
    return gExpanderPowerButtonStableHeld;
  }

  if (held != gExpanderPowerButtonStableHeld &&
      nowMs - gExpanderPowerButtonRawChangedMs >= kExpanderPowerButtonDebounceMs) {
    gExpanderPowerButtonStableHeld = held;
  }

  return gExpanderPowerButtonStableHeld;
#elif defined(RSVP_BOARD_WAVESHARE_ESP32S3_TOUCH_AMOLED_216)
  pollAxp2101PowerKeyIfDue();
  return gAxp2101PowerButtonHeld;
#else
  return false;
#endif
}

bool consumeVirtualPowerButtonShortPressEvent() {
#if defined(RSVP_BOARD_WAVESHARE_ESP32S3_TOUCH_AMOLED_216)
  pollAxp2101PowerKeyIfDue();
  const bool pending = gAxp2101PowerButtonShortPressPending;
  gAxp2101PowerButtonShortPressPending = false;
  return pending;
#else
  return false;
#endif
}

bool consumeVirtualPowerButtonLongPressEvent() {
#if defined(RSVP_BOARD_WAVESHARE_ESP32S3_TOUCH_AMOLED_216)
  pollAxp2101PowerKeyIfDue();
  const bool pending = gAxp2101PowerButtonLongPressPending;
  gAxp2101PowerButtonLongPressPending = false;
  return pending;
#else
  return false;
#endif
}

}  // namespace BoardConfig
