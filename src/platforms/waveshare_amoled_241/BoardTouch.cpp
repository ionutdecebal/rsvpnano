#include "board/BoardSystem.h"
#include "board/BoardTouch.h"

#include "drivers/touch/ft6336/ft6336.h"

namespace Board::Touch {

TwoWire &wire() { return Wire1; }

void resetController() { Board::System::resetTouchController(); }

bool ready() { return Board::Config::PIN_TOUCH_IRQ < 0 || !digitalRead(Board::Config::PIN_TOUCH_IRQ); }

bool configure() { return true; }

size_t packetLength() { return Ft6336Touch::packetLength(); }

bool readPacket(uint8_t *buffer, size_t len) {
  return Ft6336Touch::readPacket(wire(), Board::Config::TOUCH_I2C_ADDRESS, buffer, len);
}

bool decodePacket(const uint8_t *data, size_t len, BoardDrivers::Touch::Sample &sample) {
  return Ft6336Touch::decodePacket(data, len, sample);
}

}  // namespace Board::Touch
