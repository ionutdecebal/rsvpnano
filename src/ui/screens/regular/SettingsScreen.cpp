#include "ui/screens/ScreenCommon.h"

namespace screens {

    Action settings(ui::Context& ui, Screen& screen) {
        if (const Action action = detail::navigation(ui, Screen::Settings, screen); action != Action::None) {
            return action;
        }
        const ui::Rect content = detail::tabContent(ui);
        const uint8_t columns = content.w >= 280 ? 2 : 1;
        const int16_t rowHeight = columns == 2 ? 36 : 40;
        const int16_t gap = columns == 2 ? 6 : 4;
        ui.separator({content.x, content.y, content.w, 12}, ui.text(UiText::ReadingSection));
        ui::Grid grid{{content.x, static_cast<int16_t>(content.y + 18), content.w, content.h}, columns, rowHeight, gap};
        if (ui.button(grid.next(), ui.text(UiText::Reading)))
            screen = Screen::ReadingSettings;
        if (ui.button(grid.next(), ui.text(UiText::WordPacing)))
            screen = Screen::PacingSettings;
        if (ui.button(grid.next(), ui.text(UiText::Typography)))
            screen = Screen::TypographySettings;
        if (ui.button(grid.next(), ui.text(UiText::ReaderLayout)))
            screen = Screen::ReaderSettings;

        const int16_t readingRows = static_cast<int16_t>((4 + columns - 1) / columns);
        const int16_t systemY =
            static_cast<int16_t>(content.y + 24 + readingRows * rowHeight + (readingRows - 1) * gap);
        ui.separator({content.x, systemY, content.w, 12}, ui.text(UiText::SystemSection));
        ui::Grid system{{content.x, static_cast<int16_t>(systemY + 18), content.w, content.h}, columns, rowHeight, gap};
        if (ui.button(system.next(), ui.text(UiText::Display)))
            screen = Screen::InterfaceSettings;
        if (ui.button(system.next(), ui.text(UiText::NetworkUpdates)))
            screen = Screen::NetworkSettings;
        return Action::None;
    }

} // namespace screens
