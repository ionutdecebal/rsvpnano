#pragma once

// Central access point to the LilyGoLib BSP for the T-LoRa-Pager platform.
//
// All T-Pager hardware (ST7796 display, rotary encoder, TCA8418 keyboard, SPI
// SD card, BQ25896/BQ27220 power, ES8311 codec, DRV2605 haptics) is driven
// through the single LilyGoLoRaPager instance. This header gives the rest of
// the platform one idempotent entry point so initialization order does not
// matter: every Board:: backend (System/Display/Power/Audio) and the platform
// InputTouch can each call ensureBegun() before touching a peripheral.

#include <LilyGoLib.h>

namespace tpager {

// Returns the LilyGoLib singleton. Call ensureBegun() before relying on any
// peripheral being initialized.
LilyGoLoRaPager &hw();

// Runs LilyGoLib's full hardware init exactly once. Safe to call repeatedly
// (LilyGoLib::begin() itself is guarded, and so is this wrapper).
void ensureBegun();

}  // namespace tpager
