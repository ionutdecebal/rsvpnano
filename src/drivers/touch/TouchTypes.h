#pragma once

#include <Arduino.h>

namespace BoardDrivers::Touch {

struct Sample {
  bool touched = false;
  uint16_t physicalX = 0;
  uint16_t physicalY = 0;
};

}  // namespace BoardDrivers::Touch
