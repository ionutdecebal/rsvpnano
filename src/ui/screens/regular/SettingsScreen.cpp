#include "ui/screens/ScreenCommon.h"
#include "ui/screens/ModernScreens.h"
#include "ui/screens/OnboardingCopy.h"

namespace screens {

    Action settings(ui::Context& ui, Screen& screen) {
        if (ui.modernPresentation()) {
            if (auto action = modern::header(ui, ui.text(UiText::Settings), screen); action != Action::None)
                return action;
            using C=modern::Copy;
            constexpr C names[]{C::Reading,C::Appearance,C::Display,C::Connections,C::Device,C::Help};
            constexpr C details[]{C::ReadingHint,C::AppearanceHint,C::DisplayHint,C::ConnectionsHint,C::DeviceHint,C::HelpHint};
            constexpr Screen destinations[]{Screen::ReadingHub, Screen::ReaderAppearance, Screen::DisplayHub,
                                            Screen::Connections, Screen::Device, Screen::Help};
            for (size_t i=0; i<6; ++i)
                if (ui::modern::button(ui, {int16_t(12+(i%3)*210), int16_t(51+(i/3)*60), 198, 53}, modern::copy(ui,names[i]),
                                       ui::modern::Style::Secondary, true, modern::copy(ui,details[i]), 14, ui::TextAlign::Start)) screen=destinations[i];
            return Action::None;
        }
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
        auto appearance = grid.next();
        appearance.w = content.w;
        if (ui.button(appearance, ui.text(UiText::ReaderLayout)))
            screen = Screen::ReaderAppearance;

        const int16_t readingRows = static_cast<int16_t>((3 + columns - 1) / columns);
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
