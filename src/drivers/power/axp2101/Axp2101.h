#pragma once

#include <Arduino.h>

#include "board/BoardPower.h"

namespace BoardDrivers::Axp2101 {

bool begin();
bool readBatteryStatus(Board::Power::BatteryStatus &status);
Board::Power::DiagnosticSnapshot diagnosticSnapshot();
bool externalPowerPresent();
bool releasePower();
void pollPowerKeyIfDue(bool force = false);
bool isPowerButtonHeld();
bool consumeShortPress();
bool consumeLongPress();

}  // namespace BoardDrivers::Axp2101
