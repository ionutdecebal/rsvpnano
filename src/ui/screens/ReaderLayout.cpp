#include "ui/screens/ReaderLayout.h"

#include <cstdio>

namespace screens::readerLayout {
    ui::Rect batteryRect(int16_t width, int16_t height) {
        const auto layout = horizontalChrome(width, height, false);
        return {layout.batteryIcon.x, layout.batteryIcon.y,
                static_cast<int16_t>(layout.batteryLabel.x + layout.batteryLabel.w - layout.batteryIcon.x),
                layout.batteryIcon.h};
    }

    std::string batteryText(settings::BatteryLabel format, const Board::Power::BatteryState& battery) {
        char text[12];
        if (format == settings::BatteryLabel::voltage && battery.status.voltage > 0)
            std::snprintf(text, sizeof(text), "%.2fV", battery.status.voltage);
        else if (format == settings::BatteryLabel::timeRemaining) {
            constexpr uint32_t nominalRuntimeMinutes = 600;
            const uint32_t minutes = battery.status.percent * nominalRuntimeMinutes / 100;
            if (minutes >= 60)
                std::snprintf(text, sizeof(text), "%lu.%luh", static_cast<unsigned long>(minutes / 60),
                              static_cast<unsigned long>(minutes % 60 / 6));
            else
                std::snprintf(text, sizeof(text), "%lum", static_cast<unsigned long>(minutes));
        } else
            std::snprintf(text, sizeof(text), "%u%%", static_cast<unsigned int>(battery.status.percent));
        return text;
    }

    std::string progressText(ui::Context& ui, settings::FooterMetric format, uint8_t percent, uint32_t minutes) {
        if (format == settings::FooterMetric::percentage)
            return std::to_string(percent) + "%";
        std::string text{
            ui.text(format == settings::FooterMetric::chapterTime ? UiText::ChapterShort : UiText::BookShort)};
        text += ' ';
        text += minutes >= 60 ? std::to_string(minutes / 60) + "h" : std::to_string(minutes) + "m";
        return text;
    }

    void horizontalChrome(ui::Context& ui, const Chrome& view, const settings::ReadingSettings& settings,
                          const Board::Power::BatteryState& battery) {
        const auto layout = horizontalChrome(ui.width(), ui.height(), settings.leftHanded);
        const auto label = [&](ui::Rect rect, std::string_view text, settings::Visibility visibility, uint8_t size,
                               ui::TextAlign align) {
            const bool visible = settings::visible(visibility, view.reading);
            ui.label(rect, visible || view.ghostHidden ? text : std::string_view{}, size, ui::themes::Muted, align, 1,
                     view.locale, visible ? 255 : 64);
        };
        label(layout.chapter, view.chapter, settings.chapterVisibility, layout.textSize,
              settings.leftHanded ? ui::TextAlign::Right : ui::TextAlign::Left);
        label(layout.progress, view.footer, settings.progressVisibility, layout.textSize, ui::TextAlign::Right);
        label(layout.batteryLabel, view.batteryLabel, settings.batteryLabelVisibility, 2, ui::TextAlign::Right);
        const bool showBatteryIcon = settings::visible(settings.batteryIconVisibility, view.reading);
        ui.battery(layout.batteryIcon, battery.status.percent, battery.charging, {},
                   showBatteryIcon || view.ghostHidden, showBatteryIcon ? 255 : 64);
        const bool showArrows = settings::visible(settings.arrowsVisibility, view.reading);
        if (ui.redraw(layout.arrows, ui::Context::combine(view.topState, showArrows | (view.ghostHidden << 1)))
            && (showArrows || view.ghostHidden))
            ui.drawText(layout.arrows, "<<", 2, ui.blend(ui::themes::Muted, showArrows ? 255 : 64),
                        ui::TextAlign::Center);
    }
} // namespace screens::readerLayout
