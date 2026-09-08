#include "ui/screens/ScreenCommon.h"

namespace screens {

    Action ota(ui::Context& ui, std::string_view firmwareVersion, Screen& screen) {
        if (const Action action = detail::navigation(ui, Screen::Ota, screen); action != Action::None)
            return action;
        const ui::Rect content = detail::tabContent(ui);
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
