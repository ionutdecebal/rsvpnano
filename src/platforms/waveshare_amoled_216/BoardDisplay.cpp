#include "board/BoardDisplay.h"

#include "drivers/display/co5300/co5300.h"
#include "platforms/waveshare_amoled_216/WaveshareAmoled216.h"

namespace {

Co5300::Context gDisplayContext = {
    {
        WaveshareAmoled216::DisplayWiring::kCsPin,
        WaveshareAmoled216::DisplayWiring::kSclkPin,
        WaveshareAmoled216::DisplayWiring::kData0Pin,
        WaveshareAmoled216::DisplayWiring::kData1Pin,
        WaveshareAmoled216::DisplayWiring::kData2Pin,
        WaveshareAmoled216::DisplayWiring::kData3Pin,
        WaveshareAmoled216::DisplayWiring::kResetPin,
        WaveshareAmoled216::DisplayWiring::kBacklightPin,
        WaveshareAmoled216::DisplayWiring::kPanelWidth,
        WaveshareAmoled216::DisplayWiring::kPanelHeight,
        WaveshareAmoled216::DisplayWiring::kTxChunkBytes,
        WaveshareAmoled216::DisplayWiring::kPanelMemoryRotated180,
    },
};

}  // namespace

namespace Board::Display {

bool begin() {
  Co5300::init(gDisplayContext);
  return true;
}

void holdBacklightOffForDeepSleep() {}

Board::UiOrientation defaultUiOrientation() {
  return WaveshareAmoled216::DisplayWiring::kDefaultUiOrientation;
}

Board::UiOrientation rotatedUiOrientation() {
  return Board::oppositeUiOrientation(WaveshareAmoled216::DisplayWiring::kDefaultUiOrientation);
}

uint16_t nativeWidth() { return WaveshareAmoled216::DisplayWiring::kPanelWidth; }

uint16_t nativeHeight() { return WaveshareAmoled216::DisplayWiring::kPanelHeight; }

size_t txChunkBytes() { return WaveshareAmoled216::DisplayWiring::kTxChunkBytes; }

void setBacklight(bool on) { Co5300::setDisplayOn(gDisplayContext, on); }

void setBrightness(uint8_t percent) { Co5300::setBrightnessPercent(gDisplayContext, percent); }

void sleep() { Co5300::sleep(gDisplayContext); }

void wake() { Co5300::wake(gDisplayContext); }

bool pushColors(uint16_t x, uint16_t y, uint16_t width, uint16_t height,
                const uint16_t *data) {
  Co5300::pushColors(gDisplayContext, x, y, width, height, data);
  return true;
}

}  // namespace Board::Display
