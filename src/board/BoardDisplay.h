#pragma once

#include <Arduino.h>

#include "board/BoardTypes.h"

namespace Board::Display {

    bool begin();
    void holdBacklightOffForDeepSleep();
    Board::UiOrientation defaultUiOrientation();
    Board::UiOrientation rotatedUiOrientation();
    uint16_t nativeWidth();
    uint16_t nativeHeight();
    size_t txChunkBytes();
    void setBacklight(bool on);
    void setBrightness(uint8_t percent);
    void sleep();
    void wake();
    bool pushColors(uint16_t x, uint16_t y, uint16_t width, uint16_t height, const uint16_t* data);

} // namespace Board::Display
