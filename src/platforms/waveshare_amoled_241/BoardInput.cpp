#include "board/BoardInput.h"

#include <array>
#include <atomic>

#include <Wire.h>

#include "drivers/touch/ft6336/ft6336.h"
#include "platforms/waveshare_amoled_241/BoardDisplayPower.h"
#include "platforms/waveshare_amoled_241/WaveshareAmoled241.h"

namespace {

    // Native-word loads/stores only: no atomic read-modify-write helpers in the ISR.
    std::atomic<uint32_t> gTouchPending = 0;
    bool gTouchInterruptAttached = false;
    static_assert(sizeof(gTouchPending) == sizeof(uint32_t)
                  && alignof(decltype(gTouchPending)) >= sizeof(uint32_t));

    [[maybe_unused]] void IRAM_ATTR onTouchInterrupt() {
        gTouchPending.store(true);
        ::Input::notifyTouchFromISR();
    }

    TwoWire& touchWire() {
        return Wire1;
    }

    bool primaryPressedRaw() {
        if constexpr (WaveshareAmoled241::Buttons::kBootPin < 0) {
            return false;
        }
        return !digitalRead(WaveshareAmoled241::Buttons::kBootPin);
    }

    bool powerPressedRaw() {
        if constexpr (WaveshareAmoled241::Buttons::kPowerPin < 0) {
            return false;
        }
        return !digitalRead(WaveshareAmoled241::Buttons::kPowerPin);
    }

    void configureButtonPins() {
        if constexpr (WaveshareAmoled241::Buttons::kBootPin >= 0) {
            pinMode(WaveshareAmoled241::Buttons::kBootPin, INPUT_PULLUP);
        }
        if constexpr (WaveshareAmoled241::Buttons::kPowerPin >= 0) {
            pinMode(WaveshareAmoled241::Buttons::kPowerPin, INPUT_PULLUP);
        }
    }

} // namespace

namespace Board::Input {

    bool begin() {
        configureButtonPins();
        return true;
    }

    void end() {
        if constexpr (WaveshareAmoled241::System::kTouchIrqPin >= 0) {
            if (gTouchInterruptAttached) {
                detachInterrupt(WaveshareAmoled241::System::kTouchIrqPin);
            }
        }
        gTouchInterruptAttached = false;
        gTouchPending.store(false);
    }

    void cancel() {
        // Light sleep installs a level-triggered wake source on this pin instead.
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
        return {WaveshareAmoled241::DisplayWiring::kPanelWidth, WaveshareAmoled241::DisplayWiring::kPanelHeight};
    }

    ::Input::TouchTiming touchTiming() {
        return {};
    }

    bool beginTouch() {
        end();
        if (!WaveshareAmoled241::DisplayPower::resetTouchHardware()
            || !Ft6336Touch::probe(touchWire(), WaveshareAmoled241::TouchWiring::kAddress)) {
            return false;
        }
        if constexpr (WaveshareAmoled241::System::kTouchIrqPin >= 0) {
            pinMode(WaveshareAmoled241::System::kTouchIrqPin, INPUT_PULLUP);
            // Sample once even if a contact started before the ISR was installed.
            gTouchPending.store(true);
            attachInterrupt(WaveshareAmoled241::System::kTouchIrqPin, onTouchInterrupt, FALLING);
            gTouchInterruptAttached = true;
        }
        return true;
    }

    bool touchReady() {
        if constexpr (WaveshareAmoled241::System::kTouchIrqPin < 0) {
            // V1 EXIO2 is a live input, not an event latch. Do not gate reports on it.
            return true;
        }
        return !gTouchInterruptAttached || gTouchPending.load();
    }

    bool readTouch(ui::TouchContact& contact) {
        // Clear before I2C so an IRQ arriving during the read remains pending.
        gTouchPending.store(false);
        std::array<uint8_t, Ft6336Touch::kPacketLength> data = {};
        if (!Ft6336Touch::readPacket(touchWire(), WaveshareAmoled241::TouchWiring::kAddress,
                                     WaveshareAmoled241::TouchWiring::kReleaseBusBeforeRead, data.data(),
                                     data.size())) {
            gTouchPending.store(true);
            return false;
        }

        BoardDrivers::Touch::Sample decoded = {};
        if (!Ft6336Touch::decodePacket(data.data(), data.size(), WaveshareAmoled241::DisplayWiring::kPanelWidth,
                                       WaveshareAmoled241::DisplayWiring::kPanelHeight, decoded)) {
            gTouchPending.store(true);
            return false;
        }

        contact = {decoded.touched, decoded.physicalX, decoded.physicalY};
        return true;
    }

} // namespace Board::Input
