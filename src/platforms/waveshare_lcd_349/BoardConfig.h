#pragma once

#include "platforms/waveshare_lcd_349/WaveshareLcd349.h"

namespace Board::Config {
    constexpr int DISPLAY_WRITE_ALIGNMENT = 1;
    constexpr int DISPLAY_BUFFER_ROWS = 0;

    constexpr const char* BOARD_ID = WaveshareLcd349::Revision::kBoardId;
    constexpr const char* BOARD_LABEL = WaveshareLcd349::Revision::kBoardLabel;
    constexpr const char* OTA_ASSET_NAME = WaveshareLcd349::Revision::kOtaAssetName;

    constexpr bool ENABLE_TOP_EDGE_MENU_SWIPE = true;
    constexpr bool ENABLE_BOTTOM_EDGE_QUICK_SETTINGS_SWIPE = true;
    constexpr bool READER_SINGLE_TAP_PAUSES_WHILE_LOCKED = true;
    constexpr bool TOUCH_READER_PLAYBACK_ENABLED = true;
    constexpr bool ENABLE_RESTRUCTURED_MENU = true;
    constexpr bool HAS_LIGHT_SLEEP_TOUCH_IRQ = WaveshareLcd349::System::kTouchIrqPin >= 0;

    constexpr int PANEL_NATIVE_WIDTH = WaveshareLcd349::DisplayWiring::kPanelWidth;
    constexpr int PANEL_NATIVE_HEIGHT = WaveshareLcd349::DisplayWiring::kPanelHeight;
    constexpr int DISPLAY_WIDTH = PANEL_NATIVE_HEIGHT;
    constexpr int DISPLAY_HEIGHT = PANEL_NATIVE_WIDTH;
    constexpr int READER_CHROME_MARGIN_X = 12;
    constexpr int READER_CHROME_MARGIN_TOP = 8;
    constexpr int READER_CHROME_MARGIN_BOTTOM = 8;
    constexpr int READER_BATTERY_MARGIN_X = READER_CHROME_MARGIN_X;
    constexpr int READER_BATTERY_MARGIN_TOP = READER_CHROME_MARGIN_TOP;

} // namespace Board::Config
