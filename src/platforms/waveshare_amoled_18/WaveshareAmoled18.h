#pragma once

#include <Arduino.h>
#include <driver/gpio.h>

#include "board/BoardTypes.h"

#ifndef RSVP_AMOLED_18_VERSION_HEADER
#error "AMOLED 1.8 platform env must define RSVP_AMOLED_18_VERSION_HEADER."
#endif

#include RSVP_AMOLED_18_VERSION_HEADER

namespace WaveshareAmoled18::AudioWiring {
constexpr uint8_t kEs8311Address = 0x18;
constexpr int kMclkPin = 16;
constexpr int kBclkPin = 9;
constexpr int kWsPin = 45;
constexpr int kDinPin = 10;
constexpr int kDoutPin = 8;
}

namespace WaveshareAmoled18::Buttons {
constexpr int kBootPin = 0;
constexpr int kPowerPin = -1;
constexpr int kKeyPin = -1;
constexpr uint16_t kDebounceMs = 25;
constexpr uint16_t kShortPressMaxMs = 700;
constexpr uint16_t kLongPressMs = 900;
}

namespace WaveshareAmoled18::Axp2101Wiring {
constexpr bool kReleaseBusBeforeRead = true;
constexpr bool kEnablePowerKeyIrqs = false;
constexpr bool kRequiresPowerKeyConfig = true;
constexpr uint8_t kPowerKeyOnTimeValue = 0x00;   // 128 ms press to power on.
constexpr uint8_t kPowerKeyOffTimeValue = 0x01;  // 6 second PMU fallback power off.
}

namespace WaveshareAmoled18::DisplayWiring {
constexpr int kCsPin = 12;
constexpr int kSclkPin = 11;
constexpr int kData0Pin = 4;
constexpr int kData1Pin = 5;
constexpr int kData2Pin = 6;
constexpr int kData3Pin = 7;
constexpr int kResetPin = -1;
constexpr int kBacklightPin = -1;
constexpr uint16_t kPanelWidth = 368;
constexpr uint16_t kPanelHeight = 448;
constexpr size_t kTxChunkBytes = 32 * 1024;
constexpr bool kPanelMemoryRotated180 = Version::kPanelMemoryRotated180;
constexpr Board::UiOrientation kDefaultUiOrientation =
    Version::kDefaultUiOrientation;
constexpr Board::UiOrientation kRotatedUiOrientation = Version::kRotatedUiOrientation;
}

namespace WaveshareAmoled18::ImuWiring {
constexpr uint8_t kAddress = 0x6B;
constexpr bool kReleaseBusBeforeRead = true;
}

namespace WaveshareAmoled18::Power {
constexpr bool kRequestPmuShutdownOnPowerOff = true;
constexpr bool kReleaseBatteryHoldBeforeDeepSleep = false;
constexpr bool kUsesRecoverableSoftOff = true;
constexpr bool kSoftOffWakeUsesPowerButton = true;
constexpr bool kSoftOffWakeUsesBootButton = false;
constexpr uint32_t kSoftOffWakeConfirmMs = 500;
}

namespace WaveshareAmoled18::Storage {
constexpr gpio_num_t kSdClockPin = GPIO_NUM_2;
constexpr gpio_num_t kSdCommandPin = GPIO_NUM_1;
constexpr gpio_num_t kSdData0Pin = GPIO_NUM_3;
constexpr gpio_num_t kSdData1Pin = GPIO_NUM_NC;
constexpr gpio_num_t kSdData2Pin = GPIO_NUM_NC;
constexpr gpio_num_t kSdData3Pin = GPIO_NUM_NC;
}

namespace WaveshareAmoled18::System {
constexpr int kSystemI2cSdaPin = 15;
constexpr int kSystemI2cSclPin = 14;
constexpr uint32_t kSystemI2cClockHz = 200000;
constexpr uint32_t kSystemI2cTimeoutMs = 20;
constexpr int kTouchSdaPin = 15;
constexpr int kTouchSclPin = 14;
constexpr int kTouchResetPin = -1;
constexpr int kTouchIrqPin = Version::kTouchIrqPin;
constexpr uint32_t kTouchI2cClockHz = kSystemI2cClockHz;
constexpr uint32_t kTouchI2cTimeoutMs = kSystemI2cTimeoutMs;
constexpr int kDeepSleepWakePin = Buttons::kBootPin;
constexpr gpio_num_t kDeepSleepWakeGpio = GPIO_NUM_0;
}

namespace WaveshareAmoled18::Tca9554Wiring {
constexpr uint8_t kAddress = 0x20;
constexpr bool kReleaseBusBeforeRead = true;
constexpr uint8_t kPowerButtonPin = 4;
constexpr uint8_t kPmuIrqPin = 5;
constexpr uint8_t kSdEnablePin = 7;
constexpr uint8_t kTouchResetPin = 0;
constexpr uint8_t kLcdResetPin = 1;
constexpr uint8_t kDisplayEnablePin = 2;
constexpr uint8_t kDisplayMask =
    (1U << kTouchResetPin) | (1U << kLcdResetPin) | (1U << kDisplayEnablePin);
constexpr uint8_t kSdEnableMask = 1U << kSdEnablePin;
constexpr uint8_t kOutputMask = kDisplayMask | kSdEnableMask;
constexpr uint8_t kDisplayClearMask = 0xFFU ^ kDisplayMask;
constexpr uint8_t kOutputClearMask = 0xFFU ^ kOutputMask;
constexpr uint8_t kInputMask = (1U << kPowerButtonPin) | (1U << kPmuIrqPin);
}

namespace WaveshareAmoled18::TouchWiring {
constexpr uint8_t kAddress = Version::kTouchAddress;
constexpr bool kReleaseBusBeforeRead = true;
constexpr uint8_t kReleaseConfirmSamples = 2;
constexpr uint8_t kMaxConsecutiveReadFailures = 5;
constexpr uint32_t kPollIntervalMs = 50;
constexpr uint32_t kFailureBackoffMs = 500;
constexpr uint32_t kRecoveryRetryMs = 3000;
constexpr uint32_t kRecoveryEventIgnoreMs = 1200;
}
