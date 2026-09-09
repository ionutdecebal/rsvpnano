#include "ui/screens/ScreenCommon.h"

#include "settings/SettingsRules.h"

namespace screens {
    bool readingSettings(ui::Context& ui, settings::ReadingSettings& config, Screen& screen) {
        bool changed = false;
        const ui::Rect content = detail::content(ui);
        constexpr int16_t gap = 6;
        constexpr int16_t backWidth = 56;
        constexpr int16_t topHeight = 44;
        if (ui.button({content.x, content.y, backWidth, topHeight}, "<<"))
            screen = Screen::Settings;
        changed |= ui.slider({static_cast<int16_t>(content.x + backWidth + gap), content.y,
                              static_cast<int16_t>(content.w - backWidth - gap), topHeight},
                             ui.text(UiText::WordsPerMinute), config.wpm, " WPM");

        const int16_t sectionY = static_cast<int16_t>(content.y + topHeight + gap);
        ui.separator({content.x, sectionY, content.w, 10},
                     ui.text(UiText::BehaviorSection));

        const int16_t halfWidth = static_cast<int16_t>((content.w - gap) / 2);
        const int16_t rowY = static_cast<int16_t>(sectionY + 14);
        constexpr int16_t rowHeight = 40;
        if (ui.setting({content.x, rowY, halfWidth, rowHeight}, ui.text(UiText::Pause),
                       ui.text(config.pauseMode == settings::PauseMode::sentenceEnd ? UiText::SentenceEnd
                                                                                    : UiText::Instant),
                       ui::SettingLayout::Inline)) {
            config.pauseMode = settings::cycleEnum(config.pauseMode);
            changed = true;
        }

        if (ui.setting({static_cast<int16_t>(content.x + halfWidth + gap), rowY, halfWidth, rowHeight},
                       ui.text(UiText::ReadingMode),
                       ui.text(config.mode == settings::ReadingMode::page ? UiText::ScrollMode : UiText::RsvpMode),
                       ui::SettingLayout::Inline)) {
            config.mode = settings::cycleEnum(config.mode);
            changed = true;
        }

        const int16_t toggleY = static_cast<int16_t>(rowY + rowHeight + gap);
        const int16_t height = content.y + content.h - toggleY;
        if (ui.setting({content.x, toggleY, halfWidth, height}, ui.text(UiText::ReaderHand),
                       ui.text(config.leftHanded ? UiText::Left : UiText::Right), ui::SettingLayout::Inline)) {
            config.leftHanded = !config.leftHanded;
            changed = true;
        }
        if (ui.setting({static_cast<int16_t>(content.x + halfWidth + gap), toggleY, halfWidth, height},
                       ui.text(UiText::ChapterScroll),
                       ui.text(config.chapterScrollReversed ? UiText::Reversed : UiText::Normal),
                       ui::SettingLayout::Inline)) {
            config.chapterScrollReversed = !config.chapterScrollReversed;
            changed = true;
        }
        return changed;
    }

} // namespace screens
