#include "ui/screens/watch/Layout.h"

namespace screens {
    bool readingSettings(ui::Context& ui, settings::ReadingSettings& config, Screen& screen) {
        auto area = watch::header(ui, ui.text(UiText::Reading), Screen::Settings, screen);
        const auto grid = ui.pagedGrid(area, 5, 1, 72);
        bool changed = false;
        const auto speed = grid.item(0);
        if (speed.w > 0) {
            const int16_t size = std::min<int16_t>(speed.h, speed.w / 2);
            ui::Rect dial{static_cast<int16_t>(speed.x + (speed.w - size) / 2), speed.y, size, speed.h};
            int value = config.wpm;
            changed |= ui.rotary(dial, value, config.wpm.min(), config.wpm.max(), config.wpm.step(), "WPM");
            if (ui.button({speed.x, speed.y, 48, speed.h}, "-")) {
                value = std::max<int>(config.wpm.min(), value - config.wpm.step());
                changed = true;
            }
            if (ui.button({static_cast<int16_t>(speed.x + speed.w - 48), speed.y, 48, speed.h}, "+")) {
                value = std::min<int>(config.wpm.max(), value + config.wpm.step());
                changed = true;
            }
            config.wpm = value;
        }
        if (watch::setting(ui, grid.item(1), UiText::Pause,
                           ui.text(config.pauseMode == settings::PauseMode::sentenceEnd ? UiText::SentenceEnd
                                                                                        : UiText::Instant))) {
            config.pauseMode = settings::cycleEnum(config.pauseMode);
            changed = true;
        }
        if (watch::setting(ui, grid.item(2), UiText::ReadingMode,
                           ui.text(config.mode == settings::ReadingMode::page ? UiText::ScrollMode
                                                                              : UiText::RsvpMode))) {
            config.mode = settings::cycleEnum(config.mode);
            changed = true;
        }
        if (watch::setting(ui, grid.item(3), UiText::ReaderHand,
                           ui.text(config.leftHanded ? UiText::Left : UiText::Right))) {
            config.leftHanded = !config.leftHanded;
            changed = true;
        }
        if (watch::setting(ui, grid.item(4), UiText::ChapterScroll,
                           ui.text(config.chapterScrollReversed ? UiText::Reversed : UiText::Normal))) {
            config.chapterScrollReversed = !config.chapterScrollReversed;
            changed = true;
        }
        return changed;
    }
} // namespace screens
