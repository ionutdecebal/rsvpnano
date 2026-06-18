#pragma once

#include "board/BoardTypes.h"

namespace WaveshareAmoled18::Version {

constexpr const char *kBoardId = "waveshare_esp32s3_touch_amoled_1_8_v1";
constexpr const char *kBoardLabel = "Waveshare ESP32-S3-Touch-AMOLED-1.8 V1";
constexpr const char *kOtaAssetName = "rsvp-nano-esp32-s3-touch-amoled-1.8-ota.bin";

constexpr bool kPanelMemoryRotated180 = true;
constexpr Board::UiOrientation kDefaultUiOrientation = Board::UiOrientation::Landscape;
constexpr Board::UiOrientation kRotatedUiOrientation = Board::UiOrientation::LandscapeFlipped;

constexpr int kTouchIrqPin = -1;
constexpr uint8_t kTouchAddress = 0x38;

}  // namespace WaveshareAmoled18::Version
