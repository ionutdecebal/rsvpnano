#include "platforms/waveshare_amoled_241/BoardDisplayPower.h"

#include <Wire.h>

#include "drivers/gpio/tca9554/Tca9554.h"
#include "platforms/waveshare_amoled_241/WaveshareAmoled241.h"

namespace WaveshareAmoled241::DisplayPower {

    namespace {

        bool setExpanderOutput(uint8_t pin, bool high) {
            return BoardDrivers::Tca9554::configureOutputPin(Wire1, Tca9554Wiring::kAddress, pin, high,
                                                            Tca9554Wiring::kReleaseBusBeforeRead);
        }

        [[maybe_unused]] bool resetExpanderPin(uint8_t pin) {
            // Waveshare V2 example: high 20 ms, low 20 ms, then high 120 ms.
            // Each edge reads the current latch so other expander outputs survive a reset.
            if (!setExpanderOutput(pin, true))
                return false;
            delay(20);
            if (!setExpanderOutput(pin, false))
                return false;
            delay(20);
            if (!setExpanderOutput(pin, true))
                return false;
            delay(120);
            return true;
        }

    } // namespace

    bool releaseHardware() {
        if constexpr (Tca9554Wiring::kDisplayRailEnablePin >= 0) {
            if (!setExpanderOutput(Tca9554Wiring::kDisplayRailEnablePin, true))
                return false;
            delay(25);
        }
        if constexpr (Tca9554Wiring::kDisplayResetPin >= 0) {
            return resetExpanderPin(Tca9554Wiring::kDisplayResetPin);
        }
        return true;
    }

    bool resetTouchHardware() {
        if constexpr (Tca9554Wiring::kTouchIrqPin >= 0) {
            // V1 TP_INT must remain an input, even though it cannot wake the MCU.
            BoardDrivers::Tca9554::PortState state;
            if (!BoardDrivers::Tca9554::readPortState(Wire1, Tca9554Wiring::kAddress, state,
                                                     Tca9554Wiring::kReleaseBusBeforeRead))
                return false;
            state.config |= 1U << Tca9554Wiring::kTouchIrqPin;
            if (!BoardDrivers::Tca9554::writePortState(Wire1, Tca9554Wiring::kAddress, state))
                return false;
        }
        if constexpr (System::kTouchResetPin >= 0) {
            pinMode(System::kTouchResetPin, OUTPUT);
            digitalWrite(System::kTouchResetPin, LOW);
            delay(12);
            digitalWrite(System::kTouchResetPin, HIGH);
            delay(12);
        }
        if constexpr (Tca9554Wiring::kTouchResetPin >= 0) {
            return resetExpanderPin(Tca9554Wiring::kTouchResetPin);
        }
        return true;
    }

} // namespace WaveshareAmoled241::DisplayPower
