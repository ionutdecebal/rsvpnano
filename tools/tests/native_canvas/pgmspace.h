#pragma once
#include <cstdint>
#define PROGMEM
#define pgm_read_byte(address) (*reinterpret_cast<const uint8_t*>(address))
#define pgm_read_sbyte(address) (*reinterpret_cast<const int8_t*>(address))
#define pgm_read_word(address) (*reinterpret_cast<const uint16_t*>(address))
#define pgm_read_dword(address) (*reinterpret_cast<const uint32_t*>(address))
#define pgm_read_ptr(address) (*reinterpret_cast<void* const*>(address))
