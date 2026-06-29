#include "drivers/touch/axs15231b_touch/axs15231b_touch.h"

#include <algorithm>

namespace {

constexpr uint8_t kReadTouchCommand[] = {
    0xB5, 0xAB, 0xA5, 0x5A, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00,
};

bool validAddress(uint8_t address) { return address <= 0x7F; }

uint16_t clampPhysical(uint16_t value, uint16_t limit) {
  return limit == 0 ? 0 : std::min<uint16_t>(value, static_cast<uint16_t>(limit - 1));
}

}  // namespace

namespace Axs15231bTouch {

bool probe(TwoWire &wire, uint8_t address) {
  if (!validAddress(address)) {
    return false;
  }

  wire.beginTransmission(address);
  return wire.endTransmission() == 0;
}

bool readPacket(TwoWire &wire, uint8_t address, uint8_t *buffer, size_t len) {
  if (!validAddress(address) || buffer == nullptr || len < kPacketLength) {
    return false;
  }

  wire.beginTransmission(address);
  wire.write(kReadTouchCommand, sizeof(kReadTouchCommand));
  if (wire.endTransmission(false) != 0) {
    return false;
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

  const uint8_t points = data[1];
  if (points == 0 || points > 4) {
    sample.touched = false;
    return true;
  }

  const uint16_t rawLongAxis = static_cast<uint16_t>(((data[2] & 0x0F) << 8) | data[3]);
  const uint16_t rawShortAxis = static_cast<uint16_t>(((data[4] & 0x0F) << 8) | data[5]);
  sample.touched = true;
  sample.physicalX = clampPhysical(rawShortAxis, panelWidth);
  sample.physicalY = clampPhysical(
      rawLongAxis >= panelHeight ? 0 : static_cast<uint16_t>(panelHeight - 1 - rawLongAxis),
      panelHeight);
  return true;
}

}  // namespace Axs15231bTouch
