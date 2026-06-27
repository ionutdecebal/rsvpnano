#pragma once

#include <stddef.h>
#include <stdint.h>

namespace Board {

enum class UiOrientation : uint8_t {
  Landscape = 0,
  LandscapeFlipped,
  Portrait,
  PortraitFlipped,
};

enum class StorageBusKind : uint8_t {
  SdMmc1Bit = 0,
  SdSpi,
};

enum class PowerManagerKind : uint8_t {
  Tca9554 = 0,
  Axp2101,
  DirectGpioBatteryHold,
};

struct BatteryStatus {
  bool present = false;
  float voltage = 0.0f;
  uint8_t percent = 0;
};

struct PowerDiagnosticSnapshot {
  bool available = false;
  bool externalPowerPresent = false;
  uint8_t status1 = 0;
  uint8_t status2 = 0;
  uint8_t powerKeyIrqStatus = 0;
};

}  // namespace Board
