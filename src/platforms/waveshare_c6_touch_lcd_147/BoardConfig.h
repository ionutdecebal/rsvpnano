#pragma once

#include "platforms/waveshare_c6_touch_lcd_147/WaveshareC6TouchLcd147.h"

namespace Board::Config {

constexpr const char *BOARD_ID = "waveshare_esp32c6_touch_lcd_1_47";
constexpr const char *BOARD_LABEL = "Waveshare ESP32-C6-Touch-LCD-1.47";
constexpr const char *OTA_ASSET_NAME = "rsvp-nano-esp32-c6-touch-lcd-1.47-ota.bin";

constexpr bool ENABLE_TOP_EDGE_MENU_SWIPE = true;
constexpr bool ENABLE_BOTTOM_EDGE_QUICK_SETTINGS_SWIPE = true;
constexpr bool READER_SINGLE_TAP_PAUSES_WHILE_LOCKED = true;
constexpr bool TOUCH_READER_PLAYBACK_ENABLED = true;
constexpr bool ENABLE_RESTRUCTURED_MENU = true;

constexpr int PANEL_NATIVE_WIDTH = WaveshareC6TouchLcd147::DisplayWiring::kPanelWidth;
constexpr int PANEL_NATIVE_HEIGHT = WaveshareC6TouchLcd147::DisplayWiring::kPanelHeight;
constexpr int DISPLAY_WIDTH = PANEL_NATIVE_HEIGHT;
constexpr int DISPLAY_HEIGHT = PANEL_NATIVE_WIDTH;
constexpr int READER_CHROME_MARGIN_X = 28;
constexpr int READER_CHROME_MARGIN_TOP = 14;
constexpr int READER_CHROME_MARGIN_BOTTOM = 14;
constexpr int READER_BATTERY_MARGIN_X = 48;
constexpr int READER_BATTERY_MARGIN_TOP = 24;

}  // namespace Board::Config
