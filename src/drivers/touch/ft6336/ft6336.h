#pragma once

#include <Arduino.h>
#include <Wire.h>

#include "drivers/touch/TouchTypes.h"

namespace Ft6336Touch {

size_t packetLength();
bool readPacket(TwoWire &wire, uint8_t address, uint8_t *buffer, size_t len);
bool decodePacket(const uint8_t *data, size_t len, BoardDrivers::Touch::Sample &sample);

}  // namespace Ft6336Touch
