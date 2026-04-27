#pragma once

#include <Arduino.h>

namespace Panel {

void init();
void setBacklight(bool on);
void setBrightnessPercent(uint8_t percent);
void sleep();
void wake();
void pushColors(uint16_t x, uint16_t y, uint16_t width, uint16_t height, const uint16_t *data);

}  // namespace Panel
