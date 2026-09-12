#include "board/BoardInput.h"

#include <array>
#include <atomic>

#include <Wire.h>

#include "drivers/gpio/tca9554/Tca9554.h"
#include "drivers/touch/ft6336/ft6336.h"
#include "platforms/waveshare_amoled_18/BoardDisplayPower.h"
#include "platforms/waveshare_amoled_18/WaveshareAmoled18.h"

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

    bool tcaPinHeld(uint8_t pin) {
        bool levelHigh = false;
        return BoardDrivers::Tca9554::readInputPin(Wire, WaveshareAmoled18::Tca9554Wiring::kAddress, pin, levelHigh,
                                                   WaveshareAmoled18::Tca9554Wiring::kReleaseBusBeforeRead)
            && levelHigh;
    }

    bool primaryPressedRaw() {
        if constexpr (WaveshareAmoled18::Buttons::kBootPin < 0) {
            return false;
        }
        return !digitalRead(WaveshareAmoled18::Buttons::kBootPin);
    }

    bool powerPressedRaw() {
        return tcaPinHeld(WaveshareAmoled18::Tca9554Wiring::kPowerButtonPin);
    }

    void configureButtonPins() {
        if constexpr (WaveshareAmoled18::Buttons::kBootPin >= 0) {
            pinMode(WaveshareAmoled18::Buttons::kBootPin, INPUT_PULLUP);
        }
    }

} // namespace

namespace Board::Input {

    bool begin() {
        configureButtonPins();
        return true;
    }

    void end() {
        detachInterrupt(WaveshareAmoled18::System::kTouchIrqPin);
        gTouchInterruptAttached = false;
        gTouchPending.store(false);
    }

    void cancel() {
        // Light sleep changes the GPIO interrupt to level-triggered; never leave the edge ISR installed.
        end();
    }

    ::Input::ControlTiming controlTiming() {
        return {.debounceMs = WaveshareAmoled18::Buttons::kDebounceMs};
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
        return {WaveshareAmoled18::DisplayWiring::kPanelWidth, WaveshareAmoled18::DisplayWiring::kPanelHeight};
    }

    ::Input::TouchTiming touchTiming() {
        return {
            .failureBackoffMs = WaveshareAmoled18::TouchWiring::kFailureBackoffMs,
            .recoveryRetryMs = WaveshareAmoled18::TouchWiring::kRecoveryRetryMs,
            .recoveryEventIgnoreMs = WaveshareAmoled18::TouchWiring::kRecoveryEventIgnoreMs,
        };
    }

    bool beginTouch() {
        detachInterrupt(WaveshareAmoled18::System::kTouchIrqPin);
        gTouchInterruptAttached = false;
        gTouchPending.store(false);
        if (!WaveshareAmoled18::DisplayPower::resetTouchHardware())
            return false;
        TwoWire& wire = touchWire();
        if (!(Ft6336Touch::probe(wire, WaveshareAmoled18::TouchWiring::kAddress)
              && Ft6336Touch::configureMonitorMode(wire, WaveshareAmoled18::TouchWiring::kAddress)))
            return false;
        pinMode(WaveshareAmoled18::System::kTouchIrqPin, INPUT_PULLUP);
        attachInterrupt(WaveshareAmoled18::System::kTouchIrqPin, onTouchInterrupt, FALLING);
        gTouchInterruptAttached = true;
        if (!digitalRead(WaveshareAmoled18::System::kTouchIrqPin))
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
        if (!Ft6336Touch::readPacket(touchWire(), WaveshareAmoled18::TouchWiring::kAddress,
                                     WaveshareAmoled18::TouchWiring::kReleaseBusBeforeRead, data.data(), data.size())) {
            gTouchPending.store(true);
            return false;
        }

        BoardDrivers::Touch::Sample decoded = {};
        if (!Ft6336Touch::decodePacket(data.data(), data.size(), WaveshareAmoled18::DisplayWiring::kPanelWidth,
                                       WaveshareAmoled18::DisplayWiring::kPanelHeight, decoded)) {
            gTouchPending.store(true);
            return false;
        }

        contact = {decoded.touched, decoded.physicalX, decoded.physicalY};
        return true;
    }

} // namespace Board::Input
