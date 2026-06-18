#include "board/BoardDisplay.h"

#include <Arduino.h>

#include "board/BoardConfig.h"
#include "platforms/lilygo_tlora_pager/TPagerHardware.h"

// LilyGo T-LoRa-Pager display backend.
//
// DisplayManager renders into a native-geometry framebuffer (480x222) and pushes
// pixel bands through Board::Display::pushColors. On the Waveshare boards those
// calls hit the AXS15231B QSPI driver; here they are implemented against
// LilyGoLib's ST7796 SPI panel so the renderer is unchanged.
//
// Geometry note: BoardConfig sets the panel to 480x222 and the renderer uses the
// identity (Portrait) orientation, so the (x, y, width, height) bands map
// straight onto the panel. LilyGoLib's pushColors(x, y, w, h, color) calls
// setAddrWindow(x, y, x+w-1, y+h-1), matching the renderer's expectations.

#ifndef RSVP_TPAGER_ROTATION
// LilyGoLib's LoRaPager rotation table maps the panel's DISP_WIDTH=222 /
// DISP_HEIGHT=480 as: rotation 0 & 2 -> 480x222 landscape (49px row offset),
// rotation 1 & 3 -> 222x480 portrait. So landscape (what the UI expects) is
// rotation 0; the 180 deg flip is rotation 2. If the image is upside-down for
// your handedness, use the left-handed UI mode or set this to 2.
#define RSVP_TPAGER_ROTATION 0
#endif

namespace {

constexpr uint8_t kMaxBrightnessLevel = 16;  // AW9364 has 16 brightness steps.
uint8_t gBrightnessPercent = 100;
bool gBacklightOn = false;

uint8_t levelForPercent(uint8_t percent) {
  if (percent == 0) {
    return 0;
  }
  uint16_t level = (static_cast<uint16_t>(percent) * kMaxBrightnessLevel + 50) / 100;
  if (level < 1) {
    level = 1;
  } else if (level > kMaxBrightnessLevel) {
    level = kMaxBrightnessLevel;
  }
  return static_cast<uint8_t>(level);
}

void applyBacklight() {
  tpager::hw().setBrightness(gBacklightOn ? levelForPercent(gBrightnessPercent) : 0);
}

}  // namespace

namespace Board::Display {

bool begin() {
  tpager::ensureBegun();
  // LilyGoLib::begin() already brought up the ST7796 (in portrait). Force the
  // landscape orientation the UI expects.
  tpager::hw().setRotation(RSVP_TPAGER_ROTATION);
  gBacklightOn = false;
  applyBacklight();
  return true;
}

Board::UiOrientation defaultUiOrientation() {
  return Board::Config::DEFAULT_UI_ORIENTATION;
}

Board::UiOrientation rotatedUiOrientation() {
  return Board::Config::ROTATED_UI_ORIENTATION;
}

uint16_t nativeWidth() { return Board::Config::PANEL_NATIVE_WIDTH; }

uint16_t nativeHeight() { return Board::Config::PANEL_NATIVE_HEIGHT; }

size_t txChunkBytes() { return Board::Config::DISPLAY_TX_CHUNK_BYTES; }

void holdBacklightOffForDeepSleep() {
  // The backlight is an AW9364 charge-pump driver controlled by LilyGoLib, not a
  // plain PWM pin, so drop it through the driver rather than forcing the GPIO.
  gBacklightOn = false;
  tpager::hw().setBrightness(0);
}

void setBacklight(bool on) {
  gBacklightOn = on;
  applyBacklight();
}

void setBrightness(uint8_t percent) {
  if (percent > 100) {
    percent = 100;
  }
  gBrightnessPercent = percent;
  if (gBacklightOn) {
    applyBacklight();
  }
}

void sleep() {
  // For light sleep DisplayManager blanks the frame first; here we only cut the
  // backlight and leave panel RAM intact, mirroring the Waveshare behavior.
  gBacklightOn = false;
  applyBacklight();
}

void wake() {
  gBacklightOn = true;
  applyBacklight();
}

bool pushColors(uint16_t x, uint16_t y, uint16_t width, uint16_t height,
                const uint16_t *data) {
  if (data == nullptr || width == 0 || height == 0) {
    return false;
  }
  // LilyGoLib's pushColors writes the band and does not mutate the buffer.
  tpager::hw().pushColors(x, y, width, height, const_cast<uint16_t *>(data));
  return true;
}

}  // namespace Board::Display
