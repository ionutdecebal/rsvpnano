#include "ui/screens/watch/Layout.h"

namespace screens {
    Action settings(ui::Context& ui, Screen& screen) {
        detail::navigation(ui, Screen::Settings, screen);
        const auto area = detail::tabContent(ui);
        if (ui.height() < 240) {
            const auto grid =
                ui.pagedGrid({area.x, static_cast<int16_t>(area.y + 18), area.w, static_cast<int16_t>(area.h - 18)}, 6,
                             2, 48);
            ui.label({area.x, area.y, area.w, 18},
                     ui.text(grid.first < 4 ? UiText::ReadingSection : UiText::SystemSection), 2, ui::themes::Muted);
            constexpr std::array labels{UiText::Reading,      UiText::WordPacing, UiText::Typography,
                                        UiText::ReaderLayout, UiText::Display,    UiText::NetworkUpdates};
            constexpr std::array targets{Screen::ReadingSettings, Screen::PacingSettings,    Screen::TypographySettings,
                                         Screen::ReaderSettings,  Screen::InterfaceSettings, Screen::NetworkSettings};
            for (size_t i = grid.first; i < grid.first + grid.count; ++i)
                if (ui.card(grid.item(i), ui.text(labels[i]), {}, 2))
                    screen = targets[i];
            return Action::None;
        }
        const int16_t heading = ui.height() < 240 ? 18 : 24;
        const int16_t row = (area.h - 2 * heading - 8) / 3;
        ui.label({area.x, area.y, area.w, heading}, ui.text(UiText::ReadingSection), 2, ui::themes::Muted);
        ui::Grid grid{{area.x, static_cast<int16_t>(area.y + heading), area.w, static_cast<int16_t>(row * 2 + 4)},
                      2,
                      row,
                      4};
        constexpr std::array labels{UiText::Reading, UiText::WordPacing, UiText::Typography, UiText::ReaderLayout};
        constexpr std::array targets{Screen::ReadingSettings, Screen::PacingSettings, Screen::TypographySettings,
                                     Screen::ReaderSettings};
        for (size_t i = 0; i < labels.size(); ++i)
            if (ui.card(grid.next(), ui.text(labels[i]), {}, watch::textSize(ui)))
                screen = targets[i];
        const int16_t y = area.y + heading + row * 2 + 8;
        ui.label({area.x, y, area.w, heading}, ui.text(UiText::SystemSection), 2, ui::themes::Muted);
        ui::Grid system{{area.x, static_cast<int16_t>(y + heading), area.w, row}, 2, row, 4};
        if (ui.card(system.next(), ui.text(UiText::Display), {}, watch::textSize(ui)))
            screen = Screen::InterfaceSettings;
        if (ui.card(system.next(), ui.text(UiText::NetworkUpdates), {}, watch::textSize(ui)))
            screen = Screen::NetworkSettings;
        return Action::None;
    }
} // namespace screens
