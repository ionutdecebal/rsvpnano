#pragma once

#include <Arduino.h>
#include <driver/gpio.h>

#include "board/BoardTypes.h"

#ifndef RSVP_LCD_349_REVISION_HEADER
#error "LCD 3.49 platform env must define RSVP_LCD_349_REVISION_HEADER."
#endif

#include RSVP_LCD_349_REVISION_HEADER

namespace WaveshareLcd349::AudioWiring {
constexpr uint8_t kEs8311Address = 0x18;
constexpr int kMclkPin = 7;
constexpr int kBclkPin = 15;
constexpr int kWsPin = 46;
constexpr int kDinPin = 6;
constexpr int kDoutPin = 45;
}

namespace WaveshareLcd349::Buttons {
constexpr int kBootPin = 0;
constexpr int kPowerPin = 16;
constexpr int kKeyPin = -1;
constexpr uint16_t kDebounceMs = 25;
constexpr uint16_t kShortPressMaxMs = 700;
constexpr uint16_t kLongPressMs = 900;
}

namespace WaveshareLcd349::DisplayWiring {
constexpr int kCsPin = 9;
constexpr int kSclkPin = 10;
constexpr int kData0Pin = 11;
constexpr int kData1Pin = 12;
constexpr int kData2Pin = 13;
constexpr int kData3Pin = 14;
constexpr int kResetPin = 21;
constexpr int kBacklightPin = Revision::kBacklightPin;
constexpr gpio_num_t kBacklightGpio = Revision::kBacklightGpio;
constexpr uint16_t kPanelWidth = 172;
constexpr uint16_t kPanelHeight = 640;
constexpr size_t kTxChunkBytes = 16 * 1024;
constexpr bool kPanelMemoryRotated180 = true;
constexpr Board::UiOrientation kDefaultUiOrientation = Board::UiOrientation::LandscapeFlipped;
}

namespace WaveshareLcd349::ImuWiring {
constexpr uint8_t kAddress = 0x6B;
constexpr bool kReleaseBusBeforeRead = false;
}

namespace WaveshareLcd349::Power {
constexpr int kBatteryAdcPin = 4;
constexpr bool kRequestPmuShutdownOnPowerOff = false;
constexpr bool kReleaseBatteryHoldBeforeDeepSleep = true;
}

namespace WaveshareLcd349::Storage {
constexpr gpio_num_t kSdClockPin = GPIO_NUM_41;
constexpr gpio_num_t kSdCommandPin = GPIO_NUM_39;
constexpr gpio_num_t kSdData0Pin = GPIO_NUM_40;
constexpr gpio_num_t kSdData1Pin = GPIO_NUM_NC;
constexpr gpio_num_t kSdData2Pin = GPIO_NUM_NC;
constexpr gpio_num_t kSdData3Pin = GPIO_NUM_NC;
}

namespace WaveshareLcd349::System {
constexpr int kSystemI2cSdaPin = 47;
constexpr int kSystemI2cSclPin = 48;
constexpr uint32_t kSystemI2cClockHz = 300000;
constexpr uint32_t kSystemI2cTimeoutMs = 10;
constexpr int kTouchSdaPin = 17;
constexpr int kTouchSclPin = 18;
constexpr int kTouchResetPin = -1;
constexpr int kTouchIrqPin = -1;
constexpr uint32_t kTouchI2cClockHz = 300000;
constexpr uint32_t kTouchI2cTimeoutMs = 10;
constexpr int kDeepSleepWakePin = Buttons::kPowerPin;
constexpr gpio_num_t kDeepSleepWakeGpio = GPIO_NUM_16;
}

namespace WaveshareLcd349::Tca9554Wiring {
constexpr uint8_t kAddress = 0x20;
constexpr bool kReleaseBusBeforeRead = false;
constexpr uint8_t kBacklightEnablePin = 1;
constexpr uint8_t kSysEnablePin = 6;
constexpr uint8_t kAudioEnablePin = 7;
}

namespace WaveshareLcd349::TouchWiring {
constexpr uint8_t kAddress = 0x3B;
constexpr bool kReleaseBusBeforeRead = false;
constexpr uint8_t kReleaseConfirmSamples = 2;
constexpr uint8_t kMaxConsecutiveReadFailures = 5;
constexpr uint32_t kPollIntervalMs = 20;
constexpr uint32_t kFailureBackoffMs = 250;
constexpr uint32_t kRecoveryRetryMs = 1000;
constexpr uint32_t kRecoveryEventIgnoreMs = 0;
}
