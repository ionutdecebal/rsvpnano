#pragma once

#include <Arduino.h>
#include <Wire.h>

#include "board/BoardConfig.h"
#include "drivers/touch/TouchTypes.h"

namespace Board::Touch {

TwoWire &wire();
void resetController();
bool ready();
bool configure();
size_t packetLength();
bool readPacket(uint8_t *buffer, size_t len);
bool decodePacket(const uint8_t *data, size_t len, BoardDrivers::Touch::Sample &sample);

}  // namespace Board::Touch
