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
        const auto area = ui.paintBounds(arrowArea(ui.width(), ui.height(), settings.leftHanded, wordHeight));
        const auto text = prepareArrows(ui, settings, reading, ghostHidden);
        ui.paint(area, [&](Arduino_GFX& output, ui::Rect target) {
            drawArrows(ui, output, settings, reading, text, wordHeight, static_cast<int16_t>(target.x - area.x),
                       static_cast<int16_t>(target.y - area.y), ghostHidden);
        });
    }

    ui::TextLayout prepareArrows(ui::Context& ui, const settings::ReadingSettings& settings, bool reading,
                                 bool ghostHidden) {
        if (!settings::visible(settings.arrowsVisibility, reading) && !ghostHidden)
            return {};
        const auto rect = horizontalChrome(ui.width(), ui.height(), settings.leftHanded).arrows;
        return ui.prepareText(rect, "<<", 2, ui::TextAlign::Center);
    }

    void drawArrows(ui::Context& ui, Arduino_GFX& output, const settings::ReadingSettings& settings, bool reading,
                    const ui::TextLayout& text, int16_t wordHeight, int16_t offsetX, int16_t offsetY,
                    bool ghostHidden) {
        const bool visible = settings::visible(settings.arrowsVisibility, reading);
        if (!visible && !ghostHidden)
            return;
        auto rect = arrowArea(ui.width(), ui.height(), settings.leftHanded, wordHeight);
        rect.x += offsetX;
        rect.y += offsetY;
        output.fillRect(rect.x, rect.y, rect.w, rect.h, ui.color(ui::themes::Background));
        ui.drawText(output, text, ui.blend(ui::themes::Muted, visible ? 255 : 64), offsetX, offsetY);
    }

    void horizontalChrome(ui::Context& ui, const Chrome& view, const settings::ReadingSettings& settings,
                          const Board::Power::BatteryState& battery) {
        const bool showProgress = settings::visible(settings.progressVisibility, view.reading);
        const auto layout = horizontalChrome(ui.width(), ui.height(), settings.leftHanded,
                                             showProgress || view.ghostHidden ? ui.textWidth(view.footer, 2) : 0);
        horizontalChrome(ui, view, settings, battery, layout);
    }

    void horizontalChrome(ui::Context& ui, const Chrome& view, const settings::ReadingSettings& settings,
                          const Board::Power::BatteryState& battery, const HorizontalChrome& layout) {
        const bool editing = layout.batteryParts.label.w > 0;
        auto footerState = ui::Context::signature(view.chapter);
        footerState = ui::Context::signature(view.footer, footerState);
        footerState = ui::Context::signature(view.locale, footerState);
        footerState = ui::Context::combine(footerState, static_cast<uint8_t>(settings.chapterVisibility));
        footerState = ui::Context::combine(footerState, static_cast<uint8_t>(settings.progressVisibility));
        footerState = ui::Context::combine(footerState, settings.leftHanded);
        footerState = ui::Context::combine(footerState, view.reading | (view.ghostHidden << 1));
        if (editing || ui.redraw({0, layout.chapter.y, ui.width(), layout.chapter.h}, footerState)) {
            const auto label = [&](ui::Rect rect, std::string_view text, settings::Visibility visibility,
                                   ui::TextAlign align) {
                const bool visible = settings::visible(visibility, view.reading);
                if (editing)
                    ui.label(rect, visible || view.ghostHidden ? text : std::string_view{}, layout.textSize,
                             ui::themes::Muted, align, 1, view.locale, visible ? 255 : 64);
                else if (visible || view.ghostHidden)
                    ui.drawText(rect, text, layout.textSize, ui.blend(ui::themes::Muted, visible ? 255 : 64), align, 1,
                                view.locale);
            };
            label(layout.chapter, view.chapter, settings.chapterVisibility,
                  settings.leftHanded ? ui::TextAlign::Right : ui::TextAlign::Left);
            label(layout.progress, view.footer, settings.progressVisibility, ui::TextAlign::Right);
        }
        const bool showBatteryIcon = settings::visible(settings.batteryIconVisibility, view.reading);
        const bool showBatteryLabel = settings::visible(settings.batteryLabelVisibility, view.reading);
        if (editing) {
            ui.battery(layout.batteryParts.icon, battery.status.percent, battery.charging, {},
                       showBatteryIcon || view.ghostHidden, showBatteryIcon ? 255 : 64);
            ui.label(layout.batteryParts.label,
                     showBatteryLabel || view.ghostHidden ? view.batteryLabel : std::string_view{}, 2,
                     ui::themes::Muted, ui::TextAlign::Left, 1, {}, showBatteryLabel ? 255 : 64);
        } else {
            ui.battery(layout.battery, battery.status.percent, battery.charging,
                       showBatteryLabel || view.ghostHidden ? view.batteryLabel : std::string_view{},
                       showBatteryIcon || view.ghostHidden, showBatteryIcon ? 255 : 64, showBatteryLabel ? 255 : 64);
        }
    }
} // namespace screens::readerLayout
