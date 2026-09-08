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
    ui::Rect batteryRect(int16_t width) {
        return {static_cast<int16_t>(width - 112), 0, 104, 42};
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

    void chrome(ui::Context& ui, const Chrome& view, const settings::ReadingSettings& settings,
                const Board::Power::BatteryState& battery) {
        const bool vertical = view.vertical, showChapter = view.showChapter, showProgress = view.showProgress;
        const bool showBattery = view.showBattery, leftHanded = settings.leftHanded;
        const auto chapter = view.chapter, footer = view.footer;
        const auto batteryLabel = view.batteryLabel;
        const bool showBatteryIcon = settings.batteryIconVisible && showBattery;
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
                if (showBattery)
                    ui.portraitBattery(portraitBatteryRect(), battery.status.percent, battery.charging, batteryLabel,
                                       showBatteryIcon);
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
                          ui::Context::combine(Fnv1a::kOffsetBasis, leftHanded)))
                ui.portraitText(portraitPreviousRect(width, height, leftHanded), "<<", 2, muted,
                                ui::TextAlign::Center);
        } else {
            const uint8_t size = ui.height() < 240 ? 2 : 3;
            ui.battery(batteryRect(ui.width()), battery.status.percent, battery.charging,
                       showBattery ? batteryLabel : std::string_view{}, showBatteryIcon);
            const int16_t footerWidth = ui.width() / 3;
            const int16_t y = ui.height() - 42;
            ui.label({static_cast<int16_t>(leftHanded ? footerWidth + 12 : 8), y,
                      static_cast<int16_t>(ui.width() - footerWidth - 20), 40},
                     showChapter ? chapter : std::string_view{}, size, ui::themes::Muted, ui::TextAlign::Start);
            ui.label({static_cast<int16_t>(leftHanded ? 8 : ui.width() - footerWidth - 8), y, footerWidth, 40},
                     showProgress ? footer : std::string_view{}, size, ui::themes::Muted, ui::TextAlign::Right);
        }
    }
} // namespace screens::readerLayout
