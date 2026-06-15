#include "board/BoardDisplay.h"

#include "drivers/display/rm690b0/rm690b0.h"

namespace {

Rm690b0::Context gDisplayContext;

}  // namespace

namespace Board::Display {

bool begin() {
  Rm690b0::init(gDisplayContext);
  return true;
}

void enablePowerIfAvailable() {}

void holdBacklightOffForDeepSleep() {}

void setBacklight(bool on) { Rm690b0::setDisplayOn(gDisplayContext, on); }

void flashBacklight(uint8_t count, uint32_t onMs, uint32_t offMs) {
  for (uint8_t i = 0; i < count; ++i) {
    setBacklight(true);
    delay(onMs);
    setBacklight(false);
    delay(offMs);
  }
}

void setBrightness(uint8_t percent) { Rm690b0::setBrightnessPercent(gDisplayContext, percent); }

void sleep() { Rm690b0::sleep(gDisplayContext); }

void wake() { Rm690b0::wake(gDisplayContext); }

bool pushColors(uint16_t x, uint16_t y, uint16_t width, uint16_t height,
                const uint16_t *data) {
  Rm690b0::pushColors(gDisplayContext, x, y, width, height, data);
  return true;
}

}  // namespace Board::Display
