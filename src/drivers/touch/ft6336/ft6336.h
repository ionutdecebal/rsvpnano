#pragma once

#include <Arduino.h>
#include <Wire.h>

#include "drivers/touch/TouchTypes.h"

namespace Ft6336Touch {

constexpr size_t kPacketLength = 5;

bool probe(TwoWire &wire, uint8_t address);
bool configureMonitorMode(TwoWire &wire, uint8_t address);
bool readPacket(TwoWire &wire, uint8_t address, bool releaseBusBeforeRead, uint8_t *buffer,
                size_t len);
bool decodePacket(const uint8_t *data, size_t len, uint16_t panelWidth, uint16_t panelHeight,
                  BoardDrivers::Touch::Sample &sample);

}  // namespace Ft6336Touch
