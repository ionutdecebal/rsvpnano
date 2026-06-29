#include "platforms/waveshare_amoled_18/BoardDisplayPower.h"

#include <Arduino.h>
#include <Wire.h>

#include "drivers/gpio/tca9554/Tca9554.h"
#include "platforms/waveshare_amoled_18/WaveshareAmoled18.h"

namespace WaveshareAmoled18::DisplayPower {

void releaseHardware() {
  BoardDrivers::Tca9554::PortState state = {};
  if (!BoardDrivers::Tca9554::readPortState(Wire1, Tca9554Wiring::kAddress, state,
                                            Tca9554Wiring::kReleaseBusBeforeRead)) {
    Serial.println("[board] TCA9554 not detected");
    return;
  }

  state.output &= Tca9554Wiring::kDisplayClearMask;
  state.config &= Tca9554Wiring::kOutputClearMask;
  state.config |= Tca9554Wiring::kInputMask;
  if (!BoardDrivers::Tca9554::writePortState(Wire1, Tca9554Wiring::kAddress, state)) {
    Serial.println("[board] TCA9554 display hold failed");
    return;
  }

  delay(20);
  state.output |= Tca9554Wiring::kDisplayMask;
  if (!BoardDrivers::Tca9554::writeOutput(Wire1, Tca9554Wiring::kAddress, state.output)) {
    Serial.println("[board] TCA9554 display release failed");
    return;
  }
  delay(50);
}

}  // namespace WaveshareAmoled18::DisplayPower
