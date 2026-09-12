#include "ui/screens/watch/Layout.h"

namespace screens::detail {
    Action navigation(ui::Context& ui, Screen active, Screen& screen) {
        constexpr std::array faces{Screen::Read, Screen::Settings, Screen::Device, Screen::FocusTimers};
        constexpr std::array labels{UiText::Read, UiText::Settings, UiText::Device, UiText::Focus};
        constexpr std::array icons{ui::Icon::Books, ui::Icon::Edit, ui::Icon::Device, ui::Icon::Hourglass};
        const size_t selected = active <= Screen::Chapters || active == Screen::BookFonts       ? 0
                              : active >= Screen::Settings && active <= Screen::NetworkEdit     ? 1
                              : active >= Screen::FocusTimers && active <= Screen::FocusSession ? 3
                                                                                                : 2;
        const int16_t height = ui.height() < 240 ? 36 : 48;
        const auto area = content(ui);
        const int16_t y = area.y + area.h - height;
        const int16_t available = area.w - 12;
        const int16_t small = available / 6;
        int16_t x = area.x;
        for (size_t i = 0; i < faces.size(); ++i) {
            const int16_t width = i == selected ? static_cast<int16_t>(available - small * 3) : small;
            const ui::Rect rect{x, y, width, height};
            // Dock colors are stable face identities; the text still follows the active theme.
            const std::array<uint16_t, 4> colors{ui.color(ui::themes::Accent), ui::themes::rgb565(190, 130, 32),
                                                 ui::themes::rgb565(70, 132, 205), ui.color(ui::themes::BreakAccent)};
            if (ui.dockItem(rect, i == selected ? ui.text(labels[i]) : std::string_view{}, icons[i], colors[i]))
                screen = faces[i];
            x += width + 4;
        }
        return Action::None;
    }

    ui::Rect content(ui::Context& ui) {
        return watch::contentBounds(ui.width(), ui.height());
    }

    ui::Rect tabContent(ui::Context& ui) {
        auto area = content(ui);
        area.h -= ui.height() < 240 ? 42 : 54;
        return area;
    }
} // namespace screens::detail
