#include "drivers/touch/axs15231b_touch/axs15231b_touch.h"

#include <algorithm>

#include "board/BoardConfig.h"

namespace {

constexpr uint8_t kReadTouchCommand[] = {
    0xB5, 0xAB, 0xA5, 0x5A, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00,
};

uint16_t clampPhysicalX(uint16_t x) {
  return std::min<uint16_t>(x, static_cast<uint16_t>(Board::Config::PANEL_NATIVE_WIDTH - 1));
}

uint16_t clampPhysicalY(uint16_t y) {
  return std::min<uint16_t>(y, static_cast<uint16_t>(Board::Config::PANEL_NATIVE_HEIGHT - 1));
}

}  // namespace

namespace Axs15231bTouch {

size_t packetLength() { return 8; }

bool readPacket(TwoWire &wire, uint8_t address, uint8_t *buffer, size_t len) {
  wire.beginTransmission(address);
  wire.write(kReadTouchCommand, sizeof(kReadTouchCommand));
  if (wire.endTransmission(false) != 0) {
    return false;
  }

  const size_t readLen =
      wire.requestFrom(static_cast<uint8_t>(address), static_cast<size_t>(len), true);
  if (readLen != len) {
    return false;
  }

  for (size_t i = 0; i < len; ++i) {
    buffer[i] = wire.read();
  }
  return true;
}

bool decodePacket(const uint8_t *data, size_t len, BoardDrivers::Touch::Sample &sample) {
  if (data == nullptr || len < packetLength()) {
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
  sample.physicalX = clampPhysicalX(rawShortAxis);
  sample.physicalY =
      clampPhysicalY(rawLongAxis >= Board::Config::PANEL_NATIVE_HEIGHT
                         ? 0
                         : static_cast<uint16_t>(Board::Config::PANEL_NATIVE_HEIGHT - 1 -
                                                 rawLongAxis));
  return true;
}

}  // namespace Axs15231bTouch
