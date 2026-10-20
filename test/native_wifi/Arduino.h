#pragma once
#include <cstdint>
inline uint32_t fakeNow=0;
inline uint32_t millis(){return fakeNow;}
inline void delay(uint32_t ms){fakeNow+=ms;}
