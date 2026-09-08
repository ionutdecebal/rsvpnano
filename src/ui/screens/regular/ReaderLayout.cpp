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
    ui::Rect batteryRect(int16_t width) {
        return {static_cast<int16_t>(std::max<int16_t>(0, width - 126)), 0, 116, 36};
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

    uint16_t previousSentenceTapWidth() {
        return 112;
    }
    void chrome(ui::Context& ui, const Chrome& view, const settings::ReadingSettings& settings,
                const Board::Power::BatteryState& battery) {
        const bool vertical = view.vertical, showChapter = view.showChapter, showProgress = view.showProgress;
        const bool showBattery = view.showBattery, showBatteryIcon = settings.batteryIconVisible && showBattery;
        const auto chapterLabel = view.chapter, footer = view.footer, batteryLabel = view.batteryLabel;
        if (vertical) {
            const int16_t portraitWidth = ui.height();
            const int16_t portraitHeight = ui.width();
            const auto overlay = view.overlay;
            uint32_t topState = ui::Context::signature(footer, view.topState);
            topState = ui::Context::signature(batteryLabel, topState);
            topState = ui::Context::combine(topState, battery.status.percent);
            topState = ui::Context::combine(topState, battery.charging);
            topState = ui::Context::combine(topState, showBattery);
            topState = ui::Context::combine(topState, showBatteryIcon);
            topState = ui::Context::combine(topState, showProgress);
            if (ui.redraw(ui::rotateClockwise(portraitTopStrip(portraitWidth), portraitWidth), topState)) {
                if (showBattery)
                    ui.portraitBattery(portraitBatteryRect(), battery.status.percent, battery.charging, batteryLabel,
                                       showBatteryIcon);
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

            const uint32_t bottomState = view.bottomState;
            if (ui.redraw(ui::rotateClockwise(portraitBottomStrip(portraitWidth, portraitHeight), portraitWidth),
                          bottomState)) {
                ui.portraitText(portraitPreviousRect(portraitWidth, portraitHeight, settings.leftHanded), "<<", 2,
                                ui.color(ui::themes::ColorRole::Muted), ui::TextAlign::Center);
            }
        } else {
            const int16_t footerWidth = showProgress ? static_cast<int16_t>(footer.size() * 12) : 0;
            const int16_t footerX = settings.leftHanded ? 18 : static_cast<int16_t>(ui.width() - 18 - footerWidth);
            const int16_t chapterX =
                settings.leftHanded && showProgress ? static_cast<int16_t>(footerX + footerWidth + 24) : 18;
            const int16_t chapterWidth = showProgress ? static_cast<int16_t>(ui.width() - 60 - footerWidth)
                                                      : static_cast<int16_t>(ui.width() - 36);
            ui.label({chapterX, static_cast<int16_t>(ui.height() - 26), chapterWidth, 26},
                     showChapter ? chapterLabel : std::string_view{}, 2, ui::themes::ColorRole::Muted,
                     settings.leftHanded ? ui::TextAlign::Right : ui::TextAlign::Left, 1, view.locale);
            ui.label({footerX, static_cast<int16_t>(ui.height() - 26), footerWidth, 26}, footer, 2,
                     ui::themes::ColorRole::Muted, settings.leftHanded ? ui::TextAlign::Left : ui::TextAlign::Right);
            ui.battery(batteryRect(ui.width()), battery.status.percent, battery.charging,
                       showBattery ? batteryLabel : std::string_view{},
                       showBatteryIcon);
        }
    }
} // namespace screens::readerLayout
