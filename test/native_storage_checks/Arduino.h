#pragma once
#include "../support/Arduino.h"
namespace fixture { inline uint32_t now = 0; }
inline uint32_t millis() { return fixture::now; }
