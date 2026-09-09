#include "ui/screens/ReaderLayout.h"
#include "fonts/RFont4Format.h"

namespace screens::readerLayout {
    size_t pageStrikeIndex() {
        return RFont4::kCompactStrikeIndex - 1;
    }
    ui::Rect readingArea(int16_t width, int16_t height, bool verticalPage) {
        const int16_t left = verticalPage ? portraitTopStrip(height).h : 0;
        const int16_t right = verticalPage ? portraitBottomStrip(height, width).h : 0;
        return {left, 42, static_cast<int16_t>(std::max<int>(0, width - left - right)),
                static_cast<int16_t>(std::max<int>(0, height - 84))};
    }
    ui::Rect portraitTopStrip(int16_t width) {
        return {0, 0, width, 64};
    }
    ui::Rect portraitBatteryRect() {
        return {6, 4, 76, 32};
    }
    ui::Rect portraitFooterRect(int16_t width) {
        return {static_cast<int16_t>(width - 84), 4, 78, 32};
    }
    ui::Rect portraitChapterRect(int16_t width, int16_t height) {
        return {static_cast<int16_t>(width - 36), 68, 30, static_cast<int16_t>(std::max<int>(0, height - 116))};
    }
    ui::Rect portraitFeedbackRect() {
        return {6, 38, 130, 24};
    }
    ui::Rect portraitBottomStrip(int16_t width, int16_t height) {
        return {0, static_cast<int16_t>(height - 42), width, 42};
    }
    ui::Rect portraitPreviousRect(int16_t width, int16_t height, bool leftHanded) {
        return {static_cast<int16_t>(leftHanded ? 6 : width - 54), static_cast<int16_t>(height - 40), 48, 36};
    }
    uint16_t previousSentenceTapWidth() {
        return 48;
    }

    HorizontalChrome horizontalChrome(int16_t width, int16_t height, bool leftHanded) {
        const int16_t footerWidth = width / 3;
        const int16_t y = height - 40;
        return {
            .chapter = {static_cast<int16_t>(leftHanded ? footerWidth + 60 : 8), y,
                        static_cast<int16_t>(width - footerWidth - 76), 40},
            .progress = {static_cast<int16_t>(leftHanded ? 8 : width - footerWidth - 8), y, footerWidth, 40},
            .batteryIcon = {static_cast<int16_t>(width - 124), 2, 36, 36},
            .batteryLabel = {static_cast<int16_t>(width - 82), 2, 72, 36},
            .arrows = {static_cast<int16_t>(leftHanded ? 4 : width - 48), static_cast<int16_t>(height / 2 - 22), 44,
                       44},
            .textSize = static_cast<uint8_t>(height < 240 ? 2 : 3),
        };
    }

    void chrome(ui::Context& ui, const Chrome& view, const settings::ReadingSettings& settings,
                const Board::Power::BatteryState& battery) {
        const bool vertical = view.vertical;
        const bool showChapter = settings::visible(settings.chapterVisibility, view.reading);
        const bool showProgress = settings::visible(settings.progressVisibility, view.reading);
        const bool showArrows = settings::visible(settings.arrowsVisibility, view.reading);
        const bool showBattery = settings::visible(settings.batteryLabelVisibility, view.reading),
                   leftHanded = settings.leftHanded;
        const auto chapter = view.chapter, footer = view.footer;
        const auto batteryLabel = view.batteryLabel;
        const bool showBatteryIcon = settings::visible(settings.batteryIconVisibility, view.reading);
        const auto muted = ui.color(ui::themes::Muted);
        if (vertical) {
            const int16_t width = ui.height(), height = ui.width();
            auto state = ui::Context::signature(showBattery ? batteryLabel : std::string_view{});
            state = ui::Context::signature(showProgress ? footer : std::string_view{}, state);
            state = ui::Context::signature(view.overlay, state);
            state = ui::Context::combine(state, showBatteryIcon);
            state = ui::Context::combine(state, battery.charging);
            state = ui::Context::combine(state, battery.status.percent);
            if (ui.redraw(ui::rotateClockwise(portraitTopStrip(width), width), state)) {
                if (showBattery || showBatteryIcon)
                    ui.portraitBattery(portraitBatteryRect(), battery.status.percent, battery.charging,
                                       showBattery ? batteryLabel : std::string_view{}, showBatteryIcon);
                if (!view.overlay.empty())
                    ui.portraitText(portraitFeedbackRect(), view.overlay, 2, ui.color(ui::themes::Accent),
                                    ui::TextAlign::Center);
                if (showProgress)
                    ui.portraitText(portraitFooterRect(width), footer, 2, muted, ui::TextAlign::Right);
            }
            const auto chapterRect = portraitChapterRect(width, height);
            if (ui.redraw(ui::rotateClockwise(chapterRect, width),
                          ui::Context::signature(showChapter ? chapter : std::string_view{})))
                if (showChapter)
                    ui.portraitVerticalText(chapterRect, chapter, 2, muted);
            if (ui.redraw(ui::rotateClockwise(portraitBottomStrip(width, height), width),
                          ui::Context::combine(ui::Context::combine(Fnv1a::kOffsetBasis, leftHanded), showArrows)))
                if (showArrows)
                    ui.portraitText(portraitPreviousRect(width, height, leftHanded), "<<", 2, muted,
                                    ui::TextAlign::Center);
        } else {
            horizontalChrome(ui, view, settings, battery);
        }
    }
} // namespace screens::readerLayout
