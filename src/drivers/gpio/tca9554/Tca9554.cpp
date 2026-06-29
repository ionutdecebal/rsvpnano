#include "drivers/gpio/tca9554/Tca9554.h"

namespace BoardDrivers::Tca9554 {

namespace {

constexpr size_t kSingleByte = 1;
constexpr uint8_t kMaxPin = 7;
constexpr uint8_t kInputReg = 0x00;
constexpr uint8_t kOutputReg = 0x01;
constexpr uint8_t kConfigReg = 0x03;

bool readRegister(TwoWire &wire, uint8_t address, uint8_t reg, uint8_t &value,
                  bool releaseBeforeRead) {
  if (address > 0x7F) {
    return false;
  }

  wire.beginTransmission(address);
  wire.write(reg);
  if (wire.endTransmission(releaseBeforeRead) != 0) {
    return false;
  }
  if (releaseBeforeRead) {
    delayMicroseconds(50);
  }
  if (wire.requestFrom(address, kSingleByte) != kSingleByte) {
    return false;
  }

  value = wire.read();
  return true;
}

bool writeRegister(TwoWire &wire, uint8_t address, uint8_t reg, uint8_t value) {
  if (address > 0x7F) {
    return false;
  }

  wire.beginTransmission(address);
  wire.write(reg);
  wire.write(value);
  return wire.endTransmission(true) == 0;
}

}  // namespace

bool readPortState(TwoWire &wire, uint8_t address, PortState &state, bool releaseBeforeRead) {
  return readRegister(wire, address, kOutputReg, state.output, releaseBeforeRead) &&
         readRegister(wire, address, kConfigReg, state.config, releaseBeforeRead);
}

bool writePortState(TwoWire &wire, uint8_t address, const PortState &state) {
  return writeRegister(wire, address, kOutputReg, state.output) &&
         writeRegister(wire, address, kConfigReg, state.config);
}

bool writeOutput(TwoWire &wire, uint8_t address, uint8_t output) {
  return writeRegister(wire, address, kOutputReg, output);
}

bool readInputPin(TwoWire &wire, uint8_t address, uint8_t pin, bool &high,
                  bool releaseBeforeRead) {
  if (pin > kMaxPin) {
    return false;
  }

  uint8_t input = 0;
  if (!readRegister(wire, address, kInputReg, input, releaseBeforeRead)) {
    return false;
  }

  const uint8_t mask = 1U << pin;
  high = (input & mask) != 0;
  return true;
}

bool configureOutputPin(TwoWire &wire, uint8_t address, uint8_t pin, bool high,
                        bool releaseBeforeRead) {
  if (pin > kMaxPin) {
    return false;
  }

  uint8_t output = 0xFF;
  if (!readRegister(wire, address, kOutputReg, output, releaseBeforeRead)) {
    return false;
  }

  const uint8_t mask = 1U << pin;
  const uint8_t clearMask = 0xFFU ^ mask;
  if (high) {
    output |= mask;
  } else {
    output &= clearMask;
  }
  if (!writeRegister(wire, address, kOutputReg, output)) {
    return false;
  }

  uint8_t config = 0xFF;
  if (!readRegister(wire, address, kConfigReg, config, releaseBeforeRead)) {
    return false;
  }

  config &= clearMask;
  return writeRegister(wire, address, kConfigReg, config);
}

}  // namespace BoardDrivers::Tca9554
