#pragma once

#include <Arduino.h>

#include "board/BoardConfig.h"

namespace Board::Power {

using BatteryStatus = Board::Config::BatteryStatus;
using DiagnosticSnapshot = Board::Config::PowerDiagnosticSnapshot;

void begin();
void prepareDeepSleepPowerHold();
void resetWakePeripherals();
bool enableAudioPowerIfAvailable();
bool readBatteryStatus(BatteryStatus &status);
DiagnosticSnapshot diagnosticSnapshot();
bool externalPowerPresent();
bool releaseBatteryPowerHold();
bool powerOffUsesControllerWake();
bool shouldRequestShutdownOnPowerOff();
bool shouldReleaseBatteryPowerBeforeDeepSleep();

}  // namespace Board::Power
