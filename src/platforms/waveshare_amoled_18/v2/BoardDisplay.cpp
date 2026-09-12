#include "board/BoardDisplay.h"

#include "board/BacklightBrightness.h"

#include "platforms/waveshare_amoled_18/BoardDisplayPower.h"
#include "platforms/waveshare_amoled_18/WaveshareAmoled18.h"

namespace {

    constexpr uint8_t kMinimumBacklightDuty = Board::Backlight::kDefaultMinimumDuty;

    Arduino_ESP32QSPI gBus(WaveshareAmoled18::DisplayWiring::kCsPin, WaveshareAmoled18::DisplayWiring::kSclkPin,
                           WaveshareAmoled18::DisplayWiring::kData0Pin, WaveshareAmoled18::DisplayWiring::kData1Pin,
                           WaveshareAmoled18::DisplayWiring::kData2Pin, WaveshareAmoled18::DisplayWiring::kData3Pin);

    Arduino_CO5300 gPanel(&gBus, WaveshareAmoled18::DisplayWiring::kResetPin, 0,
                          WaveshareAmoled18::DisplayWiring::kPanelWidth, WaveshareAmoled18::DisplayWiring::kPanelHeight,
                          WaveshareAmoled18::DisplayWiring::kPanelColumnOffset,
                          WaveshareAmoled18::DisplayWiring::kPanelRowOffset,
                          WaveshareAmoled18::DisplayWiring::kPanelColumnOffset,
                          WaveshareAmoled18::DisplayWiring::kPanelRowOffset);

} // namespace

namespace Board::Display {

    bool begin() {
        if (!WaveshareAmoled18::DisplayPower::releaseHardware() || !gPanel.begin())
            return false;
        gPanel.fillScreen(0x0000);
        return true;
    }

    Arduino_GFX& gfx() {
        return gPanel;
    }

    ui::Orientation defaultUiOrientation() {
        return WaveshareAmoled18::DisplayWiring::kDefaultUiOrientation;
    }
    ui::Orientation rotatedUiOrientation() {
        return ui::opposite(defaultUiOrientation());
    }
    uint16_t nativeWidth() {
        return WaveshareAmoled18::DisplayWiring::kPanelWidth;
    }
    uint16_t nativeHeight() {
        return WaveshareAmoled18::DisplayWiring::kPanelHeight;
    }
    size_t txChunkBytes() {
        return WaveshareAmoled18::DisplayWiring::kTxChunkBytes;
    }
    void setBacklight(bool on) {
        on ? gPanel.displayOn() : gPanel.displayOff();
    }
    void setBrightness(uint8_t percent) {
        gPanel.setBrightness(Board::Backlight::dutyFromPercent(percent, kMinimumBacklightDuty));
    }
    void sleep() {
        gPanel.displayOff();
    }
    void wake() {
        gPanel.displayOn();
    }
    bool pushColors(uint16_t x, uint16_t y, uint16_t width, uint16_t height, const uint16_t* data) {
        gPanel.draw16bitRGBBitmap(x, y, const_cast<uint16_t*>(data), width, height);
        return true;
    }

} // namespace Board::Display
