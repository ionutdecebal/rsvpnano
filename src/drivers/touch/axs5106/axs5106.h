#pragma once

#include <Arduino.h>
#include <Wire.h>

#include "drivers/touch/TouchTypes.h"

namespace Axs5106Touch {

constexpr size_t kPacketLength = 14;

bool probe(TwoWire &wire, uint8_t address);
bool readPacket(TwoWire &wire, uint8_t address, bool releaseBusBeforeRead, uint8_t *buffer,
                size_t len);
bool decodePacket(const uint8_t *data, size_t len, uint16_t panelWidth, uint16_t panelHeight,
                  BoardDrivers::Touch::Sample &sample);

}  // namespace Axs5106Touch
