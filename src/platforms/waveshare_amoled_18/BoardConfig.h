#pragma once

#include "platforms/waveshare_amoled_18/WaveshareAmoled18.h"

namespace Board::Config {

using UiOrientation = Board::UiOrientation;

constexpr const char *BOARD_ID = WaveshareAmoled18::Version::kBoardId;
constexpr const char *BOARD_LABEL = WaveshareAmoled18::Version::kBoardLabel;
constexpr const char *OTA_ASSET_NAME = WaveshareAmoled18::Version::kOtaAssetName;

constexpr bool ENABLE_TOP_EDGE_MENU_SWIPE = true;
constexpr bool ENABLE_BOTTOM_EDGE_QUICK_SETTINGS_SWIPE = true;
constexpr bool READER_SINGLE_TAP_PAUSES_WHILE_LOCKED = false;
constexpr bool TOUCH_READER_PLAYBACK_ENABLED = false;
constexpr bool ENABLE_RESTRUCTURED_MENU = true;

constexpr int PANEL_NATIVE_WIDTH = WaveshareAmoled18::DisplayWiring::kPanelWidth;
constexpr int PANEL_NATIVE_HEIGHT = WaveshareAmoled18::DisplayWiring::kPanelHeight;
constexpr int DISPLAY_WIDTH = 448;
constexpr int DISPLAY_HEIGHT = 368;
constexpr int READER_CHROME_MARGIN_X = 40;
constexpr int READER_CHROME_MARGIN_TOP = 24;
constexpr int READER_CHROME_MARGIN_BOTTOM = 24;
constexpr int READER_BATTERY_MARGIN_X = 64;
constexpr int READER_BATTERY_MARGIN_TOP = 32;

constexpr bool UI_ROTATED_180 = WaveshareAmoled18::DisplayWiring::kDefaultUiOrientation ==
                                UiOrientation::LandscapeFlipped;
constexpr UiOrientation DEFAULT_UI_ORIENTATION =
    WaveshareAmoled18::DisplayWiring::kDefaultUiOrientation;
constexpr UiOrientation ROTATED_UI_ORIENTATION =
    WaveshareAmoled18::DisplayWiring::kRotatedUiOrientation;

}  // namespace Board::Config
