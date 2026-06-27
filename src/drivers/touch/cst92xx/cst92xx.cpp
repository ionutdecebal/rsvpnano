#include "drivers/touch/cst92xx/cst92xx.h"

#include <algorithm>

#include "board/BoardConfig.h"

namespace {

constexpr uint16_t kReadCommand = 0xD000;
constexpr uint8_t kAck = 0xAB;
constexpr uint8_t kMaxTouchPoints = 1;
constexpr size_t kPacketLength = kMaxTouchPoints * 5 + 5;

uint16_t clampPhysicalX(uint16_t x) {
  return std::min<uint16_t>(x, static_cast<uint16_t>(Board::Config::PANEL_NATIVE_WIDTH - 1));
}

uint16_t clampPhysicalY(uint16_t y) {
  return std::min<uint16_t>(y, static_cast<uint16_t>(Board::Config::PANEL_NATIVE_HEIGHT - 1));
}

}  // namespace

namespace Cst92xxTouch {

size_t packetLength() { return kPacketLength; }

bool readPacket(TwoWire &wire, uint8_t address, uint8_t *buffer, size_t len) {
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
    const size_t readLen =
        wire.requestFrom(static_cast<uint8_t>(address), static_cast<size_t>(len), true);
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

bool decodePacket(const uint8_t *data, size_t len, BoardDrivers::Touch::Sample &sample) {
  if (data == nullptr || len < packetLength()) {
    return false;
  }

  if (data[0] == kAck || data[6] != kAck) {
    sample.touched = false;
    return true;
  }

  const uint8_t points = static_cast<uint8_t>(data[5] & 0x7F);
  const uint8_t touchId = static_cast<uint8_t>(data[0] >> 4);
  const uint8_t event = static_cast<uint8_t>(data[0] & 0x0F);
  if (points == 0 || points > kMaxTouchPoints || event != 0x06 || touchId >= kMaxTouchPoints) {
    sample.touched = false;
    return true;
  }

  sample.touched = true;
  sample.physicalX = clampPhysicalX(static_cast<uint16_t>((data[1] << 4) | (data[3] >> 4)));
  sample.physicalY = clampPhysicalY(static_cast<uint16_t>((data[2] << 4) | (data[3] & 0x0F)));
  return true;
}

}  // namespace Cst92xxTouch
