#include "drivers/touch/cst816/cst816.h"

#include <algorithm>

namespace {

constexpr uint8_t kFingerCountRegister = 0x02;
constexpr uint8_t kInterruptModeRegister = 0xFA;
constexpr uint8_t kPeriodicInterruptValue = 0x40;

bool validAddress(uint8_t address) { return address <= 0x7F; }

uint16_t clampPhysical(uint16_t value, uint16_t limit) {
  return limit == 0 ? 0 : std::min<uint16_t>(value, static_cast<uint16_t>(limit - 1));
}

}  // namespace

namespace Cst816Touch {

bool probe(TwoWire &wire, uint8_t address) {
  if (!validAddress(address)) {
    return false;
  }

  wire.beginTransmission(address);
  return wire.endTransmission() == 0;
}

bool configurePeriodicInterrupt(TwoWire &wire, uint8_t address) {
  if (!validAddress(address)) {
    return false;
  }

  wire.beginTransmission(address);
  wire.write(kInterruptModeRegister);
  wire.write(kPeriodicInterruptValue);
  return wire.endTransmission(true) == 0;
}

bool readPacket(TwoWire &wire, uint8_t address, bool releaseBusBeforeRead, uint8_t *buffer,
                size_t len) {
  if (!validAddress(address) || buffer == nullptr || len < kPacketLength) {
    return false;
  }

  wire.beginTransmission(address);
  wire.write(kFingerCountRegister);
  if (wire.endTransmission(releaseBusBeforeRead) != 0) {
    return false;
  }
  if (releaseBusBeforeRead) {
    delayMicroseconds(50);
  }

  const size_t readLen = wire.requestFrom(address, static_cast<size_t>(len), true);
  if (readLen != len) {
    return false;
  }

  for (size_t i = 0; i < len; ++i) {
    buffer[i] = wire.read();
  }
  return true;
}

bool decodePacket(const uint8_t *data, size_t len, uint16_t panelWidth, uint16_t panelHeight,
                  BoardDrivers::Touch::Sample &sample) {
  if (data == nullptr || len < kPacketLength) {
    return false;
  }

  const uint8_t points = data[0] & 0x0F;
  if (points == 0 || points > 2) {
    sample.touched = false;
    return true;
  }

  sample.touched = true;
  sample.physicalX =
      clampPhysical(static_cast<uint16_t>(((data[1] & 0x0F) << 8) | data[2]), panelWidth);
  sample.physicalY =
      clampPhysical(static_cast<uint16_t>(((data[3] & 0x0F) << 8) | data[4]), panelHeight);
  return true;
}

}  // namespace Cst816Touch
