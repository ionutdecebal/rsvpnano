#pragma once

#include "board/BoardPower.h"
#include "settings/SettingsModel.h"
#include "ui/Ui.h"

namespace screens::readerLayout {
    size_t pageStrikeIndex();
    ui::Rect readingArea(int16_t width, int16_t height, bool verticalPage);
    ui::Rect batteryRect(int16_t width, int16_t height);
    ui::Rect portraitTopStrip(int16_t width);
    ui::Rect portraitBatteryRect();
    ui::Rect portraitFooterRect(int16_t width);
    ui::Rect portraitChapterRect(int16_t width, int16_t height);
    ui::Rect portraitFeedbackRect();
    ui::Rect portraitBottomStrip(int16_t width, int16_t height);
    ui::Rect portraitPreviousRect(int16_t width, int16_t height, bool leftHanded);
    uint16_t previousSentenceTapWidth();

    struct HorizontalChrome {
        ui::Rect chapter, progress, battery, arrows;
        uint8_t textSize;
        ui::Context::BatteryLayout batteryParts{};
    };
    HorizontalChrome horizontalChrome(int16_t width, int16_t height, bool leftHanded, int16_t footerWidth = 36);
    std::string batteryText(settings::BatteryLabel format, const Board::Power::BatteryState& battery);
    std::string progressText(ui::Context& ui, settings::FooterMetric format, uint8_t percent, uint32_t minutes);

    struct Chrome {
        bool vertical;
        std::string_view chapter;
        std::string_view locale;
        std::string_view footer;
        std::string_view batteryLabel;
        std::string_view overlay;
        bool reading = false;
        uint32_t topState;
        uint32_t bottomState;
        bool ghostHidden = false;
    };
    void drawArrows(ui::Context& ui, const settings::ReadingSettings& settings, bool reading, int16_t wordHeight,
                    bool ghostHidden = false);
    void horizontalChrome(ui::Context& ui, const Chrome& view, const settings::ReadingSettings& settings,
                          const Board::Power::BatteryState& battery);
    void horizontalChrome(ui::Context& ui, const Chrome& view, const settings::ReadingSettings& settings,
                          const Board::Power::BatteryState& battery, const HorizontalChrome& layout);
    void chrome(ui::Context& ui, const Chrome& view, const settings::ReadingSettings& settings,
                const Board::Power::BatteryState& battery);
} // namespace screens::readerLayout
