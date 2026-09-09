#include "board/BoardDisplay.h"

#include "board/BacklightBrightness.h"

#include "platforms/waveshare_c6_touch_lcd_147/WaveshareC6TouchLcd147.h"

namespace {

    Arduino_ESP32SPI gBus(WaveshareC6TouchLcd147::DisplayWiring::kDcPin, WaveshareC6TouchLcd147::DisplayWiring::kCsPin,
                          WaveshareC6TouchLcd147::DisplayWiring::kSclkPin,
                          WaveshareC6TouchLcd147::DisplayWiring::kMosiPin,
                          WaveshareC6TouchLcd147::DisplayWiring::kMisoPin);

    Arduino_ST7789 gPanel(&gBus, WaveshareC6TouchLcd147::DisplayWiring::kResetPin, 0, false,
                          WaveshareC6TouchLcd147::DisplayWiring::kPanelWidth,
                          WaveshareC6TouchLcd147::DisplayWiring::kPanelHeight,
                          WaveshareC6TouchLcd147::DisplayWiring::kColumnOffset,
                          WaveshareC6TouchLcd147::DisplayWiring::kRowOffset,
                          WaveshareC6TouchLcd147::DisplayWiring::kColumnOffset,
                          WaveshareC6TouchLcd147::DisplayWiring::kRowOffset);

    const uint8_t kJd9853InitOperations[] = {
        BEGIN_WRITE,
        WRITE_COMMAND_8, 0x11,
        END_WRITE,
        DELAY, 120,

        BEGIN_WRITE,
        WRITE_C8_D16, 0xDF, 0x98, 0x53,
        WRITE_C8_D8, 0xB2, 0x23,
        WRITE_COMMAND_8, 0xB7,
        WRITE_BYTES, 4, 0x00, 0x47, 0x00, 0x6F,
        WRITE_COMMAND_8, 0xBB,
        WRITE_BYTES, 6, 0x1C, 0x1A, 0x55, 0x73, 0x63, 0xF0,
        WRITE_C8_D16, 0xC0, 0x44, 0xA4,
        WRITE_C8_D8, 0xC1, 0x16,
        WRITE_COMMAND_8, 0xC3,
        WRITE_BYTES, 8, 0x7D, 0x07, 0x14, 0x06, 0xCF, 0x71, 0x72, 0x77,
        WRITE_COMMAND_8, 0xC4,
        WRITE_BYTES, 12, 0x00, 0x00, 0xA0, 0x79, 0x0B, 0x0A, 0x16, 0x79, 0x0B, 0x0A, 0x16, 0x82,
        WRITE_COMMAND_8, 0xC8,
        WRITE_BYTES, 32,
        0x3F, 0x32, 0x29, 0x29, 0x27, 0x2B, 0x27, 0x28,
        0x28, 0x26, 0x25, 0x17, 0x12, 0x0D, 0x04, 0x00,
        0x3F, 0x32, 0x29, 0x29, 0x27, 0x2B, 0x27, 0x28,
        0x28, 0x26, 0x25, 0x17, 0x12, 0x0D, 0x04, 0x00,
        WRITE_COMMAND_8, 0xD0,
        WRITE_BYTES, 5, 0x04, 0x06, 0x6B, 0x0F, 0x00,
        WRITE_C8_D16, 0xD7, 0x00, 0x30,
        WRITE_C8_D8, 0xE6, 0x14,
        WRITE_C8_D8, 0xDE, 0x01,
        WRITE_COMMAND_8, 0xB7,
        WRITE_BYTES, 5, 0x03, 0x13, 0xEF, 0x35, 0x35,
        WRITE_COMMAND_8, 0xC1,
        WRITE_BYTES, 3, 0x14, 0x15, 0xC0,
        WRITE_C8_D16, 0xC2, 0x06, 0x3A,
        WRITE_C8_D16, 0xC4, 0x72, 0x12,
        WRITE_C8_D8, 0xBE, 0x00,
        WRITE_C8_D8, 0xDE, 0x02,
        WRITE_COMMAND_8, 0xE5,
        WRITE_BYTES, 3, 0x00, 0x02, 0x00,
        WRITE_COMMAND_8, 0xE5,
        WRITE_BYTES, 3, 0x01, 0x02, 0x00,
        WRITE_C8_D8, 0xDE, 0x00,
        WRITE_C8_D8, 0x35, 0x00,
        WRITE_C8_D8, 0x3A, 0x05,
        WRITE_COMMAND_8, 0x2A,
        WRITE_BYTES, 4, 0x00, 0x22, 0x00, 0xCD,
        WRITE_COMMAND_8, 0x2B,
        WRITE_BYTES, 4, 0x00, 0x00, 0x01, 0x3F,
        WRITE_C8_D8, 0xDE, 0x02,
        WRITE_COMMAND_8, 0xE5,
        WRITE_BYTES, 3, 0x00, 0x02, 0x00,
        WRITE_C8_D8, 0xDE, 0x00,
        WRITE_C8_D8, 0x36, 0x00,
        WRITE_COMMAND_8, 0x21,
        END_WRITE,
        DELAY, 10,

        BEGIN_WRITE,
        WRITE_COMMAND_8, 0x29,
        END_WRITE,
    };

    constexpr uint8_t kMinimumBacklightDuty = Board::Backlight::kDefaultMinimumDuty;

    uint8_t gBacklightDuty = Board::Backlight::dutyFromPercent(100, kMinimumBacklightDuty);
    bool gBacklight = true;

    void writeBacklight() {
        if constexpr (WaveshareC6TouchLcd147::DisplayWiring::kBacklightPin < 0) {
            return;
        }
        analogWriteResolution(WaveshareC6TouchLcd147::DisplayWiring::kBacklightPin, 8);
        analogWriteFrequency(WaveshareC6TouchLcd147::DisplayWiring::kBacklightPin, 20000);
        analogWrite(WaveshareC6TouchLcd147::DisplayWiring::kBacklightPin, gBacklight ? gBacklightDuty : 0);
    }

} // namespace

namespace Board::Display {

    bool begin() {
        const bool ok = gPanel.begin();
        if (ok) {
            gBus.batchOperation(kJd9853InitOperations, sizeof(kJd9853InitOperations));
        }
        gPanel.fillScreen(0x0000);
        writeBacklight();
        return ok;
    }
    Arduino_GFX& gfx() {
        return gPanel;
    }

    ui::Orientation defaultUiOrientation() {
        return WaveshareC6TouchLcd147::DisplayWiring::kDefaultUiOrientation;
    }
    ui::Orientation rotatedUiOrientation() {
        return ui::opposite(defaultUiOrientation());
    }
    uint16_t nativeWidth() {
        return WaveshareC6TouchLcd147::DisplayWiring::kPanelWidth;
    }
    uint16_t nativeHeight() {
        return WaveshareC6TouchLcd147::DisplayWiring::kPanelHeight;
    }
    size_t txChunkBytes() {
        return WaveshareC6TouchLcd147::DisplayWiring::kTxChunkBytes;
    }
    void setBacklight(bool on) {
        gBacklight = on;
        writeBacklight();
    }
    void setBrightness(uint8_t percent) {
        gBacklightDuty = Board::Backlight::dutyFromPercent(percent, kMinimumBacklightDuty);
        writeBacklight();
    }
    void sleep() {
        setBacklight(false);
        gPanel.displayOff();
    }
    void wake() {
        gPanel.displayOn();
        setBacklight(true);
    }
    bool pushColors(uint16_t x, uint16_t y, uint16_t width, uint16_t height, const uint16_t* data) {
        gPanel.draw16bitRGBBitmap(x, y, const_cast<uint16_t*>(data), width, height);
        return true;
    }

} // namespace Board::Display
