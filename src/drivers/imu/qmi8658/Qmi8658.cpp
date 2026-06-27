#include "drivers/imu/qmi8658/Qmi8658.h"

namespace BoardDrivers::Qmi8658 {
namespace {

constexpr size_t kSingleByte = 1;
constexpr size_t kMaxI2cReadBytes = 32;

bool validAddress(uint8_t address) { return address <= 0x7F; }

void waitAfterWriteIfNeeded(bool releaseBusBeforeRead) {
  if (releaseBusBeforeRead) {
    delayMicroseconds(50);
  }
}

}  // namespace

bool probeAddress(TwoWire &wire, uint8_t address) {
  if (!validAddress(address)) {
    return false;
  }

  wire.beginTransmission(address);
  return wire.endTransmission(true) == 0;
}

bool readRegister(TwoWire &wire, uint8_t address, uint8_t reg, uint8_t &value,
                  bool releaseBusBeforeRead) {
  if (!validAddress(address)) {
    return false;
  }

  wire.beginTransmission(address);
  wire.write(reg);
  if (wire.endTransmission(releaseBusBeforeRead) != 0) {
    return false;
  }
  waitAfterWriteIfNeeded(releaseBusBeforeRead);
  if (wire.requestFrom(address, kSingleByte, true) != kSingleByte) {
    return false;
  }

  value = wire.read();
  return true;
}

bool writeRegister(TwoWire &wire, uint8_t address, uint8_t reg, uint8_t value) {
  if (!validAddress(address)) {
    return false;
  }

  wire.beginTransmission(address);
  wire.write(reg);
  wire.write(value);
  return wire.endTransmission(true) == 0;
}

bool readRegisters(TwoWire &wire, uint8_t address, uint8_t startReg, uint8_t *buffer, size_t len,
                   bool releaseBusBeforeRead) {
  if (!validAddress(address) || buffer == nullptr || len == 0 || len > kMaxI2cReadBytes) {
    return false;
  }

  wire.beginTransmission(address);
  wire.write(startReg);
  if (wire.endTransmission(releaseBusBeforeRead) != 0) {
    return false;
  }
  waitAfterWriteIfNeeded(releaseBusBeforeRead);
  if (wire.requestFrom(address, len, true) != len) {
    return false;
  }

  for (size_t i = 0; i < len; ++i) {
    buffer[i] = wire.read();
  }
  return true;
}

}  // namespace BoardDrivers::Qmi8658
