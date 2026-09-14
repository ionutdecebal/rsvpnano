#include "board/BoardDisplay.h"

#include "board/BacklightBrightness.h"

#include "platforms/waveshare_amoled_241/BoardDisplayPower.h"
#include "platforms/waveshare_amoled_241/WaveshareAmoled241.h"

namespace {

    constexpr uint8_t kMinimumBacklightDuty = Board::Backlight::kDefaultMinimumDuty;

    Arduino_ESP32QSPI gBus(WaveshareAmoled241::DisplayWiring::kCsPin, WaveshareAmoled241::DisplayWiring::kSclkPin,
                           WaveshareAmoled241::DisplayWiring::kData0Pin, WaveshareAmoled241::DisplayWiring::kData1Pin,
                           WaveshareAmoled241::DisplayWiring::kData2Pin, WaveshareAmoled241::DisplayWiring::kData3Pin);

    Arduino_RM690B0 gPanel(&gBus, WaveshareAmoled241::DisplayWiring::kResetPin, 0,
                           WaveshareAmoled241::DisplayWiring::kPanelWidth,
                           WaveshareAmoled241::DisplayWiring::kPanelHeight,
                           WaveshareAmoled241::DisplayWiring::kPanelColumnOffset,
                           WaveshareAmoled241::DisplayWiring::kPanelRowOffset,
                           WaveshareAmoled241::DisplayWiring::kPanelColumnOffset,
                           WaveshareAmoled241::DisplayWiring::kPanelRowOffset);

} // namespace

namespace Board::Display {

    bool begin() {
        if (!WaveshareAmoled241::DisplayPower::releaseHardware()) {
            return false;
        }
        const bool ok = gPanel.begin();
        gPanel.fillScreen(0x0000);
        return ok;
    }
    Arduino_GFX& gfx() {
        return gPanel;
    }

    ui::Orientation defaultUiOrientation() {
        return WaveshareAmoled241::DisplayWiring::kDefaultUiOrientation;
    }
    ui::Orientation rotatedUiOrientation() {
        return ui::opposite(defaultUiOrientation());
    }
    uint16_t nativeWidth() {
        return WaveshareAmoled241::DisplayWiring::kPanelWidth;
    }
    uint16_t nativeHeight() {
        return WaveshareAmoled241::DisplayWiring::kPanelHeight;
    }
    size_t txChunkBytes() {
        return WaveshareAmoled241::DisplayWiring::kTxChunkBytes;
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
