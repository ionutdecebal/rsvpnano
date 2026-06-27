#pragma once

#include <Arduino.h>
#include <Wire.h>

namespace BoardDrivers::Qmi8658 {

bool probeAddress(TwoWire &wire, uint8_t address);
bool readRegister(TwoWire &wire, uint8_t address, uint8_t reg, uint8_t &value,
                  bool releaseBusBeforeRead);
bool writeRegister(TwoWire &wire, uint8_t address, uint8_t reg, uint8_t value);
bool readRegisters(TwoWire &wire, uint8_t address, uint8_t startReg, uint8_t *buffer, size_t len,
                   bool releaseBusBeforeRead);

}  // namespace BoardDrivers::Qmi8658
