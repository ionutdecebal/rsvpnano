#include "ui/screens/ScreenCommon.h"

namespace screens {

    Action read(ui::Context& ui, std::string_view title, std::string_view author, uint8_t percent, Screen& screen) {
        if (const Action action = detail::navigation(ui, Screen::Read, screen); action != Action::None) {
            return action;
        }

        const ui::Rect area = detail::tabContent(ui);
        const bool wide = ui.width() >= 620 && ui.height() >= 150 && ui.height() <= 240;
        ui::Column column{area, static_cast<int16_t>(wide ? 18 : 10)};
        const ui::Rect header = column.next(64);
        constexpr int16_t kHeaderActionSize = 40;
        constexpr int16_t kHeaderGap = 8;
        const ui::Rect resume{header.x, header.y, static_cast<int16_t>(header.w - kHeaderActionSize - kHeaderGap),
                              header.h};
        const ui::Rect language{static_cast<int16_t>(header.x + header.w - kHeaderActionSize),
                                static_cast<int16_t>(header.y + (header.h - kHeaderActionSize) / 2), kHeaderActionSize,
                                kHeaderActionSize};
        author = author.empty() ? ui.text(UiText::Unknown) : author;
        char progress[4];
        size_t progressLength = 0;
        if (percent >= 100) {
            progress[progressLength++] = '1';
            progress[progressLength++] = '0';
            progress[progressLength++] = '0';
        } else {
            if (percent >= 10)
                progress[progressLength++] = static_cast<char>('0' + percent / 10);
            progress[progressLength++] = static_cast<char>('0' + percent % 10);
        }
        progress[progressLength++] = '%';
        const std::string_view progressText{progress, progressLength};
        if (ui.button(resume, title, true, ui::Icon::Bookmark, 2, author, progressText)) {
            return Action::Resume;
        }
        if (ui.iconButton(language, ui::Icon::Language)) {
            screen = Screen::BookFonts;
        }

        ui::Row actions{column.next(64), 10};
        const int16_t buttonWidth = static_cast<int16_t>((actions.bounds.w - actions.gap) / 2);
        if (ui.button(actions.next(buttonWidth), ui.text(UiText::Chapters))) {
            screen = Screen::Chapters;
        }
        if (ui.button(actions.next(buttonWidth), ui.text(UiText::Library))) {
            screen = Screen::Library;
        }
        return Action::None;
    }

} // namespace screens
