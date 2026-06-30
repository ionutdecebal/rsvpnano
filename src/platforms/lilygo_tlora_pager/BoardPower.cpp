#include "board/BoardPower.h"

#include "platforms/lilygo_tlora_pager/TPagerHardware.h"

// LilyGo T-LoRa-Pager power backend. The BQ25896 charger and BQ27220 fuel gauge
// are owned by LilyGoLib; there is no programmable system-rail latch to hold or
// release like the Waveshare TCA9554 boards. True power-off is deep sleep, woken
// by the BOOT button (ext0 on GPIO0).

namespace Board::Power {

void begin() {
  // LilyGoLib::begin() brings up the charger, fuel gauge and power rails.
  tpager::ensureBegun();
}

void prepareDeepSleepPowerHold() {}

bool enableAudioPowerIfAvailable() {
  // Class-D speaker amplifier rail (XL9555 GPIO1).
  tpager::hw().powerControl(POWER_SPEAK, true);
  return true;
}

bool readBatteryStatus(BatteryStatus &status) {
  status = BatteryStatus{};

  // The BQ27220 getters return a cached register snapshot; refresh() performs the
  // actual I2C read that repopulates it. Without this, getVoltage() returns the
  // zero-initialized cache and the battery reads as absent. LilyGoLib only calls
  // gauge.begin()/setNewCapacity() at init, so we must refresh on every sample.
  if (!tpager::hw().gauge.refresh()) {
    return false;
  }

  // BQ27220 fuel gauge reports voltage in millivolts and a calibrated SoC.
  const uint16_t millivolts = tpager::hw().gauge.getVoltage();
  status.voltage = static_cast<float>(millivolts) / 1000.0f;
  status.present = status.voltage >= 2.5f && status.voltage <= 4.6f;
  if (!status.present) {
    status.percent = 0;
    return false;
  }

  uint16_t soc = tpager::hw().gauge.getStateOfCharge();
  if (soc > 100) {
    soc = 100;
  }
  status.percent = static_cast<uint8_t>(soc);
  return true;
}

DiagnosticSnapshot diagnosticSnapshot() { return PowerDiagnosticSnapshot{}; }

bool externalPowerPresent() { return false; }

bool releaseBatteryPowerHold() {
  // No programmable system-rail latch on this board; the caller follows up with
  // esp_deep_sleep_start(), which reaches the documented deep-sleep state and
  // wakes on the BOOT button (ext0 on GPIO0).
  return true;
}

bool supportsSoftwarePowerOff() { return Config::SUPPORTS_SOFTWARE_POWEROFF; }

bool powerOffUsesControllerWake() { return false; }

bool shouldRequestShutdownOnPowerOff() { return Config::REQUEST_PMU_SHUTDOWN_ON_POWEROFF; }

bool shouldReleaseBatteryPowerBeforeDeepSleep() {
  return Config::RELEASE_BATTERY_HOLD_BEFORE_DEEP_SLEEP;
}

}  // namespace Board::Power
