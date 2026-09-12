#include "board/BoardInput.h"

#include <array>
#include <atomic>

#include <Wire.h>

#include "board/BoardPower.h"
#include "drivers/touch/cst92xx/cst92xx.h"
#include "platforms/waveshare_amoled_216/WaveshareAmoled216.h"

namespace {

    // Native word loads/stores only: ESP32 builds disable hardware RMW atomics (exchange/fetch/CAS).
    std::atomic<uint32_t> gTouchPending = 0;
    bool gTouchInterruptAttached = false;
    static_assert(sizeof(gTouchPending) == sizeof(uint32_t) && alignof(decltype(gTouchPending)) >= sizeof(uint32_t));

    void IRAM_ATTR onTouchInterrupt() {
        gTouchPending.store(true);
        ::Input::notifyTouchFromISR();
    }

    TwoWire& touchWire() {
        return Wire;
    }

    void resetTouchHardware() {
        if constexpr (WaveshareAmoled216::System::kTouchResetPin >= 0) {
            pinMode(WaveshareAmoled216::System::kTouchResetPin, OUTPUT);
            digitalWrite(WaveshareAmoled216::System::kTouchResetPin, LOW);
            delay(10);
            digitalWrite(WaveshareAmoled216::System::kTouchResetPin, HIGH);
            // Match Waveshare's CST92xx reset settling before the command-mode identification.
            delay(50);
        }
    }

    bool primaryPressedRaw() {
        if constexpr (WaveshareAmoled216::Buttons::kBootPin < 0) {
            return false;
        }
        return !digitalRead(WaveshareAmoled216::Buttons::kBootPin);
    }

    bool powerPressedRaw() {
        return Board::Power::powerButtonHeld();
    }

    bool keyPressedRaw() {
        if constexpr (WaveshareAmoled216::Buttons::kKeyPin < 0) {
            return false;
        }
        return !digitalRead(WaveshareAmoled216::Buttons::kKeyPin);
    }

    void configureButtonPins() {
        if constexpr (WaveshareAmoled216::Buttons::kBootPin >= 0) {
            pinMode(WaveshareAmoled216::Buttons::kBootPin, INPUT_PULLUP);
        }
        if constexpr (WaveshareAmoled216::Buttons::kKeyPin >= 0) {
            pinMode(WaveshareAmoled216::Buttons::kKeyPin, INPUT_PULLUP);
        }
    }

} // namespace

namespace Board::Input {

    bool begin() {
        configureButtonPins();
        return true;
    }

    void end() {
        detachInterrupt(WaveshareAmoled216::System::kTouchIrqPin);
        gTouchInterruptAttached = false;
        gTouchPending.store(false);
    }

    void cancel() {
        // Light sleep changes the GPIO interrupt to level-triggered; never leave the edge ISR installed.
        end();
    }

    ::Input::ControlTiming controlTiming() {
        return {};
    }

    ::Input::PressActions currentActions() {
        ::Input::PressActions actions = {};
        const bool primaryPressed = primaryPressedRaw();
        const bool powerPressed = powerPressedRaw();
        const bool keyPressed = keyPressedRaw();
        if (primaryPressed) {
            actions.shortPress |= ::Input::ActionSelect | ::Input::ActionPlayPause;
            actions.longPress |= ::Input::ActionStandby;
        }
        if (powerPressed) {
            actions.shortPress |= ::Input::ActionOpenMenu | ::Input::ActionBack;
            actions.longPress |= ::Input::ActionPowerOff;
        }
        if (keyPressed) {
            actions.shortPress |= ::Input::ActionPlayPause;
        }
        return actions;
    }

    ui::TouchSurface touchSurface() {
        return {WaveshareAmoled216::DisplayWiring::kPanelWidth, WaveshareAmoled216::DisplayWiring::kPanelHeight};
    }

    ::Input::TouchTiming touchTiming() {
        return {};
    }

    bool beginTouch() {
        detachInterrupt(WaveshareAmoled216::System::kTouchIrqPin);
        gTouchInterruptAttached = false;
        gTouchPending.store(false);
        resetTouchHardware();
        TwoWire& wire = touchWire();
        if (!Cst92xxTouch::begin(wire, WaveshareAmoled216::TouchWiring::kAddress))
            return false;
        pinMode(WaveshareAmoled216::System::kTouchIrqPin, INPUT_PULLUP);
        attachInterrupt(WaveshareAmoled216::System::kTouchIrqPin, onTouchInterrupt, FALLING);
        gTouchInterruptAttached = true;
        if (!digitalRead(WaveshareAmoled216::System::kTouchIrqPin))
            gTouchPending.store(true);
        return true;
    }

    bool touchReady() {
        // While paused, a GPIO wake is enough to justify one report even after its pulse has ended.
        return !gTouchInterruptAttached || gTouchPending.load();
    }

    bool readTouch(ui::TouchContact& contact) {
        // Consume before I2C so an IRQ arriving during the read remains pending.
        gTouchPending.store(false);
        std::array<uint8_t, Cst92xxTouch::kPacketLength> data = {};
        if (!Cst92xxTouch::readPacket(touchWire(), WaveshareAmoled216::TouchWiring::kAddress, data.data(),
                                      data.size())) {
            gTouchPending.store(true);
            return false;
        }

        BoardDrivers::Touch::Sample decoded = {};
        if (!Cst92xxTouch::decodePacket(data.data(), data.size(), WaveshareAmoled216::DisplayWiring::kPanelWidth,
                                        WaveshareAmoled216::DisplayWiring::kPanelHeight, decoded)) {
            gTouchPending.store(true);
            return false;
        }

        contact = {decoded.touched, decoded.physicalX, decoded.physicalY};
        return true;
    }

} // namespace Board::Input
