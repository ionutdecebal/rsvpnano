#include "board/BoardDisplay.h"

#include <Wire.h>
#include <esp_log.h>

#include "drivers/display/rm690b0/rm690b0.h"
#include "drivers/gpio/tca9554/Tca9554.h"
#include "platforms/waveshare_amoled_241/WaveshareAmoled241.h"

namespace {

constexpr char kBoardDisplayTag[] = "board-display";

Rm690b0::Context gDisplayContext = {
    {
        WaveshareAmoled241::DisplayWiring::kCsPin,
        WaveshareAmoled241::DisplayWiring::kSclkPin,
        WaveshareAmoled241::DisplayWiring::kData0Pin,
        WaveshareAmoled241::DisplayWiring::kData1Pin,
        WaveshareAmoled241::DisplayWiring::kData2Pin,
        WaveshareAmoled241::DisplayWiring::kData3Pin,
        WaveshareAmoled241::DisplayWiring::kResetPin,
        WaveshareAmoled241::DisplayWiring::kBacklightPin,
        WaveshareAmoled241::DisplayWiring::kPanelWidth,
        WaveshareAmoled241::DisplayWiring::kPanelHeight,
        WaveshareAmoled241::DisplayWiring::kTxChunkBytes,
        WaveshareAmoled241::DisplayWiring::kPanelMemoryRotated180,
    },
};

void enableDisplayRail() {
  if (!BoardDrivers::Tca9554::configureOutputPin(
          Wire1, WaveshareAmoled241::Tca9554Wiring::kDisplayRailAddress,
          WaveshareAmoled241::Tca9554Wiring::kDisplayRailEnablePin, true,
          WaveshareAmoled241::Tca9554Wiring::kDisplayRailReleaseBusBeforeRead)) {
    ESP_LOGW(kBoardDisplayTag, "Failed to enable display rail");
    return;
  }

  delay(25);
}

}  // namespace

namespace Board::Display {

bool begin() {
  enableDisplayRail();
  Rm690b0::init(gDisplayContext);
  return true;
}

void holdBacklightOffForDeepSleep() {}

Board::UiOrientation defaultUiOrientation() {
  return WaveshareAmoled241::DisplayWiring::kDefaultUiOrientation;
}

Board::UiOrientation rotatedUiOrientation() {
  return Board::oppositeUiOrientation(WaveshareAmoled241::DisplayWiring::kDefaultUiOrientation);
}

uint16_t nativeWidth() { return WaveshareAmoled241::DisplayWiring::kPanelWidth; }

uint16_t nativeHeight() { return WaveshareAmoled241::DisplayWiring::kPanelHeight; }

size_t txChunkBytes() { return WaveshareAmoled241::DisplayWiring::kTxChunkBytes; }

void setBacklight(bool on) { Rm690b0::setDisplayOn(gDisplayContext, on); }

void setBrightness(uint8_t percent) { Rm690b0::setBrightnessPercent(gDisplayContext, percent); }

void sleep() { Rm690b0::sleep(gDisplayContext); }

void wake() { Rm690b0::wake(gDisplayContext); }

bool pushColors(uint16_t x, uint16_t y, uint16_t width, uint16_t height,
                const uint16_t *data) {
  Rm690b0::pushColors(gDisplayContext, x, y, width, height, data);
  return true;
}

}  // namespace Board::Display
