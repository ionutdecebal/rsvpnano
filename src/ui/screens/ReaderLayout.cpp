#include "ui/screens/ReaderLayout.h"

#include <cstdio>

namespace screens::readerLayout {
    ui::Rect batteryRect(int16_t width, int16_t height) {
        return horizontalChrome(width, height, false).battery;
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

    void drawArrows(ui::Context& ui, const settings::ReadingSettings& settings, bool reading, int16_t wordHeight,
                    bool ghostHidden) {
        const bool visible = settings::visible(settings.arrowsVisibility, reading);
        if (!visible && !ghostHidden)
            return;
        const auto rect = horizontalChrome(ui.width(), ui.height(), settings.leftHanded).arrows;
        const int16_t height = std::max(rect.h, wordHeight);
        ui.gfx().fillRect(rect.x, (ui.height() - height) / 2, rect.w, height, ui.color(ui::themes::Background));
        ui.drawText(rect, "<<", 2, ui.blend(ui::themes::Muted, visible ? 255 : 64), ui::TextAlign::Center);
    }

    void horizontalChrome(ui::Context& ui, const Chrome& view, const settings::ReadingSettings& settings,
                          const Board::Power::BatteryState& battery) {
        const bool showProgress = settings::visible(settings.progressVisibility, view.reading);
        const auto layout = horizontalChrome(ui.width(), ui.height(), settings.leftHanded,
                                             showProgress || view.ghostHidden ? ui.textWidth(view.footer, 2) : 0);
        auto footerState = ui::Context::signature(view.chapter);
        footerState = ui::Context::signature(view.footer, footerState);
        footerState = ui::Context::signature(view.locale, footerState);
        footerState = ui::Context::combine(footerState, static_cast<uint8_t>(settings.chapterVisibility));
        footerState = ui::Context::combine(footerState, static_cast<uint8_t>(settings.progressVisibility));
        footerState = ui::Context::combine(footerState, settings.leftHanded);
        footerState = ui::Context::combine(footerState, view.reading | (view.ghostHidden << 1));
        if (ui.redraw({0, layout.chapter.y, ui.width(), layout.chapter.h}, footerState)) {
            const auto label = [&](ui::Rect rect, std::string_view text, settings::Visibility visibility,
                                   ui::TextAlign align) {
                const bool visible = settings::visible(visibility, view.reading);
                if (visible || view.ghostHidden)
                    ui.drawText(rect, text, layout.textSize, ui.blend(ui::themes::Muted, visible ? 255 : 64), align, 1,
                                view.locale);
            };
            label(layout.chapter, view.chapter, settings.chapterVisibility,
                  settings.leftHanded ? ui::TextAlign::Right : ui::TextAlign::Left);
            label(layout.progress, view.footer, settings.progressVisibility, ui::TextAlign::Right);
        }
        const bool showBatteryIcon = settings::visible(settings.batteryIconVisibility, view.reading);
        const bool showBatteryLabel = settings::visible(settings.batteryLabelVisibility, view.reading);
        ui.battery(layout.battery, battery.status.percent, battery.charging,
                   showBatteryLabel || view.ghostHidden ? view.batteryLabel : std::string_view{},
                   showBatteryIcon || view.ghostHidden, showBatteryIcon ? 255 : 64, showBatteryLabel ? 255 : 64);
    }
} // namespace screens::readerLayout
