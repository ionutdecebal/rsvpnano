#pragma once

#include <Arduino.h>

void jd9853Init();
void jd9853SetBacklight(bool on);
void jd9853SetBrightnessPercent(uint8_t percent);
void jd9853Sleep();
void jd9853Wake();
void jd9853PushColors(uint16_t x, uint16_t y, uint16_t width, uint16_t height,
                      const uint16_t *data);
