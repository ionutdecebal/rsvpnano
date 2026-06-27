#include "board/BoardPower.h"

#include <Wire.h>

#include "drivers/power/axp2101/Axp2101.h"
#include "drivers/gpio/Tca9554.h"

namespace {

struct PowerContext {
  bool tca9554Sequenced = false;
};

PowerContext gPower;

bool tcaRead(uint8_t reg, uint8_t &value) {
  return BoardDrivers::Tca9554::read(Wire, static_cast<uint8_t>(Board::Config::TCA9554_ADDRESS),
                                     reg, value, Board::Config::TCA9554_RELEASE_BUS_BEFORE_READ);
}

bool tcaWrite(uint8_t reg, uint8_t value) {
  return BoardDrivers::Tca9554::write(Wire, static_cast<uint8_t>(Board::Config::TCA9554_ADDRESS),
                                      reg, value);
}

void configureIoExpander(bool forceDisplaySequence = false) {
  uint8_t output = 0xFF;
  uint8_t config = 0xFF;
  if (!tcaRead(BoardDrivers::Tca9554::kOutputReg, output) ||
      !tcaRead(BoardDrivers::Tca9554::kConfigReg, config)) {
    Serial.println("[board] TCA9554 not detected");
    return;
  }

  const uint8_t displayMask =
      static_cast<uint8_t>((1U << Board::Config::TCA9554_PIN_TOUCH_RESET) |
                           (1U << Board::Config::TCA9554_PIN_LCD_RESET) |
                           (1U << Board::Config::TCA9554_PIN_DISPLAY_ENABLE));
  const uint8_t outputMask =
      static_cast<uint8_t>(displayMask | (1U << Board::Config::TCA9554_PIN_SD_ENABLE));
  const bool runDisplaySequence = forceDisplaySequence || !gPower.tca9554Sequenced;

  if (runDisplaySequence) {
    output &= static_cast<uint8_t>(~displayMask);
  } else {
    output |= displayMask;
  }
  output |= static_cast<uint8_t>(1U << Board::Config::TCA9554_PIN_SD_ENABLE);
  config &= static_cast<uint8_t>(~outputMask);
  config |= static_cast<uint8_t>((1U << Board::Config::TCA9554_PIN_PWR_BUTTON) |
                                 (1U << Board::Config::TCA9554_PIN_PMU_IRQ));

  if (!tcaWrite(BoardDrivers::Tca9554::kOutputReg, output) ||
      !tcaWrite(BoardDrivers::Tca9554::kConfigReg, config)) {
    Serial.println("[board] TCA9554 output setup failed");
    return;
  }
  if (!runDisplaySequence) {
    return;
  }

  if (forceDisplaySequence) {
    Serial.println("[board] TCA9554 display/touch wake sequence");
  }
  delay(20);
  output |= displayMask;
  if (!tcaWrite(BoardDrivers::Tca9554::kOutputReg, output)) {
    Serial.println("[board] TCA9554 display release failed");
    return;
  }
  delay(50);
  gPower.tca9554Sequenced = true;
}

}  // namespace

namespace Board::Power {

void begin() {
  configureIoExpander();
  BoardDrivers::Axp2101::begin();
}

void prepareDeepSleepPowerHold() {}

void resetWakePeripherals() { configureIoExpander(true); }

bool enableAudioPowerIfAvailable() { return true; }

bool readBatteryStatus(BatteryStatus &status) {
  return BoardDrivers::Axp2101::readBatteryStatus(status);
}

DiagnosticSnapshot diagnosticSnapshot() { return BoardDrivers::Axp2101::diagnosticSnapshot(); }

bool externalPowerPresent() { return BoardDrivers::Axp2101::externalPowerPresent(); }

bool releaseBatteryPowerHold() { return BoardDrivers::Axp2101::releasePower(); }

bool powerOffUsesControllerWake() { return Config::REQUEST_PMU_SHUTDOWN_ON_POWEROFF; }

bool shouldRequestShutdownOnPowerOff() { return Config::REQUEST_PMU_SHUTDOWN_ON_POWEROFF; }

bool shouldReleaseBatteryPowerBeforeDeepSleep() {
  return Config::RELEASE_BATTERY_HOLD_BEFORE_DEEP_SLEEP;
}

}  // namespace Board::Power
