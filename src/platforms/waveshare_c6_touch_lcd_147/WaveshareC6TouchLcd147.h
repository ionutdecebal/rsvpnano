#pragma once

#include <Arduino.h>
#include <driver/gpio.h>

#include "ui/Touch.h"

namespace WaveshareC6TouchLcd147::Buttons {
    constexpr int kBootPin = 9; // Schematic BOOT net; GPIO8 is a separate header pin.
    constexpr int kPowerPin = -1;
    constexpr int kKeyPin = -1;
} // namespace WaveshareC6TouchLcd147::Buttons

namespace WaveshareC6TouchLcd147::DisplayWiring {
    constexpr int kCsPin = 14;
    constexpr int kDcPin = 15;
    constexpr int kSclkPin = 1;
    constexpr int kMosiPin = 2;
    constexpr int kMisoPin = 3;
    constexpr int kResetPin = 22;
    constexpr int kBacklightPin = 23;
    constexpr uint16_t kPanelWidth = 172;
    constexpr uint16_t kPanelHeight = 320;
    constexpr uint16_t kColumnOffset = 34;
    constexpr uint16_t kRowOffset = 0;
    constexpr size_t kTxChunkBytes = 16 * 1024;
    constexpr ui::Orientation kDefaultUiOrientation = ui::Orientation::Landscape;
} // namespace WaveshareC6TouchLcd147::DisplayWiring

namespace WaveshareC6TouchLcd147::ImuWiring {
    constexpr uint8_t kAddress = 0x6B;
    constexpr bool kReleaseBusBeforeRead = false;
} // namespace WaveshareC6TouchLcd147::ImuWiring

namespace WaveshareC6TouchLcd147::Power {
    constexpr int kBatteryAdcPin = 0;
    constexpr float kBatteryDividerScale = 0.003f;
} // namespace WaveshareC6TouchLcd147::Power

namespace WaveshareC6TouchLcd147::Storage {
    constexpr int kSclkPin = DisplayWiring::kSclkPin;
    constexpr int kMosiPin = DisplayWiring::kMosiPin;
    constexpr int kMisoPin = DisplayWiring::kMisoPin;
    constexpr int kCsPin = 4;
    constexpr uint32_t kDefaultFrequencyHz = 20000000;
} // namespace WaveshareC6TouchLcd147::Storage

namespace WaveshareC6TouchLcd147::System {
    constexpr int kSystemI2cSdaPin = 18;
    constexpr int kSystemI2cSclPin = 19;
    constexpr uint32_t kSystemI2cClockHz = 400000;
    constexpr uint32_t kSystemI2cTimeoutMs = 10;
    constexpr int kTouchResetPin = 20;
    constexpr int kTouchIrqPin = 21;
    constexpr uint32_t kTouchI2cClockHz = kSystemI2cClockHz;
    constexpr uint32_t kTouchI2cTimeoutMs = kSystemI2cTimeoutMs;
    constexpr gpio_num_t kLightSleepWakeGpio = static_cast<gpio_num_t>(Buttons::kBootPin);
} // namespace WaveshareC6TouchLcd147::System

namespace WaveshareC6TouchLcd147::TouchWiring {
    constexpr uint8_t kAddress = 0x63;
    constexpr bool kReleaseBusBeforeRead = true;
} // namespace WaveshareC6TouchLcd147::TouchWiring
