#pragma once

#include <cstdint>

using BaseType_t = int;
using UBaseType_t = unsigned;
using TickType_t = uint32_t;
constexpr BaseType_t pdTRUE = 1;
constexpr BaseType_t pdFALSE = 0;
constexpr BaseType_t pdPASS = 1;
#define pdMS_TO_TICKS(ms) (ms)
#define IRAM_ATTR
#define portYIELD_FROM_ISR() ((void) 0)
