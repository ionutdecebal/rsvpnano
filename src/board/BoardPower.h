#pragma once

#include <Arduino.h>

#include "board/BoardTypes.h"

namespace Board::Power {

    using BatteryStatus = Board::BatteryStatus;
    using DiagnosticSnapshot = Board::PowerDiagnosticSnapshot;

    void begin();
    void prepareDeepSleepPowerHold();
    bool enableAudioPowerIfAvailable();
    bool readBatteryStatus(BatteryStatus& status);
    DiagnosticSnapshot diagnosticSnapshot();
    bool externalPowerPresent();
    bool releaseBatteryPowerHold();
    bool supportsSoftwarePowerOff();
    bool powerOffUsesControllerWake();
    bool shouldRequestShutdownOnPowerOff();
    bool shouldReleaseBatteryPowerBeforeDeepSleep();

} // namespace Board::Power
