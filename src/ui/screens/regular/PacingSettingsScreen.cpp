#include "ui/screens/ScreenCommon.h"
#include "ui/screens/ModernScreens.h"

namespace screens {
    bool pacingSettings(ui::Context& ui, settings::PacingSettings& config, Screen& screen) {
        bool changed = false;
        if (ui.modernPresentation()) {
            if (modern::header(ui, ui.text(UiText::WordPacing), screen, Screen::ReadingHub) == Action::Resume) screen = Screen::Reader;
            changed |= ui.slider({12, 49, 198, 50}, ui.text(UiText::LongWords), config.longWordDelayMs, " ms");
            changed |= ui.slider({222, 49, 198, 50}, ui.text(UiText::Complexity), config.complexWordDelayMs, " ms");
            changed |= ui.slider({432, 49, 196, 50}, ui.text(UiText::Punctuation), config.punctuationDelayMs, " ms");
            ui.label({16,112,430,48},"Extra time where words need a little more room.",13,ui::themes::Muted,ui::TextAlign::Start,2);
            if (ui::modern::button(ui,{480,111,148,53},ui.text(UiText::Reset),ui::modern::Style::Secondary)) {
                changed |= config != settings::PacingSettings{}; config = {};
            }
            return changed;
        }
        const ui::Rect content = detail::content(ui);
        constexpr int16_t gap = 6;
        constexpr int16_t backWidth = 56;
        constexpr int16_t resetWidth = 90;
        const int16_t rowHeight = static_cast<int16_t>((content.h - gap * 2) / 3);

        if (ui.button({content.x, content.y, backWidth, rowHeight}, "<<"))
            screen = ui.modernPresentation() ? Screen::ReadingHub : Screen::Settings;
        changed |= ui.slider({static_cast<int16_t>(content.x + backWidth + gap), content.y,
                              static_cast<int16_t>(content.w - backWidth - gap), rowHeight},
                             ui.text(UiText::LongWords), config.longWordDelayMs, " ms");

        const int16_t secondRowY = static_cast<int16_t>(content.y + rowHeight + gap);
        changed |= ui.slider({content.x, secondRowY, content.w, rowHeight}, ui.text(UiText::Complexity),
                             config.complexWordDelayMs, " ms");

        const int16_t thirdRowY = static_cast<int16_t>(secondRowY + rowHeight + gap);
        if (ui.button({content.x, thirdRowY, resetWidth, rowHeight}, ui.text(UiText::Reset))) {
            changed |= config != settings::PacingSettings{};
            config = {};
        }
        changed |= ui.slider({static_cast<int16_t>(content.x + resetWidth + gap), thirdRowY,
                              static_cast<int16_t>(content.w - resetWidth - gap), rowHeight},
                             ui.text(UiText::Punctuation), config.punctuationDelayMs, " ms");
        return changed;
    }

} // namespace screens
