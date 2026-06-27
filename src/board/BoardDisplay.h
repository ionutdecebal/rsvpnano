#pragma once

#include <Arduino.h>

#include "board/BoardConfig.h"

namespace Board::Display {

bool begin();
void enablePowerIfAvailable();
void holdBacklightOffForDeepSleep();
uint16_t nativeWidth();
uint16_t nativeHeight();
size_t txChunkBytes();
void setBacklight(bool on);
void flashBacklight(uint8_t count, uint32_t onMs, uint32_t offMs);
void setBrightness(uint8_t percent);
void sleep();
void wake();
bool pushColors(uint16_t x, uint16_t y, uint16_t width, uint16_t height,
                const uint16_t *data);

}  // namespace Board::Display
