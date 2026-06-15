#pragma once

#include <Arduino.h>

#include "board/BoardConfig.h"

namespace Board::System {

void begin();
void lightSleepUntilBootButton();
void holdBacklightOffForDeepSleep();
void resetWakePeripherals();
void resetTouchController();
void deepSleepUntilConfiguredWake();
void logStartupDiagnostics();

}  // namespace Board::System
