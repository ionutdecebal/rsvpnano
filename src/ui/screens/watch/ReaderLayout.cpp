#include "ui/screens/ReaderLayout.h"
#include "fonts/RFont4Format.h"
#include "ui/screens/watch/Layout.h"

namespace screens::readerLayout {
    size_t pageStrikeIndex() {
        return RFont4::kCompactStrikeIndex - 1;
    }
    ui::Rect readingArea(int16_t width, int16_t height, bool verticalPage) {
        if (!verticalPage) {
            auto area = watch::contentBounds(width, height);
            area.y += 42;
            area.h = std::max<int16_t>(0, area.h - 84);
            return area;
        }
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
    ui::Rect previousSentenceRect(int16_t width, int16_t height, bool leftHanded, bool pageView) {
        if (pageView)
            return {static_cast<int16_t>(leftHanded ? 0 : std::max<int>(0, width - 48)), 0,
                    std::min<int16_t>(width, leftHanded ? 49 : 48), height};
        return horizontalChrome(width, height, leftHanded).arrows;
    }
    ui::Rect arrowArea(int16_t width, int16_t height, bool leftHanded, int16_t) {
        return horizontalChrome(width, height, leftHanded).arrows;
    }

    HorizontalChrome horizontalChrome(int16_t width, int16_t height, bool leftHanded, int16_t) {
        const auto area = watch::contentBounds(width, height);
        const int16_t footerWidth = area.w / 3;
        const int16_t batteryWidth = std::min<int16_t>(104, area.w - 48);
        const int16_t y = area.y + area.h - 42;
        return {
            .chapter = {static_cast<int16_t>(area.x + (leftHanded ? footerWidth + 4 : 0)), y,
                        static_cast<int16_t>(area.w - footerWidth - 4), 40},
            .progress = {static_cast<int16_t>(leftHanded ? area.x : area.x + area.w - footerWidth), y, footerWidth, 40},
            .battery = {static_cast<int16_t>(area.x + area.w - batteryWidth), area.y, batteryWidth, 42},
            .arrows = {static_cast<int16_t>(leftHanded ? area.x : area.x + area.w - 44), static_cast<int16_t>(y - 44),
                       44, 44},
            .textSize = static_cast<uint8_t>(height < 240 ? 2 : 3),
        };
    }

    void chrome(ui::Context& ui, const Chrome& view, const settings::ReadingSettings& settings,
                const Board::Power::BatteryState& battery) {
        const bool vertical = view.vertical;
        const bool showChapter = settings::visible(settings.chapterVisibility, view.reading);
        const bool showProgress = settings::visible(settings.progressVisibility, view.reading);
        const bool showArrows = settings::visible(settings.arrowsVisibility, view.reading);
        const bool leftHanded = settings.leftHanded;
        const auto chapter = view.chapter, footer = view.footer;
        const auto batteryLabel = view.batteryLabel;
        const bool showBatteryLabel = settings::visible(settings.batteryLabelVisibility, view.reading);
        const bool showBatteryIcon = settings::visible(settings.batteryIconVisibility, view.reading);
        const auto muted = ui.color(ui::themes::Muted);
        if (vertical) {
            const int16_t width = ui.height(), height = ui.width();
            auto state = ui::Context::signature(showBatteryLabel ? batteryLabel : std::string_view{});
            state = ui::Context::signature(showProgress ? footer : std::string_view{}, state);
            state = ui::Context::signature(view.overlay, state);
            state = ui::Context::combine(state, showBatteryIcon);
            state = ui::Context::combine(state, battery.charging);
            state = ui::Context::combine(state, battery.status.percent);
            if (ui.redraw(ui::rotateClockwise(portraitTopStrip(width), width), state)) {
                if (showBatteryLabel || showBatteryIcon)
                    ui.portraitBattery(portraitBatteryRect(), battery.status.percent, battery.charging,
                                       showBatteryLabel ? batteryLabel : std::string_view{}, showBatteryIcon);
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
