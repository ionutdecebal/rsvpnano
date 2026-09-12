#include "board/BoardInput.h"

#include <array>
#include <atomic>

#include <Wire.h>

#include "board/BoardPower.h"
#include "drivers/touch/ft6336/ft6336.h"
#include "platforms/waveshare_amoled_206/WaveshareAmoled206.h"

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
        if constexpr (WaveshareAmoled206::System::kTouchResetPin >= 0) {
            pinMode(WaveshareAmoled206::System::kTouchResetPin, OUTPUT);
            digitalWrite(WaveshareAmoled206::System::kTouchResetPin, LOW);
            delay(12);
            digitalWrite(WaveshareAmoled206::System::kTouchResetPin, HIGH);
            delay(12);
        }
    }

    bool primaryPressedRaw() {
        if constexpr (WaveshareAmoled206::Buttons::kBootPin < 0) {
            return false;
        }
        return !digitalRead(WaveshareAmoled206::Buttons::kBootPin);
    }

    bool powerPressedRaw() {
        return Board::Power::powerButtonHeld();
    }

    void configureButtonPins() {
        if constexpr (WaveshareAmoled206::Buttons::kBootPin >= 0) {
            pinMode(WaveshareAmoled206::Buttons::kBootPin, INPUT_PULLUP);
        }
    }

} // namespace

namespace Board::Input {

    bool begin() {
        configureButtonPins();
        return true;
    }

    void end() {
        detachInterrupt(WaveshareAmoled206::System::kTouchIrqPin);
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
        if (primaryPressed) {
            actions.shortPress |= ::Input::ActionSelect | ::Input::ActionPlayPause;
            actions.longPress |= ::Input::ActionStandby;
        }
        if (powerPressed) {
            actions.shortPress |= ::Input::ActionOpenMenu | ::Input::ActionBack;
            actions.longPress |= ::Input::ActionPowerOff;
        }
        return actions;
    }

    ui::TouchSurface touchSurface() {
        return {WaveshareAmoled206::DisplayWiring::kPanelWidth, WaveshareAmoled206::DisplayWiring::kPanelHeight};
    }

    ::Input::TouchTiming touchTiming() {
        return {};
    }

    bool beginTouch() {
        detachInterrupt(WaveshareAmoled206::System::kTouchIrqPin);
        gTouchInterruptAttached = false;
        gTouchPending.store(false);
        resetTouchHardware();
        TwoWire& wire = touchWire();
        if (!(Ft6336Touch::probe(wire, WaveshareAmoled206::TouchWiring::kAddress)
              && Ft6336Touch::configureMonitorMode(wire, WaveshareAmoled206::TouchWiring::kAddress)))
            return false;
        pinMode(WaveshareAmoled206::System::kTouchIrqPin, INPUT_PULLUP);
        attachInterrupt(WaveshareAmoled206::System::kTouchIrqPin, onTouchInterrupt, FALLING);
        gTouchInterruptAttached = true;
        if (!digitalRead(WaveshareAmoled206::System::kTouchIrqPin))
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
        std::array<uint8_t, Ft6336Touch::kPacketLength> data = {};
        if (!Ft6336Touch::readPacket(touchWire(), WaveshareAmoled206::TouchWiring::kAddress,
                                     WaveshareAmoled206::TouchWiring::kReleaseBusBeforeRead, data.data(),
                                     data.size())) {
            gTouchPending.store(true);
            return false;
        }

        BoardDrivers::Touch::Sample decoded = {};
        if (!Ft6336Touch::decodePacket(data.data(), data.size(), WaveshareAmoled206::DisplayWiring::kPanelWidth,
                                       WaveshareAmoled206::DisplayWiring::kPanelHeight, decoded)) {
            gTouchPending.store(true);
            return false;
        }

        contact = {decoded.touched, decoded.physicalX, decoded.physicalY};
        return true;
    }

} // namespace Board::Input
