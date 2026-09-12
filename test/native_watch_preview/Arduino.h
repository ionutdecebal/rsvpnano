#pragma once
#include <cmath>
#include <cstdlib>
#include "../support/Arduino.h"
class __FlashStringHelper;
using std::max;
using std::min;
inline int8_t pgm_read_sbyte(const void* address) {
    return static_cast<int8_t>(pgm_read_byte(address));
}
