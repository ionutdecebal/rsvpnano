#pragma once

#include <Arduino.h>

namespace BoardConfig {
// Assumed mappings for Waveshare ESP32-S3 Touch AMOLED 1.91.
// Adjust these if your specific board revision differs.
constexpr int PIN_BOOT_BUTTON = 0;

constexpr int PIN_TFT_CS = 45;
constexpr int PIN_TFT_SCLK = 47;
constexpr int PIN_TFT_MOSI = 21;
constexpr int PIN_TFT_MISO = -1;
constexpr int PIN_TFT_DC = 40;
constexpr int PIN_TFT_RST = 41;
constexpr int PIN_TFT_BL = 42;

constexpr int PIN_TOUCH_SDA = 8;
constexpr int PIN_TOUCH_SCL = 9;
constexpr int PIN_TOUCH_INT = 4;
constexpr int PIN_TOUCH_RST = 5;
constexpr uint8_t TOUCH_I2C_ADDR = 0x15; // CST816-style default

constexpr int SCREEN_W = 536;
constexpr int SCREEN_H = 240;

constexpr uint32_t RELEASE_BUFFER_MS = 200;
constexpr uint32_t PAUSE_TRANSITION_MS = 120;
constexpr uint32_t LONG_PRESS_MS = 550;
constexpr uint32_t TRIPLE_PRESS_WINDOW_MS = 850;
constexpr uint32_t DEBOUNCE_MS = 25;
} // namespace BoardConfig
