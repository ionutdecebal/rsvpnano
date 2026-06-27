#include "drivers/touch/ft6336/ft6336.h"

#include <algorithm>

#include "board/BoardConfig.h"

namespace {

constexpr uint8_t kPointsReg = 0x02;

uint16_t clampPhysicalX(uint16_t x) {
  return std::min<uint16_t>(x, static_cast<uint16_t>(Board::Config::PANEL_NATIVE_WIDTH - 1));
}

uint16_t clampPhysicalY(uint16_t y) {
  return std::min<uint16_t>(y, static_cast<uint16_t>(Board::Config::PANEL_NATIVE_HEIGHT - 1));
}

}  // namespace

namespace Ft6336Touch {

size_t packetLength() { return 5; }

bool readPacket(TwoWire &wire, uint8_t address, uint8_t *buffer, size_t len) {
  wire.beginTransmission(address);
  wire.write(kPointsReg);
  if (wire.endTransmission(Board::Config::TOUCH_RELEASE_BUS_BEFORE_READ) != 0) {
    return false;
  }
  if (Board::Config::TOUCH_RELEASE_BUS_BEFORE_READ) {
    delayMicroseconds(50);
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

  const uint8_t points = static_cast<uint8_t>(data[0] & 0x0F);
  if (points == 0 || points > 4) {
    sample.touched = false;
    return true;
  }

  sample.touched = true;
  sample.physicalX = clampPhysicalX(static_cast<uint16_t>(((data[1] & 0x0F) << 8) | data[2]));
  sample.physicalY = clampPhysicalY(static_cast<uint16_t>(((data[3] & 0x0F) << 8) | data[4]));
  return true;
}

}  // namespace Ft6336Touch
