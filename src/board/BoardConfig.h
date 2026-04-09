#pragma once

#include <Arduino.h>

namespace BoardConfig {

// TODO: Verify these pins against the exact Waveshare ESP32-S3 AMOLED 1.91" schematic.
constexpr int PIN_BOOT_BUTTON = 0;
constexpr int PIN_SD_CS = 10;
constexpr int PIN_I2C_SDA = 8;
constexpr int PIN_I2C_SCL = 9;

void begin();

}  // namespace BoardConfig
