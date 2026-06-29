#pragma once

#include <driver/gpio.h>

namespace WaveshareLcd349::Revision {

constexpr const char *kBoardId = "waveshare_esp32s3_touch_lcd_3_49_rev2";
constexpr const char *kBoardLabel = "Waveshare ESP32-S3-Touch-LCD-3.49 Rev2";
constexpr const char *kOtaAssetName = "rsvp-nano-esp32-s3-touch-lcd-3.49-rev2-ota.bin";
constexpr int kBacklightPin = 42;
constexpr gpio_num_t kBacklightGpio = GPIO_NUM_42;

}  // namespace WaveshareLcd349::Revision
