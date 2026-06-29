#include "board/BoardDisplay.h"

#include <Wire.h>
#include <driver/gpio.h>

#include "drivers/display/axs15231b/axs15231b.h"
#include "drivers/gpio/tca9554/Tca9554.h"
#include "platforms/waveshare_lcd_349/WaveshareLcd349.h"

namespace {

    Axs15231b::Context gDisplayContext = {
        {
            WaveshareLcd349::DisplayWiring::kCsPin,
            WaveshareLcd349::DisplayWiring::kSclkPin,
            WaveshareLcd349::DisplayWiring::kData0Pin,
            WaveshareLcd349::DisplayWiring::kData1Pin,
            WaveshareLcd349::DisplayWiring::kData2Pin,
            WaveshareLcd349::DisplayWiring::kData3Pin,
            WaveshareLcd349::DisplayWiring::kResetPin,
            WaveshareLcd349::DisplayWiring::kBacklightPin,
            WaveshareLcd349::DisplayWiring::kPanelWidth,
            WaveshareLcd349::DisplayWiring::kPanelHeight,
            WaveshareLcd349::DisplayWiring::kTxChunkBytes,
            WaveshareLcd349::DisplayWiring::kPanelMemoryRotated180,


        },
    };
} // namespace

namespace Board::Display {

    void enableBacklightPower() {
        if (!BoardDrivers::Tca9554::configureOutputPin(Wire1, WaveshareLcd349::Tca9554Wiring::kAddress,
                                                       WaveshareLcd349::Tca9554Wiring::kBacklightEnablePin, true,
                                                       WaveshareLcd349::Tca9554Wiring::kReleaseBusBeforeRead)) {
            Serial.println("[board] TCA9554 backlight enable not configured");
            return;
        }

        Serial.println("[board] Backlight enable configured");
    }

    bool begin() {
        Axs15231b::init(gDisplayContext);
        enableBacklightPower();
        return true;
    }

    void holdBacklightOffForDeepSleep() {
        if constexpr (WaveshareLcd349::DisplayWiring::kBacklightPin < 0) {
            return;
        }

        analogWrite(WaveshareLcd349::DisplayWiring::kBacklightPin, 255);
        pinMode(WaveshareLcd349::DisplayWiring::kBacklightPin, OUTPUT);
        digitalWrite(WaveshareLcd349::DisplayWiring::kBacklightPin, HIGH);
        gpio_set_direction(WaveshareLcd349::DisplayWiring::kBacklightGpio, GPIO_MODE_OUTPUT);
        gpio_set_level(WaveshareLcd349::DisplayWiring::kBacklightGpio, 1);
        gpio_hold_en(WaveshareLcd349::DisplayWiring::kBacklightGpio);
        gpio_deep_sleep_hold_en();
    }

    Board::UiOrientation defaultUiOrientation() {
        return WaveshareLcd349::DisplayWiring::kDefaultUiOrientation;
    }

    Board::UiOrientation rotatedUiOrientation() {
        return Board::oppositeUiOrientation(WaveshareLcd349::DisplayWiring::kDefaultUiOrientation);
    }

    uint16_t nativeWidth() {
        return WaveshareLcd349::DisplayWiring::kPanelWidth;
    }

    uint16_t nativeHeight() {
        return WaveshareLcd349::DisplayWiring::kPanelHeight;
    }

    size_t txChunkBytes() {
        return WaveshareLcd349::DisplayWiring::kTxChunkBytes;
    }

    void setBacklight(bool on) {
        Axs15231b::setBacklight(gDisplayContext, on);
    }

    void setBrightness(uint8_t percent) {
        Axs15231b::setBrightnessPercent(gDisplayContext, percent);
    }

    void sleep() {
        Axs15231b::sleep(gDisplayContext);
    }

    void wake() {
        Axs15231b::wake(gDisplayContext);
    }

    bool pushColors(uint16_t x, uint16_t y, uint16_t width, uint16_t height, const uint16_t* data) {
        Axs15231b::pushColors(gDisplayContext, x, y, width, height, data);
        return true;
    }

} // namespace Board::Display
