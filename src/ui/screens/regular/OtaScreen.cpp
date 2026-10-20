#include "ui/screens/ScreenCommon.h"
#include "ui/screens/ModernScreens.h"

namespace screens {

    Action ota(ui::Context& ui, std::string_view firmwareVersion, Screen& screen) {
        if (const Action action = detail::navigation(ui, Screen::Ota, screen); action != Action::None)
            return action;
        const ui::Rect content = detail::tabContent(ui);
        if (ui.modernPresentation()) {
            ui.label({16, 56, 330, 32}, "Keep your reader up to date.", 21);
            ui.label({16, 94, 330, 40}, firmwareVersion, 13, ui::themes::Muted, ui::TextAlign::Start, 2);
            ui.label({16, 139, 330, 22}, "Wi-Fi update · keep the reader powered.", 11, ui::themes::Muted);
            if (ui::modern::button(ui, {374, 58, 250, 44}, ui.text(UiText::CheckOnly), ui::modern::Style::Secondary)) return Action::OtaCheck;
            if (ui::modern::button(ui, {374, 116, 250, 44}, ui.text(UiText::InstallUpdate), ui::modern::Style::Primary)) return Action::OtaInstall;
            return Action::None;
        }
        constexpr int16_t gap = 6;
        ui::Grid actions{content, 3, 56, gap};
        if (ui.button(actions.next(), "<<"))
            screen = Screen::Device;
        if (ui.button(actions.next(), ui.text(UiText::CheckOnly)))
            return Action::OtaCheck;
        if (ui.button(actions.next(), ui.text(UiText::InstallUpdate)))
            return Action::OtaInstall;
        const uint8_t versionSize = firmwareVersion.size() * 12U <= static_cast<size_t>(content.w) ? 2 : 1;
        ui.label({content.x, static_cast<int16_t>(content.y + content.h - 24), content.w, 24}, firmwareVersion,
                 versionSize, ui::themes::ColorRole::Muted, ui::TextAlign::Center);
        return Action::None;
    }

} // namespace screens
