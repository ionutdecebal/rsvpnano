#include "board/BoardDisplay.h"

#include "board/BacklightBrightness.h"

#include "platforms/waveshare_amoled_206/WaveshareAmoled206.h"

namespace {

    constexpr uint8_t kMinimumBacklightDuty = Board::Backlight::kDefaultMinimumDuty;

    Arduino_ESP32QSPI gBus(WaveshareAmoled206::DisplayWiring::kCsPin, WaveshareAmoled206::DisplayWiring::kSclkPin,
                           WaveshareAmoled206::DisplayWiring::kData0Pin, WaveshareAmoled206::DisplayWiring::kData1Pin,
                           WaveshareAmoled206::DisplayWiring::kData2Pin, WaveshareAmoled206::DisplayWiring::kData3Pin);

    Arduino_CO5300 gPanel(&gBus, WaveshareAmoled206::DisplayWiring::kResetPin, 0,
                          WaveshareAmoled206::DisplayWiring::kPanelWidth,
                          WaveshareAmoled206::DisplayWiring::kPanelHeight,
                          WaveshareAmoled206::DisplayWiring::kColumnOffset,
                          WaveshareAmoled206::DisplayWiring::kRowOffset,
                          WaveshareAmoled206::DisplayWiring::kColumnOffset,
                          WaveshareAmoled206::DisplayWiring::kRowOffset);

} // namespace

namespace Board::Display {

    bool begin() {
        const bool ok = gPanel.begin();
        gPanel.fillScreen(0x0000);
        return ok;
    }
    Arduino_GFX& gfx() {
        return gPanel;
    }

    ui::Orientation defaultUiOrientation() {
        return WaveshareAmoled206::DisplayWiring::kDefaultUiOrientation;
    }
    ui::Orientation rotatedUiOrientation() {
        return ui::opposite(defaultUiOrientation());
    }
    uint16_t nativeWidth() {
        return WaveshareAmoled206::DisplayWiring::kPanelWidth;
    }
    uint16_t nativeHeight() {
        return WaveshareAmoled206::DisplayWiring::kPanelHeight;
    }
    size_t txChunkBytes() {
        return WaveshareAmoled206::DisplayWiring::kTxChunkBytes;
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
