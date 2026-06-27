#include "board/BoardButtons.h"

#include <Wire.h>

#include "drivers/gpio/Tca9554.h"

namespace {

constexpr uint32_t kExpanderPowerButtonPollIntervalMs = 25;
constexpr uint32_t kExpanderPowerButtonDebounceMs = 70;

struct ButtonContext {
  bool stableHeld = false;
  bool rawHeld = false;
  uint32_t rawChangedMs = 0;
  uint32_t lastPollMs = 0;
  bool seen = false;
};

ButtonContext gPowerButton;

}  // namespace

namespace Board::Buttons {

bool readVirtualBootHeld() { return false; }

bool readVirtualPowerHeld() {
  const uint32_t nowMs = millis();
  if (gPowerButton.seen && nowMs - gPowerButton.lastPollMs < kExpanderPowerButtonPollIntervalMs) {
    return gPowerButton.stableHeld;
  }
  gPowerButton.lastPollMs = nowMs;

  bool held = false;
  if (!BoardDrivers::Tca9554::readInputPin(
          Wire, static_cast<uint8_t>(Board::Config::TCA9554_ADDRESS),
          Board::Config::TCA9554_PIN_PWR_BUTTON, held,
          Board::Config::TCA9554_RELEASE_BUS_BEFORE_READ)) {
    return gPowerButton.stableHeld;
  }

  if (!gPowerButton.seen) {
    gPowerButton.seen = true;
    gPowerButton.rawHeld = held;
    gPowerButton.stableHeld = held;
    gPowerButton.rawChangedMs = nowMs;
    return gPowerButton.stableHeld;
  }

  if (held != gPowerButton.rawHeld) {
    gPowerButton.rawHeld = held;
    gPowerButton.rawChangedMs = nowMs;
    return gPowerButton.stableHeld;
  }

  if (held != gPowerButton.stableHeld &&
      nowMs - gPowerButton.rawChangedMs >= kExpanderPowerButtonDebounceMs) {
    gPowerButton.stableHeld = held;
  }

  return gPowerButton.stableHeld;
}

bool consumeVirtualPowerShortPress() { return false; }
bool consumeVirtualPowerLongPress() { return false; }
bool usesPowerEvents() { return Config::APP_POWER_BUTTON_USES_PMU_EVENTS; }
uint32_t powerEventIgnoreMs() { return Config::PMU_BOOT_BUTTON_IGNORE_MS; }

}  // namespace Board::Buttons
