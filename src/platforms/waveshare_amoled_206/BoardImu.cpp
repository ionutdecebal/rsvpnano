#include "board/BoardImu.h"

#include <Wire.h>

#include "drivers/imu/qmi8658/Qmi8658.h"
#include "platforms/waveshare_amoled_206/WaveshareAmoled206.h"

namespace Board::Imu {

namespace {

Board::UiOrientation gUiOrientation = WaveshareAmoled206::DisplayWiring::kDefaultUiOrientation;

TwoWire &imuWire() { return Wire; }

}  // namespace

bool available() { return true; }

const char *wireName() { return "Wire"; }

uint8_t address() { return WaveshareAmoled206::ImuWiring::kAddress; }

Board::UiOrientation uiOrientation() { return gUiOrientation; }

void setUiOrientation(Board::UiOrientation orientation) { gUiOrientation = orientation; }

bool probeAddress(uint8_t candidateAddress) {
  return BoardDrivers::Qmi8658::probeAddress(imuWire(), candidateAddress);
}

bool readRegister(uint8_t deviceAddress, uint8_t reg, uint8_t &value) {
  return BoardDrivers::Qmi8658::readRegister(
      imuWire(), deviceAddress, reg, value, WaveshareAmoled206::ImuWiring::kReleaseBusBeforeRead);
}

bool writeRegister(uint8_t deviceAddress, uint8_t reg, uint8_t value) {
  return BoardDrivers::Qmi8658::writeRegister(imuWire(), deviceAddress, reg, value);
}

bool readRegisters(uint8_t deviceAddress, uint8_t startReg, uint8_t *buffer, size_t len) {
  return BoardDrivers::Qmi8658::readRegisters(
      imuWire(), deviceAddress, startReg, buffer, len,
      WaveshareAmoled206::ImuWiring::kReleaseBusBeforeRead);
}

}  // namespace Board::Imu
