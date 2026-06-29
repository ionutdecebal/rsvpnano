#pragma once

#include <Arduino.h>

namespace Board::System {

    void begin();
    void lightSleepUntilBootButton();
    void holdBacklightOffForDeepSleep();
    void deepSleepUntilConfiguredWake();
    const char* wakeLabel(bool externalPowerPresent);
    void logStartupDiagnostics();

} // namespace Board::System
