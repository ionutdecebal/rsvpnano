#include "drivers/touch/cst92xx/cst92xx.h"

#include <algorithm>

namespace {

constexpr uint16_t kReadCommand = 0xD000;
constexpr uint8_t kAck = 0xAB;
constexpr uint8_t kMonitorModeRegister = 0xFA;
constexpr uint8_t kMonitorModeValue = 0x40;

bool validAddress(uint8_t address) { return address <= 0x7F; }

uint16_t clampPhysical(uint16_t value, uint16_t limit) {
  return limit == 0 ? 0 : std::min<uint16_t>(value, static_cast<uint16_t>(limit - 1));
}

}  // namespace

namespace Cst92xxTouch {

bool probe(TwoWire &wire, uint8_t address) {
  if (!validAddress(address)) {
    return false;
  }

  wire.beginTransmission(address);
  return wire.endTransmission() == 0;
}

bool configureMonitorMode(TwoWire &wire, uint8_t address) {
  if (!validAddress(address)) {
    return false;
  }

  wire.beginTransmission(address);
  wire.write(kMonitorModeRegister);
  wire.write(kMonitorModeValue);
  return wire.endTransmission(true) == 0;
}

bool readPacket(TwoWire &wire, uint8_t address, uint8_t *buffer, size_t len) {
  if (!validAddress(address) || buffer == nullptr || len < kPacketLength) {
    return false;
  }

  const uint8_t readCommand[] = {highByte(kReadCommand), lowByte(kReadCommand)};
  constexpr uint8_t kMaxRetries = 5;

  for (uint8_t retry = 0; retry < kMaxRetries; ++retry) {
    wire.beginTransmission(address);
    wire.write(readCommand, sizeof(readCommand));
    if (wire.endTransmission(true) != 0) {
      delay(3);
      continue;
    }

    delay(2);
    const size_t readLen = wire.requestFrom(address, static_cast<size_t>(len), true);
    if (readLen == len) {
      for (size_t i = 0; i < len; ++i) {
        buffer[i] = wire.read();
      }
      return true;
    }
    while (wire.available() > 0) {
      wire.read();
    }
    delay(3);
  }

  return false;
}

bool decodePacket(const uint8_t *data, size_t len, uint16_t panelWidth, uint16_t panelHeight,
                  BoardDrivers::Touch::Sample &sample) {
  if (data == nullptr || len < kPacketLength) {
    return false;
  }

  if (data[0] == kAck || data[6] != kAck) {
    sample.touched = false;
    return true;
  }

  const uint8_t points = data[5] & 0x7F;
  const uint8_t touchId = data[0] >> 4;
  const uint8_t event = data[0] & 0x0F;
  if (points == 0 || points > kMaxTouchPoints || event != 0x06 || touchId >= kMaxTouchPoints) {
    sample.touched = false;
    return true;
  }

  sample.touched = true;
  sample.physicalX =
      clampPhysical(static_cast<uint16_t>((data[1] << 4) | (data[3] >> 4)), panelWidth);
  sample.physicalY =
      clampPhysical(static_cast<uint16_t>((data[2] << 4) | (data[3] & 0x0F)), panelHeight);
  return true;
}

}  // namespace Cst92xxTouch
