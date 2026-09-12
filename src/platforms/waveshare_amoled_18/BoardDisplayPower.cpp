#include "platforms/waveshare_amoled_18/BoardDisplayPower.h"
#include <esp_log.h>

#include <Arduino.h>
#include <Wire.h>

#include "drivers/gpio/tca9554/Tca9554.h"
#include "platforms/waveshare_amoled_18/WaveshareAmoled18.h"

namespace WaveshareAmoled18::DisplayPower {

    bool releaseHardware() {
        BoardDrivers::Tca9554::PortState state = {};
        if (!BoardDrivers::Tca9554::readPortState(Wire, Tca9554Wiring::kAddress, state,
                                                  Tca9554Wiring::kReleaseBusBeforeRead)) {
            ESP_LOGW("board", "TCA9554 not detected");
            return false;
        }

        state.output &= Tca9554Wiring::kDisplayClearMask;
        state.config &= Tca9554Wiring::kOutputClearMask;
        state.config |= Tca9554Wiring::kInputMask;
        if (!BoardDrivers::Tca9554::writePortState(Wire, Tca9554Wiring::kAddress, state)) {
            ESP_LOGE("board", "TCA9554 display hold failed");
            return false;
        }

        delay(20);
        state.output |= Tca9554Wiring::kDisplayMask;
        if (!BoardDrivers::Tca9554::writeOutput(Wire, Tca9554Wiring::kAddress, state.output)) {
            ESP_LOGE("board", "TCA9554 display release failed");
            return false;
        }
        delay(50);
        return true;
    }

    bool resetTouchHardware() {
        // Startup is complete before the input task starts. Recovery is the only
        // runtime expander writer; re-read the latch on each edge to preserve it.
        if (!BoardDrivers::Tca9554::configureOutputPin(Wire, Tca9554Wiring::kAddress, Tca9554Wiring::kTouchResetPin,
                                                       false, Tca9554Wiring::kReleaseBusBeforeRead))
            return false;
        delay(20);
        if (!BoardDrivers::Tca9554::configureOutputPin(Wire, Tca9554Wiring::kAddress, Tca9554Wiring::kTouchResetPin,
                                                       true, Tca9554Wiring::kReleaseBusBeforeRead))
            return false;
        delay(50);
        return true;
    }

} // namespace WaveshareAmoled18::DisplayPower
