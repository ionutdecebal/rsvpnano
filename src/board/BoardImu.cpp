#include "board/BoardImu.h"

#include <Wire.h>

#include "board/BoardConfig.h"
#include "drivers/imu/qmi8658/Qmi8658.h"

namespace Board::Imu {
namespace {

Board::UiOrientation gUiOrientation = Board::Config::DEFAULT_UI_ORIENTATION;

TwoWire &imuWire() { return Board::Config::IMU_USES_WIRE1 ? Wire1 : Wire; }

}  // namespace

bool available() { return Board::Config::HAS_IMU; }

const char *wireName() { return Board::Config::IMU_USES_WIRE1 ? "Wire1" : "Wire"; }

uint8_t address() { return Board::Config::IMU_I2C_ADDRESS; }

Board::UiOrientation uiOrientation() { return gUiOrientation; }

void setUiOrientation(Board::UiOrientation orientation) { gUiOrientation = orientation; }

bool probeAddress(uint8_t candidateAddress) {
  return BoardDrivers::Qmi8658::probeAddress(imuWire(), candidateAddress);
}

bool readRegister(uint8_t deviceAddress, uint8_t reg, uint8_t &value) {
  return BoardDrivers::Qmi8658::readRegister(
      imuWire(), deviceAddress, reg, value, Board::Config::IMU_RELEASE_BUS_BEFORE_READ);
}

bool writeRegister(uint8_t deviceAddress, uint8_t reg, uint8_t value) {
  return BoardDrivers::Qmi8658::writeRegister(imuWire(), deviceAddress, reg, value);
}

bool readRegisters(uint8_t deviceAddress, uint8_t startReg, uint8_t *buffer, size_t len) {
  return BoardDrivers::Qmi8658::readRegisters(
      imuWire(), deviceAddress, startReg, buffer, len,
      Board::Config::IMU_RELEASE_BUS_BEFORE_READ);
}

}  // namespace Board::Imu
