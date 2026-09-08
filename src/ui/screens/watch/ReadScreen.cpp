#include "ui/screens/watch/Layout.h"

namespace screens {
    Action read(ui::Context& ui, std::string_view title, std::string_view author, uint8_t percent, Screen& screen) {
        detail::navigation(ui, Screen::Read, screen);
        const auto area = detail::tabContent(ui);
        const int16_t resumeHeight = area.h * 2 / 5;
        const int16_t ringSize = std::min<int16_t>(resumeHeight, 86);
        ui.progressRing({area.x, area.y, ringSize, ringSize}, percent);
        if (ui.card({static_cast<int16_t>(area.x + ringSize + 6), area.y, static_cast<int16_t>(area.w - ringSize - 6),
                     resumeHeight},
                    title, author, watch::textSize(ui)))
            return Action::Resume;
        ui::Grid actions{{area.x, static_cast<int16_t>(area.y + resumeHeight + 6), area.w,
                          static_cast<int16_t>(area.h - resumeHeight - 6)},
                         3,
                         static_cast<int16_t>(area.h - resumeHeight - 6),
                         6};
        if (ui.card(actions.next(), ui.text(UiText::Library), {}, watch::textSize(ui)))
            screen = Screen::Library;
        if (ui.card(actions.next(), ui.text(UiText::Chapters), {}, watch::textSize(ui)))
            screen = Screen::Chapters;
        if (ui.card(actions.next(), ui.text(UiText::Typeface), {}, watch::textSize(ui)))
            screen = Screen::BookFonts;
        return Action::None;
    }
} // namespace screens
