#include "board/BoardImu.h"

#include <Arduino.h>
#include <SensorBHI260AP.hpp>
#include <bosch/BoschSensorDataHelper.hpp>

#include "board/BoardConfig.h"
#include "platforms/lilygo_tlora_pager/TPagerHardware.h"

// LilyGo T-LoRa-Pager IMU backend.
//
// This board's IMU is a Bosch BHI260AP smart sensor hub (I2C 0x28), booted and
// owned by the LilyGoLib BSP, which uploads its firmware during begin(). It is
// not a QMI8658 on a raw I2C bus, so the generic register-level Board::Imu
// driver (board/BoardImu.cpp) is excluded for this platform (see platformio.ini)
// and replaced here.
//
// The focus timer only needs the gravity direction to detect how the device is
// resting (flat / on a short side / flipped). We subscribe to the hub's gravity
// virtual sensor through SensorLib's SensorXYZ helper and hand FocusTimer a unit
// gravity vector; its existing classify()/state machine is unchanged. Only
// available(), begin() and readGravity() are defined -- nothing on this board
// references the register/probe helpers, so they are intentionally absent.

namespace Board::Imu {
namespace {

// The gravity virtual sensor reports the gravity direction with magnitude ~1g.
// 25 Hz with no batching resolves a placement/flip well within the focus timer's
// 700 ms orientation-stability window at negligible bus cost.
constexpr float kGravitySampleRateHz = 25.0f;
constexpr uint32_t kGravityReportLatencyMs = 0;

bool gEnabled = false;
bool gHasSample = false;

// UI orientation tracked for the generic Board::Imu seam. The synthetic
// rotary/center input (BoardInput.cpp) is anchored at screen center, so the
// generic touch poller maps it with the identity (Portrait) transform here.
Board::UiOrientation gUiOrientation = Board::Config::DEFAULT_UI_ORIENTATION;
float gLastX = 0.0f;
float gLastY = 0.0f;
float gLastZ = 0.0f;

// Constructed lazily after the BSP exists. tpager::hw() returns the singleton
// board object (whose `sensor` member is valid before begin()), and SensorXYZ
// only stores the id + a reference, so this touches no hardware at construction.
SensorXYZ &gravitySensor() {
  static SensorXYZ sensor(SensorBHI260AP::GRAVITY_VECTOR, tpager::hw().sensor);
  return sensor;
}

}  // namespace

bool available() { return Board::Config::HAS_IMU; }

// --- New Board::Imu register seam --------------------------------------------
// This board's IMU is a BHI260AP sensor hub owned by LilyGoLib, not a register-
// addressable QMI8658 on a raw bus, so the generic register/probe helpers do not
// apply: they report "no device" so the shared orientation/probe code is inert.
// The gravity backend used by the focus timer lives in begin()/readGravity()
// below (declared under RSVP_BOARD_TPAGER in board/BoardImu.h).
const char *wireName() { return "BSP"; }

uint8_t address() { return Board::Config::IMU_I2C_ADDRESS; }

Board::UiOrientation uiOrientation() { return gUiOrientation; }

void setUiOrientation(Board::UiOrientation orientation) { gUiOrientation = orientation; }

bool probeAddress(uint8_t) { return false; }

bool readRegister(uint8_t, uint8_t, uint8_t &) { return false; }

bool writeRegister(uint8_t, uint8_t, uint8_t) { return false; }

bool readRegisters(uint8_t, uint8_t, uint8_t *, size_t) { return false; }

bool begin() {
  if (!Board::Config::HAS_IMU) {
    return false;
  }
  tpager::ensureBegun();
  if (gEnabled) {
    return true;
  }
  if (!gravitySensor().enable(kGravitySampleRateHz, kGravityReportLatencyMs)) {
    Serial.println("[timer] BHI260AP gravity sensor enable failed");
    return false;
  }
  gEnabled = true;
  gHasSample = false;
  Serial.println("[timer] BHI260AP gravity sensor enabled");
  return true;
}

bool readGravity(float &gx, float &gy, float &gz) {
  if (!gEnabled) {
    return false;
  }

  // Pump the hub's FIFO so the helper sees fresh frames; cache the latest so a
  // poll between frames still returns the last known direction.
  tpager::hw().sensor.update();
  SensorXYZ &sensor = gravitySensor();
  if (sensor.hasUpdated()) {
    gLastX = sensor.getX();
    gLastY = sensor.getY();
    gLastZ = sensor.getZ();
    gHasSample = true;
  }

  if (!gHasSample) {
    return false;
  }

  // Normalize to a unit vector so FocusTimer::classify()'s g-based thresholds
  // are independent of the hub's output scaling/units.
  const float magnitude = sqrtf(gLastX * gLastX + gLastY * gLastY + gLastZ * gLastZ);
  if (magnitude < 1e-3f) {
    return false;
  }
  gx = gLastX / magnitude;
  gy = gLastY / magnitude;
  gz = gLastZ / magnitude;
  return true;
}

}  // namespace Board::Imu
