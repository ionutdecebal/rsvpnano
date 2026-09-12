#pragma once

#include <Arduino.h>
#include <Wire.h>

#include "drivers/touch/TouchTypes.h"

namespace Cst92xxTouch {

    constexpr uint8_t kMaxTouchPoints = 2;
    // Like the BSP, read the first point and report metadata; the UI consumes one contact.
    constexpr size_t kPacketLength = 10;

    bool begin(TwoWire& wire, uint8_t address);
    bool readPacket(TwoWire& wire, uint8_t address, uint8_t* buffer, size_t len);
    bool decodePacket(const uint8_t* data, size_t len, uint16_t panelWidth, uint16_t panelHeight,
                      BoardDrivers::Touch::Sample& sample);

} // namespace Cst92xxTouch
