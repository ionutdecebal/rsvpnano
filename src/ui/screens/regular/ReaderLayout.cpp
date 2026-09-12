#include "ui/screens/ReaderLayout.h"
#include "fonts/RFont4Format.h"

namespace screens::readerLayout {
    size_t pageStrikeIndex() {
        return RFont4::kCompactStrikeIndex;
    }
    ui::Rect readingArea(int16_t width, int16_t height, bool verticalPage) {
        const int16_t left = verticalPage ? portraitTopStrip(height).h : 0;
        const int16_t right = verticalPage ? portraitBottomStrip(height, width).h : 0;
        return {left, 36, static_cast<int16_t>(std::max<int>(0, width - left - right)),
                static_cast<int16_t>(std::max<int>(0, height - 72))};
    }

    ui::Rect portraitTopStrip(int16_t width) {
        return {0, 0, width, 58};
    }

    ui::Rect portraitBatteryRect() {
        return {6, 4, 92, 30};
    }

    ui::Rect portraitFooterRect(int16_t width) {
        return {static_cast<int16_t>(width - 72), 4, 66, 30};
    }

    ui::Rect portraitChapterRect(int16_t width, int16_t height) {
        return {static_cast<int16_t>(width - 36), 58, 30, static_cast<int16_t>(height - 106)};
    }

    ui::Rect portraitFeedbackRect() {
        return {6, 36, 118, 20};
    }

    ui::Rect portraitBottomStrip(int16_t width, int16_t height) {
        return {0, static_cast<int16_t>(height - 48), width, 48};
    }

    ui::Rect portraitPreviousRect(int16_t width, int16_t height, bool leftHanded) {
        return {static_cast<int16_t>(leftHanded ? 8 : width - 48), static_cast<int16_t>(height - 40), 40, 30};
    }

    ui::Rect previousSentenceRect(int16_t width, int16_t height, bool leftHanded, bool) {
        return {static_cast<int16_t>(leftHanded ? 0 : std::max<int>(0, width - 112)), 0,
                std::min<int16_t>(width, leftHanded ? 113 : 112), height};
    }
    ui::Rect arrowArea(int16_t width, int16_t height, bool leftHanded, int16_t wordHeight) {
        auto rect = horizontalChrome(width, height, leftHanded).arrows;
        rect.h = std::max(rect.h, wordHeight);
        rect.y = (height - rect.h) / 2;
        return rect;
    }
    HorizontalChrome horizontalChrome(int16_t width, int16_t height, bool leftHanded, int16_t footerWidth) {
        const int16_t y = height - 26;
        return {
            .chapter = {static_cast<int16_t>(leftHanded && footerWidth ? footerWidth + 42 : 18), y,
                        static_cast<int16_t>(footerWidth ? width - footerWidth - 60 : width - 36), 26},
            .progress = {static_cast<int16_t>(leftHanded ? 18 : width - footerWidth - 18), y, footerWidth, 26},
            .battery = {static_cast<int16_t>(width - 126), 0, 116, 36},
            .arrows = {static_cast<int16_t>(leftHanded ? 8 : width - 52), static_cast<int16_t>(height / 2 - 22), 44,
                       44},
            .textSize = 2,
        };
    }

    void chrome(ui::Context& ui, const Chrome& view, const settings::ReadingSettings& settings,
                const Board::Power::BatteryState& battery) {
        const bool vertical = view.vertical;
        const bool showChapter = settings::visible(settings.chapterVisibility, view.reading);
        const bool showProgress = settings::visible(settings.progressVisibility, view.reading);
        const bool showArrows = settings::visible(settings.arrowsVisibility, view.reading);
        const bool showBatteryLabel = settings::visible(settings.batteryLabelVisibility, view.reading);
        const bool showBatteryIcon = settings::visible(settings.batteryIconVisibility, view.reading);
        const auto chapterLabel = view.chapter, footer = view.footer, batteryLabel = view.batteryLabel;
        if (vertical) {
            const int16_t portraitWidth = ui.height();
            const int16_t portraitHeight = ui.width();
            const auto overlay = view.overlay;
            uint32_t topState = ui::Context::signature(footer, view.topState);
            topState = ui::Context::signature(batteryLabel, topState);
            topState = ui::Context::combine(topState, battery.status.percent);
            topState = ui::Context::combine(topState, battery.charging);
            topState = ui::Context::combine(topState, showBatteryIcon);
            topState = ui::Context::combine(topState, showBatteryLabel);
            topState = ui::Context::combine(topState, showProgress);
            if (ui.redraw(ui::rotateClockwise(portraitTopStrip(portraitWidth), portraitWidth), topState)) {
                if (showBatteryLabel || showBatteryIcon)
                    ui.portraitBattery(portraitBatteryRect(), battery.status.percent, battery.charging,
                                       showBatteryLabel ? batteryLabel : std::string_view{}, showBatteryIcon);
                if (showProgress)
                    ui.portraitText(portraitFooterRect(portraitWidth), footer, 2,
                                    ui.color(ui::themes::ColorRole::Muted), ui::TextAlign::Right);
                if (!overlay.empty())
                    ui.portraitText(portraitFeedbackRect(), overlay, 1, ui.color(ui::themes::ColorRole::Accent),
                                    ui::TextAlign::Center);
            }

            const std::string_view visibleChapter = showChapter ? chapterLabel : std::string_view{};
            uint32_t chapterState = ui::Context::signature(visibleChapter);
            chapterState = ui::Context::combine(chapterState, showChapter);
            const ui::Rect chapterArea = portraitChapterRect(portraitWidth, portraitHeight);
            if (ui.redraw(ui::rotateClockwise(chapterArea, portraitWidth), chapterState))
                ui.portraitVerticalText(chapterArea, visibleChapter, 1, ui.color(ui::themes::ColorRole::Muted),
                                        view.locale);

            const uint32_t bottomState = ui::Context::combine(view.bottomState, showArrows);
            if (ui.redraw(ui::rotateClockwise(portraitBottomStrip(portraitWidth, portraitHeight), portraitWidth),
                          bottomState)) {
                if (showArrows)
                    ui.portraitText(portraitPreviousRect(portraitWidth, portraitHeight, settings.leftHanded), "<<", 2,
                                    ui.color(ui::themes::ColorRole::Muted), ui::TextAlign::Center);
            }
        } else {
            horizontalChrome(ui, view, settings, battery);
        }
    }
} // namespace screens::readerLayout
