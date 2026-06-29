#pragma once

namespace Board::Config {

constexpr const char *BOARD_ID = "waveshare_esp32s3_touch_amoled_2_41";
constexpr const char *BOARD_LABEL = "Waveshare ESP32-S3-Touch-AMOLED-2.41";
constexpr const char *OTA_ASSET_NAME = "rsvp-nano-esp32-s3-touch-amoled-2.41-ota.bin";

constexpr bool ENABLE_TOP_EDGE_MENU_SWIPE = true;
constexpr bool ENABLE_BOTTOM_EDGE_QUICK_SETTINGS_SWIPE = true;
constexpr bool READER_SINGLE_TAP_PAUSES_WHILE_LOCKED = true;
constexpr bool TOUCH_READER_PLAYBACK_ENABLED = true;
constexpr bool ENABLE_RESTRUCTURED_MENU = true;

constexpr int PANEL_NATIVE_WIDTH = 450;
constexpr int PANEL_NATIVE_HEIGHT = 600;
constexpr int DISPLAY_WIDTH = 600;
constexpr int DISPLAY_HEIGHT = 450;
constexpr int READER_CHROME_MARGIN_X = 12;
constexpr int READER_CHROME_MARGIN_TOP = 8;
constexpr int READER_CHROME_MARGIN_BOTTOM = 8;
constexpr int READER_BATTERY_MARGIN_X = READER_CHROME_MARGIN_X;
constexpr int READER_BATTERY_MARGIN_TOP = READER_CHROME_MARGIN_TOP;

}  // namespace Board::Config
