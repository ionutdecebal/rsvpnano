#include "board/BoardDisplay.h"

#include "drivers/display/jd9853/jd9853.h"
#include "platforms/waveshare_c6_touch_lcd_147/WaveshareC6TouchLcd147.h"

namespace {

Jd9853::Context gDisplayContext = {
    {
        WaveshareC6TouchLcd147::DisplayWiring::kCsPin,
        WaveshareC6TouchLcd147::DisplayWiring::kDcPin,
        WaveshareC6TouchLcd147::DisplayWiring::kSclkPin,
        WaveshareC6TouchLcd147::DisplayWiring::kMosiPin,
        WaveshareC6TouchLcd147::DisplayWiring::kMisoPin,
        WaveshareC6TouchLcd147::DisplayWiring::kResetPin,
        WaveshareC6TouchLcd147::DisplayWiring::kBacklightPin,
        WaveshareC6TouchLcd147::DisplayWiring::kPanelWidth,
        WaveshareC6TouchLcd147::DisplayWiring::kPanelHeight,
        WaveshareC6TouchLcd147::DisplayWiring::kColumnOffset,
        WaveshareC6TouchLcd147::DisplayWiring::kRowOffset,
        WaveshareC6TouchLcd147::DisplayWiring::kTxChunkBytes,
    },
};

}  // namespace

namespace Board::Display {

bool begin() {
  Jd9853::init(gDisplayContext);
  return true;
}

void enablePowerIfAvailable() {}

void holdBacklightOffForDeepSleep() { setBacklight(false); }

Board::UiOrientation defaultUiOrientation() {
  return WaveshareC6TouchLcd147::DisplayWiring::kDefaultUiOrientation;
}

Board::UiOrientation rotatedUiOrientation() {
  return Board::oppositeUiOrientation(
      WaveshareC6TouchLcd147::DisplayWiring::kDefaultUiOrientation);
}

uint16_t nativeWidth() { return WaveshareC6TouchLcd147::DisplayWiring::kPanelWidth; }

uint16_t nativeHeight() { return WaveshareC6TouchLcd147::DisplayWiring::kPanelHeight; }

size_t txChunkBytes() { return WaveshareC6TouchLcd147::DisplayWiring::kTxChunkBytes; }

void setBacklight(bool on) { Jd9853::setDisplayOn(gDisplayContext, on); }

void setBrightness(uint8_t percent) { Jd9853::setBrightnessPercent(gDisplayContext, percent); }

void sleep() { Jd9853::sleep(gDisplayContext); }

void wake() { Jd9853::wake(gDisplayContext); }

bool pushColors(uint16_t x, uint16_t y, uint16_t width, uint16_t height,
                const uint16_t *data) {
  Jd9853::pushColors(gDisplayContext, x, y, width, height, data);
  return true;
}

}  // namespace Board::Display
