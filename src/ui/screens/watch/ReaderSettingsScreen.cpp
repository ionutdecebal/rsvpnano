#include "ui/screens/watch/Layout.h"

namespace screens {
    bool readerSettings(ui::Context& ui, settings::ReadingSettings& config, Screen& screen) {
        auto area = watch::header(ui, ui.text(UiText::ReaderLayout), Screen::Settings, screen);
        auto grid = ui.pagedGrid(area, 8, 1, 56);
        bool changed = false;
        if (watch::setting(ui, grid.item(0), UiText::ReaderHand,
                           ui.text(config.leftHanded ? UiText::Left : UiText::Right))) {
            config.leftHanded = !config.leftHanded;
            changed = true;
        }
        if (watch::setting(ui, grid.item(1), UiText::ChapterScroll,
                           ui.text(config.chapterScrollReversed ? UiText::Reversed : UiText::Normal))) {
            config.chapterScrollReversed = !config.chapterScrollReversed;
            changed = true;
        }
        const UiText footer = config.footerMetric == settings::FooterMetric::chapterTime ? UiText::ChapterTime
                            : config.footerMetric == settings::FooterMetric::bookTime    ? UiText::BookTime
                                                                                         : UiText::Percentage;
        if (watch::setting(ui, grid.item(2), UiText::Footer, ui.text(footer))) {
            config.footerMetric = settings::cycleEnum(config.footerMetric);
            changed = true;
        }
        const UiText battery = config.batteryLabel == settings::BatteryLabel::timeRemaining ? UiText::TimeLeft
                             : config.batteryLabel == settings::BatteryLabel::voltage       ? UiText::Voltage
                                                                                            : UiText::Percentage;
        if (watch::setting(ui, grid.item(3), UiText::BatteryLabel, ui.text(battery))) {
            config.batteryLabel = settings::cycleEnum(config.batteryLabel);
            changed = true;
        }
        changed |= watch::toggle(ui, grid.item(4), UiText::Battery, config.batteryVisibleWhileReading);
        changed |= watch::toggle(ui, grid.item(5), UiText::Chapter, config.chapterVisibleWhileReading);
        changed |= watch::toggle(ui, grid.item(6), UiText::Progress, config.progressVisibleWhileReading);
        changed |= watch::toggle(ui, grid.item(7), UiText::Battery, config.batteryIconVisible);
        return changed;
    }
} // namespace screens
