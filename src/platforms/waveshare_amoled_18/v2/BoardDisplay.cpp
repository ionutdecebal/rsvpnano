#include "board/BoardDisplay.h"

#include "drivers/display/co5300/co5300.h"
#include "platforms/waveshare_amoled_18/BoardDisplayPower.h"
#include "platforms/waveshare_amoled_18/WaveshareAmoled18.h"

namespace {

Co5300::Context gDisplayContext = {
    {
        WaveshareAmoled18::DisplayWiring::kCsPin,
        WaveshareAmoled18::DisplayWiring::kSclkPin,
        WaveshareAmoled18::DisplayWiring::kData0Pin,
        WaveshareAmoled18::DisplayWiring::kData1Pin,
        WaveshareAmoled18::DisplayWiring::kData2Pin,
        WaveshareAmoled18::DisplayWiring::kData3Pin,
        WaveshareAmoled18::DisplayWiring::kResetPin,
        WaveshareAmoled18::DisplayWiring::kBacklightPin,
        WaveshareAmoled18::DisplayWiring::kPanelWidth,
        WaveshareAmoled18::DisplayWiring::kPanelHeight,
        WaveshareAmoled18::DisplayWiring::kTxChunkBytes,
        WaveshareAmoled18::DisplayWiring::kPanelMemoryRotated180,
        WaveshareAmoled18::DisplayWiring::kPanelColumnOffset,
        WaveshareAmoled18::DisplayWiring::kPanelRowOffset,
    },
};

}  // namespace

namespace Board::Display {

bool begin() {
  WaveshareAmoled18::DisplayPower::releaseHardware();
  Co5300::init(gDisplayContext);
  return true;
}

void holdBacklightOffForDeepSleep() {}

Board::UiOrientation defaultUiOrientation() {
  return WaveshareAmoled18::DisplayWiring::kDefaultUiOrientation;
}

Board::UiOrientation rotatedUiOrientation() {
  return Board::oppositeUiOrientation(WaveshareAmoled18::DisplayWiring::kDefaultUiOrientation);
}

uint16_t nativeWidth() { return WaveshareAmoled18::DisplayWiring::kPanelWidth; }

uint16_t nativeHeight() { return WaveshareAmoled18::DisplayWiring::kPanelHeight; }

size_t txChunkBytes() { return WaveshareAmoled18::DisplayWiring::kTxChunkBytes; }

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
