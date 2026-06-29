#pragma once

#include <Arduino.h>
#include <driver/gpio.h>

#include "board/BoardTypes.h"

namespace WaveshareAmoled206::AudioWiring {
constexpr uint8_t kEs8311Address = 0x18;
constexpr int kAudioEnablePin = 46;
constexpr int kMclkPin = 16;
constexpr int kBclkPin = 41;
constexpr int kWsPin = 45;
constexpr int kDinPin = 42;
constexpr int kDoutPin = 40;
}

namespace WaveshareAmoled206::Buttons {
constexpr int kBootPin = 0;
constexpr int kPowerPin = -1;
constexpr int kKeyPin = -1;
constexpr uint16_t kDebounceMs = 25;
constexpr uint16_t kShortPressMaxMs = 700;
constexpr uint16_t kLongPressMs = 900;
}

namespace WaveshareAmoled206::Axp2101Wiring {
constexpr bool kReleaseBusBeforeRead = false;
constexpr bool kEnablePowerKeyIrqs = true;
constexpr bool kRequiresPowerKeyConfig = false;
constexpr uint8_t kPowerKeyOnTimeValue = 0x00;
constexpr uint8_t kPowerKeyOffTimeValue = 0x00;
}

namespace WaveshareAmoled206::DisplayWiring {
constexpr int kCsPin = 12;
constexpr int kSclkPin = 11;
constexpr int kData0Pin = 4;
constexpr int kData1Pin = 5;
constexpr int kData2Pin = 6;
constexpr int kData3Pin = 7;
constexpr int kResetPin = 8;
constexpr int kBacklightPin = -1;
constexpr uint16_t kPanelWidth = 410;
constexpr uint16_t kPanelHeight = 502;
constexpr uint16_t kColumnOffset = 22;
constexpr uint16_t kRowOffset = 0;
constexpr size_t kTxChunkBytes = 32 * 1024;
constexpr bool kPanelMemoryRotated180 = false;
constexpr Board::UiOrientation kDefaultUiOrientation = Board::UiOrientation::Landscape;
}

namespace WaveshareAmoled206::ImuWiring {
constexpr uint8_t kAddress = 0x6B;
constexpr bool kReleaseBusBeforeRead = true;
}

namespace WaveshareAmoled206::Power {
constexpr bool kRequestPmuShutdownOnPowerOff = true;
constexpr bool kReleaseBatteryHoldBeforeDeepSleep = false;
}

namespace WaveshareAmoled206::Storage {
constexpr gpio_num_t kSdClockPin = GPIO_NUM_2;
constexpr gpio_num_t kSdCommandPin = GPIO_NUM_1;
constexpr gpio_num_t kSdData0Pin = GPIO_NUM_3;
constexpr gpio_num_t kSdData1Pin = GPIO_NUM_NC;
constexpr gpio_num_t kSdData2Pin = GPIO_NUM_NC;
constexpr gpio_num_t kSdData3Pin = GPIO_NUM_NC;
}

namespace WaveshareAmoled206::System {
constexpr int kSystemI2cSdaPin = 15;
constexpr int kSystemI2cSclPin = 14;
constexpr uint32_t kSystemI2cClockHz = 400000;
constexpr uint32_t kSystemI2cTimeoutMs = 10;
constexpr int kTouchSdaPin = 15;
constexpr int kTouchSclPin = 14;
constexpr int kTouchResetPin = 9;
constexpr int kTouchIrqPin = 38;
constexpr uint32_t kTouchI2cClockHz = kSystemI2cClockHz;
constexpr uint32_t kTouchI2cTimeoutMs = kSystemI2cTimeoutMs;
constexpr int kDeepSleepWakePin = Buttons::kBootPin;
constexpr gpio_num_t kDeepSleepWakeGpio = GPIO_NUM_0;
}

namespace WaveshareAmoled206::TouchWiring {
constexpr uint8_t kAddress = 0x38;
constexpr bool kReleaseBusBeforeRead = false;
constexpr uint8_t kReleaseConfirmSamples = 2;
constexpr uint8_t kMaxConsecutiveReadFailures = 5;
constexpr uint32_t kPollIntervalMs = 20;
constexpr uint32_t kFailureBackoffMs = 250;
constexpr uint32_t kRecoveryRetryMs = 1000;
constexpr uint32_t kRecoveryEventIgnoreMs = 0;
}
