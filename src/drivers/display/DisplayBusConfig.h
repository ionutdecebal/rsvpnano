#pragma once

#include <stddef.h>
#include <stdint.h>

namespace BoardDrivers::Display {

    struct BusConfig {
        int csPin = -1;
        int sclkPin = -1;
        int data0Pin = -1;
        int data1Pin = -1;
        int data2Pin = -1;
        int data3Pin = -1;
        int resetPin = -1;
        int backlightPin = -1;
        uint16_t panelWidth = 0;
        uint16_t panelHeight = 0;
        size_t txChunkBytes = 0;
        bool uiRotated180 = false;
        uint16_t columnOffset = 0;
        uint16_t rowOffset = 0;
    };

} // namespace BoardDrivers::Display
