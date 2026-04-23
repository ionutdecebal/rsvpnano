#pragma once

#include <Arduino.h>

void axs15231bInit();
void axs15231bSetBacklight(bool on);
void axs15231bSetBrightnessPercent(uint8_t percent);
void axs15231bSleep();
void axs15231bWake();
void axs15231bPushColors(uint16_t x, uint16_t y, uint16_t width, uint16_t height,
                         const uint16_t *data);
