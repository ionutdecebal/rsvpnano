#pragma once

#include <Arduino.h>
#include <Wire.h>

namespace BoardDrivers::Tca9554 {

constexpr uint8_t kInputReg = 0x00;
constexpr uint8_t kOutputReg = 0x01;
constexpr uint8_t kConfigReg = 0x03;

inline bool read(TwoWire &wire, uint8_t address, uint8_t reg, uint8_t &value,
                 bool releaseBeforeRead = false) {
  wire.beginTransmission(address);
  wire.write(reg);
  if (wire.endTransmission(releaseBeforeRead) != 0) {
    return false;
  }
  if (releaseBeforeRead) {
    delayMicroseconds(50);
  }
  if (wire.requestFrom(address, static_cast<uint8_t>(1)) != 1) {
    return false;
  }

  value = wire.read();
  return true;
}

inline bool write(TwoWire &wire, uint8_t address, uint8_t reg, uint8_t value) {
  wire.beginTransmission(address);
  wire.write(reg);
  wire.write(value);
  return wire.endTransmission(true) == 0;
}

inline bool readInputPin(TwoWire &wire, uint8_t address, uint8_t pin, bool &high,
                         bool releaseBeforeRead = false) {
  uint8_t input = 0;
  if (!read(wire, address, kInputReg, input, releaseBeforeRead)) {
    return false;
  }

  high = (input & static_cast<uint8_t>(1U << pin)) != 0;
  return true;
}

inline bool configureOutputPin(TwoWire &wire, uint8_t address, uint8_t pin, bool high,
                               bool releaseBeforeRead = false) {
  uint8_t output = 0xFF;
  if (!read(wire, address, kOutputReg, output, releaseBeforeRead)) {
    return false;
  }

  const uint8_t mask = static_cast<uint8_t>(1U << pin);
  if (high) {
    output |= mask;
  } else {
    output &= static_cast<uint8_t>(~mask);
  }
  if (!write(wire, address, kOutputReg, output)) {
    return false;
  }

  uint8_t config = 0xFF;
  if (!read(wire, address, kConfigReg, config, releaseBeforeRead)) {
    return false;
  }

  config &= static_cast<uint8_t>(~mask);
  return write(wire, address, kConfigReg, config);
}

}  // namespace BoardDrivers::Tca9554
