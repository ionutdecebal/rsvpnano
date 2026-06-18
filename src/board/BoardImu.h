#pragma once

#include <Arduino.h>

#include "board/BoardTypes.h"

namespace Board::Imu {

    bool available();
    const char* wireName();
    uint8_t address();
    Board::UiOrientation uiOrientation();
    void setUiOrientation(Board::UiOrientation orientation);
    bool probeAddress(uint8_t address);
    bool readRegister(uint8_t address, uint8_t reg, uint8_t& value);
    bool writeRegister(uint8_t address, uint8_t reg, uint8_t value);
    bool readRegisters(uint8_t address, uint8_t startReg, uint8_t* buffer, size_t len);

#ifdef RSVP_BOARD_TPAGER
// T-LoRa-Pager only. The IMU here is a Bosch BHI260AP sensor hub driven by the
// LilyGoLib BSP, not a QMI8658 on a raw I2C bus, so the register helpers above
// do not apply. begin() boots the hub's gravity virtual sensor; readGravity()
// returns the latest gravity direction as a unit vector (magnitude ~1). The
// focus timer uses these in place of the register path.
bool begin();
bool readGravity(float &gx, float &gy, float &gz);
#endif

}  // namespace Board::Imu
