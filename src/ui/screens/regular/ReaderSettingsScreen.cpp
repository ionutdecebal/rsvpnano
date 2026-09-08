#include "ui/screens/ScreenCommon.h"

#include "settings/SettingsRules.h"

namespace screens {

    bool readerSettings(ui::Context& ui, settings::ReadingSettings& config, Screen& screen) {
        bool changed = false;
        const ui::Rect content = detail::content(ui);
        constexpr int16_t gap = 4;
        constexpr int16_t backWidth = 56;
        constexpr int16_t rowHeight = 40;
        if (ui.button({content.x, content.y, backWidth, rowHeight}, "<<"))
            screen = Screen::Settings;
        const int16_t remainingWidth = static_cast<int16_t>(content.w - backWidth - gap * 2);
        const int16_t firstSettingWidth = static_cast<int16_t>(remainingWidth / 2);
        if (ui.setting({static_cast<int16_t>(content.x + backWidth + gap), content.y, firstSettingWidth, rowHeight},
                       ui.text(UiText::ReaderHand),
                       ui.text(config.leftHanded ? UiText::Left : UiText::Right), ui::SettingLayout::Inline)) {
            config.leftHanded = !config.leftHanded;
            changed = true;
        }
        if (ui.setting({static_cast<int16_t>(content.x + backWidth + gap * 2 + firstSettingWidth), content.y,
                        static_cast<int16_t>(remainingWidth - firstSettingWidth), rowHeight},
                       ui.text(UiText::ChapterScroll),
                       ui.text(config.chapterScrollReversed ? UiText::Reversed : UiText::Normal),
                       ui::SettingLayout::Inline)) {
            config.chapterScrollReversed = !config.chapterScrollReversed;
            changed = true;
        }

        const UiText footer = config.footerMetric == settings::FooterMetric::chapterTime ? UiText::ChapterTime
                            : config.footerMetric == settings::FooterMetric::bookTime    ? UiText::BookTime
                                                                                         : UiText::Percentage;
        const int16_t secondRowY = static_cast<int16_t>(content.y + rowHeight + gap);
        const int16_t halfWidth = static_cast<int16_t>((content.w - gap) / 2);
        if (ui.setting({content.x, secondRowY, halfWidth, rowHeight}, ui.text(UiText::Footer), ui.text(footer),
                       ui::SettingLayout::Inline)) {
            config.footerMetric = settings::cycleEnum(config.footerMetric);
            changed = true;
        }

        const UiText battery = config.batteryLabel == settings::BatteryLabel::timeRemaining ? UiText::TimeLeft
                             : config.batteryLabel == settings::BatteryLabel::voltage       ? UiText::Voltage
                                                                                            : UiText::Percentage;
        if (ui.setting({static_cast<int16_t>(content.x + halfWidth + gap), secondRowY, halfWidth, rowHeight},
                       ui.text(UiText::BatteryLabel), ui.text(battery), ui::SettingLayout::Inline)) {
            config.batteryLabel = settings::cycleEnum(config.batteryLabel);
            changed = true;
        }

        const int16_t visibilityY = static_cast<int16_t>(secondRowY + rowHeight + gap);
        ui.separator({content.x, visibilityY, content.w, 10}, ui.text(UiText::VisibleWhileReadingSection));
        ui::Grid visibility{{content.x, static_cast<int16_t>(visibilityY + 10), content.w,
                             static_cast<int16_t>(content.y + content.h - visibilityY - 10)},
                            3,
                            static_cast<int16_t>(content.y + content.h - visibilityY - 10),
                            gap};
        changed |= ui.toggle(visibility.next(), ui.text(UiText::Battery), config.batteryVisibleWhileReading);
        changed |= ui.toggle(visibility.next(), ui.text(UiText::Chapter), config.chapterVisibleWhileReading);
        changed |= ui.toggle(visibility.next(), ui.text(UiText::Progress), config.progressVisibleWhileReading);
        return changed;
    }

} // namespace screens
