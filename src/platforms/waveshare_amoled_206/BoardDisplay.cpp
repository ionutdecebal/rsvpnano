#include "board/BoardDisplay.h"

#include "drivers/display/co5300/co5300.h"
#include "platforms/waveshare_amoled_206/WaveshareAmoled206.h"

namespace {

Co5300::Context gDisplayContext = {
    {
        WaveshareAmoled206::DisplayWiring::kCsPin,
        WaveshareAmoled206::DisplayWiring::kSclkPin,
        WaveshareAmoled206::DisplayWiring::kData0Pin,
        WaveshareAmoled206::DisplayWiring::kData1Pin,
        WaveshareAmoled206::DisplayWiring::kData2Pin,
        WaveshareAmoled206::DisplayWiring::kData3Pin,
        WaveshareAmoled206::DisplayWiring::kResetPin,
        WaveshareAmoled206::DisplayWiring::kBacklightPin,
        WaveshareAmoled206::DisplayWiring::kPanelWidth,
        WaveshareAmoled206::DisplayWiring::kPanelHeight,
        WaveshareAmoled206::DisplayWiring::kTxChunkBytes,
        WaveshareAmoled206::DisplayWiring::kUiRotated180,
        WaveshareAmoled206::DisplayWiring::kColumnOffset,
        WaveshareAmoled206::DisplayWiring::kRowOffset,
    },
};

}  // namespace

namespace Board::Display {

bool begin() {
  Co5300::init(gDisplayContext);
  return true;
}

void enablePowerIfAvailable() {}

void holdBacklightOffForDeepSleep() {}

uint16_t nativeWidth() { return WaveshareAmoled206::DisplayWiring::kPanelWidth; }

uint16_t nativeHeight() { return WaveshareAmoled206::DisplayWiring::kPanelHeight; }

size_t txChunkBytes() { return WaveshareAmoled206::DisplayWiring::kTxChunkBytes; }

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
