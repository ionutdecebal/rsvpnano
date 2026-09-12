#pragma once

#include <Arduino.h>

#include "ui/Touch.h"

namespace Input {

    using ActionMask = uint16_t;

    enum Action : ActionMask {
        ActionNone = 0,
        ActionSelect = 1U << 0,
        ActionBack = 1U << 1,
        ActionOpenMenu = 1U << 2,
        ActionPlayPause = 1U << 3,
        ActionStandby = 1U << 4,
        ActionPowerOff = 1U << 5,
    };

    struct PressActions {
        ActionMask shortPress = ActionNone;
        ActionMask longPress = ActionNone;
    };

    struct ControlTiming {
        uint16_t debounceMs = 25;
        uint16_t shortPressMaxMs = 700;
        uint16_t longPressMs = 900;
    };

    struct TouchTiming {
        uint8_t releaseConfirmSamples = 2;
        uint8_t maxConsecutiveReadFailures = 5;
        uint32_t readyPollIntervalMs = 20;
        uint32_t pollIntervalMs = 20;
        uint32_t failureBackoffMs = 250;
        uint32_t recoveryRetryMs = 1000;
        uint32_t recoveryEventIgnoreMs = 0;
    };

    constexpr bool hasAction(ActionMask actions, ActionMask action) {
        return (actions & action) != 0;
    }

    bool begin();
    void end();
    void cancel();
    void resume();
    void notifyTouchFromISR();
    bool poll(ActionMask& actions);
    ui::TouchSampleResult pollTouch(ui::TouchContact& contact);

} // namespace Input
