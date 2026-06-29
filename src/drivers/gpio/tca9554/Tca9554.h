#pragma once

#include <Arduino.h>
#include <Wire.h>

namespace BoardDrivers::Tca9554 {

struct PortState {
  uint8_t output = 0xFF;
  uint8_t config = 0xFF;
};

bool readPortState(TwoWire &wire, uint8_t address, PortState &state,
                   bool releaseBeforeRead = false);
bool writePortState(TwoWire &wire, uint8_t address, const PortState &state);
bool writeOutput(TwoWire &wire, uint8_t address, uint8_t output);
bool readInputPin(TwoWire &wire, uint8_t address, uint8_t pin, bool &high,
                  bool releaseBeforeRead = false);
bool configureOutputPin(TwoWire &wire, uint8_t address, uint8_t pin, bool high,
                        bool releaseBeforeRead = false);

}  // namespace BoardDrivers::Tca9554
