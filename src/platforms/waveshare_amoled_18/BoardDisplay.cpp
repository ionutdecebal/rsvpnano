#include "board/BoardDisplay.h"

#include "drivers/display/sh8601/sh8601.h"

namespace {

Sh8601::Context gDisplayContext;

}  // namespace

namespace Board::Display {

bool begin() {
  Sh8601::init(gDisplayContext);
  return true;
}

void enablePowerIfAvailable() {}

void holdBacklightOffForDeepSleep() {}

void setBacklight(bool on) { Sh8601::setDisplayOn(gDisplayContext, on); }

void flashBacklight(uint8_t count, uint32_t onMs, uint32_t offMs) {
  for (uint8_t i = 0; i < count; ++i) {
    setBacklight(true);
    delay(onMs);
    setBacklight(false);
    delay(offMs);
  }
}

void setBrightness(uint8_t percent) { Sh8601::setBrightnessPercent(gDisplayContext, percent); }

void sleep() { Sh8601::sleep(gDisplayContext); }

void wake() { Sh8601::wake(gDisplayContext); }

bool pushColors(uint16_t x, uint16_t y, uint16_t width, uint16_t height,
                const uint16_t *data) {
  Sh8601::pushColors(gDisplayContext, x, y, width, height, data);
  return true;
}

}  // namespace Board::Display
