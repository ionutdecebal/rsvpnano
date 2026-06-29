#pragma once

#include <Arduino.h>
#include <driver/gpio.h>

#include "board/BoardTypes.h"

namespace WaveshareAmoled241::ImuWiring {
constexpr uint8_t kAddress = 0x6B;
constexpr bool kReleaseBusBeforeRead = false;
}

namespace WaveshareAmoled241::Buttons {
constexpr int kBootPin = 0;
constexpr int kPowerPin = 15;
constexpr int kKeyPin = -1;
constexpr uint16_t kDebounceMs = 25;
constexpr uint16_t kShortPressMaxMs = 700;
constexpr uint16_t kLongPressMs = 900;
}

namespace WaveshareAmoled241::DisplayWiring {
constexpr int kCsPin = 9;
constexpr int kSclkPin = 10;
constexpr int kData0Pin = 11;
constexpr int kData1Pin = 12;
constexpr int kData2Pin = 13;
constexpr int kData3Pin = 14;
constexpr int kResetPin = 21;
constexpr int kBacklightPin = -1;
constexpr uint16_t kPanelWidth = 450;
constexpr uint16_t kPanelHeight = 600;
constexpr size_t kTxChunkBytes = 48 * 1024;
constexpr bool kPanelMemoryRotated180 = false;
constexpr Board::UiOrientation kDefaultUiOrientation = Board::UiOrientation::Landscape;
}

namespace WaveshareAmoled241::Power {
constexpr int kBatteryAdcPin = 17;
constexpr int kBatteryHoldPin = 16;
constexpr gpio_num_t kBatteryHoldGpio = GPIO_NUM_16;
constexpr bool kRequestPmuShutdownOnPowerOff = false;
constexpr bool kReleaseBatteryHoldBeforeDeepSleep = false;
}

namespace WaveshareAmoled241::Storage {
constexpr gpio_num_t kSdClockPin = GPIO_NUM_4;
constexpr gpio_num_t kSdCommandPin = GPIO_NUM_5;
constexpr gpio_num_t kSdData0Pin = GPIO_NUM_6;
constexpr gpio_num_t kSdData1Pin = GPIO_NUM_NC;
constexpr gpio_num_t kSdData2Pin = GPIO_NUM_NC;
constexpr gpio_num_t kSdData3Pin = GPIO_NUM_NC;
}

namespace WaveshareAmoled241::System {
constexpr int kSystemI2cSdaPin = 47;
constexpr int kSystemI2cSclPin = 48;
constexpr uint32_t kSystemI2cClockHz = 400000;
constexpr uint32_t kSystemI2cTimeoutMs = 10;
constexpr int kTouchSdaPin = 47;
constexpr int kTouchSclPin = 48;
constexpr int kTouchResetPin = 3;
constexpr int kTouchIrqPin = -1;
constexpr uint32_t kTouchI2cClockHz = 400000;
constexpr uint32_t kTouchI2cTimeoutMs = 10;
constexpr int kDeepSleepWakePin = Buttons::kPowerPin;
constexpr gpio_num_t kDeepSleepWakeGpio = GPIO_NUM_15;
}

namespace WaveshareAmoled241::Tca9554Wiring {
constexpr uint8_t kDisplayRailAddress = 0x20;
constexpr uint8_t kDisplayRailEnablePin = 1;
constexpr bool kDisplayRailReleaseBusBeforeRead = false;
}

namespace WaveshareAmoled241::TouchWiring {
constexpr uint8_t kAddress = 0x38;
constexpr bool kReleaseBusBeforeRead = false;
constexpr uint8_t kReleaseConfirmSamples = 2;
constexpr uint8_t kMaxConsecutiveReadFailures = 5;
constexpr uint32_t kPollIntervalMs = 20;
constexpr uint32_t kFailureBackoffMs = 250;
constexpr uint32_t kRecoveryRetryMs = 1000;
constexpr uint32_t kRecoveryEventIgnoreMs = 0;
}
