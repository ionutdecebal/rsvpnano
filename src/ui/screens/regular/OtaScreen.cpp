#include "ui/screens/ScreenCommon.h"
#include "board/BoardConfig.h"

namespace screens {

    Action ota(ui::Context& ui, std::string_view firmwareVersion, Screen& screen, const OtaUpdater::Offer* offer) {
        if (const Action action = detail::navigation(ui, Screen::Ota, screen); action != Action::None)
            return action;
        const ui::Rect content = detail::tabContent(ui);
        constexpr int16_t gap = 6;
        const bool canSwitch = update::supportsNewUi(Board::Config::BOARD_ID);
        ui::Grid actions{content, static_cast<uint8_t>(canSwitch ? 4 : 3), 44, gap};
        if (ui.button(actions.next(), "<<"))
            screen = Screen::Device;
        if (ui.button(actions.next(), ui.text(UiText::CheckOnly)))
            return Action::OtaCheck;
        if (canSwitch && ui.button(actions.next(), "Try New UI"))
            return Action::OtaSwitchCheck;
        if (ui.button(actions.next(), "Confirm install", offer && offer->ready))
            return Action::OtaInstall;
        if (offer) {
            ui.label({content.x, static_cast<int16_t>(content.y + 50), content.w, 22},
                offer->version.empty() ? "Check a version before installing" : "Selected: " + offer->version, 1);
            ui.label({content.x, static_cast<int16_t>(content.y + 76), content.w, 34},
                offer->message, 1, ui::themes::Muted, ui::TextAlign::Start, 2);
        }
        const uint8_t versionSize = firmwareVersion.size() * 12U <= static_cast<size_t>(content.w) ? 2 : 1;
        ui.label({content.x, static_cast<int16_t>(content.y + content.h - 24), content.w, 24}, firmwareVersion,
                 versionSize, ui::themes::ColorRole::Muted, ui::TextAlign::Center);
        return Action::None;
    }

} // namespace screens
