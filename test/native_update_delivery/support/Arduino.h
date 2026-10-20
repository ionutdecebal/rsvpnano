#pragma once
#include <cstdint>
extern uint32_t testMillis;
inline uint32_t millis() { return testMillis; }
inline void delay(uint32_t ms) { testMillis += ms; }
inline void configTime(long, int, const char*, const char*) {}
