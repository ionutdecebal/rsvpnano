#pragma once

#include <Arduino.h>

#include "board/BoardPower.h"

namespace BoardDrivers::Axp2101 {

struct Config {
  bool releaseBusBeforeRead = false;
  bool enablePowerKeyIrqs = false;
  bool requiresPowerKeyConfig = false;
  uint8_t powerKeyOnTimeValue = 0;
  uint8_t powerKeyOffTimeValue = 0;
};

bool begin(const Config &config);
bool readBatteryStatus(Board::Power::BatteryStatus &status);
Board::Power::DiagnosticSnapshot diagnosticSnapshot();
bool externalPowerPresent();
bool releasePower();
void pollPowerKeyIfDue(bool force = false);
bool isPowerButtonHeld();

}  // namespace BoardDrivers::Axp2101
