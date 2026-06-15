#include "board/BoardPower.h"

#include "drivers/power/axp2101/Axp2101.h"

namespace Board::Power {

void begin() { BoardDrivers::Axp2101::begin(); }

void prepareDeepSleepPowerHold() {}

void resetWakePeripherals() {}

bool enableAudioPowerIfAvailable() { return true; }

bool readBatteryStatus(BatteryStatus &status) {
  return BoardDrivers::Axp2101::readBatteryStatus(status);
}

DiagnosticSnapshot diagnosticSnapshot() { return BoardDrivers::Axp2101::diagnosticSnapshot(); }

bool externalPowerPresent() { return BoardDrivers::Axp2101::externalPowerPresent(); }

bool releaseBatteryPowerHold() { return BoardDrivers::Axp2101::releasePower(); }

bool powerOffUsesControllerWake() { return Config::REQUEST_PMU_SHUTDOWN_ON_POWEROFF; }

bool shouldRequestShutdownOnPowerOff() { return Config::REQUEST_PMU_SHUTDOWN_ON_POWEROFF; }

bool shouldReleaseBatteryPowerBeforeDeepSleep() {
  return Config::RELEASE_BATTERY_HOLD_BEFORE_DEEP_SLEEP;
}

}  // namespace Board::Power
