#include "ui/screens/watch/Layout.h"

namespace screens {
    Action ota(ui::Context& ui, std::string_view firmwareVersion, Screen& screen) {
        auto area = watch::header(ui, ui.text(UiText::FirmwareUpdates), Screen::Device, screen);
        ui.label({area.x, area.y, area.w, 30}, firmwareVersion, 2, ui::themes::Muted, ui::TextAlign::Center);
        area.y += 34;
        area.h -= 34;
        ui::Grid grid{area, 2, area.h, 6};
        if (ui.card(grid.next(), ui.text(UiText::CheckOnly), {}, watch::textSize(ui)))
            return Action::OtaCheck;
        if (ui.card(grid.next(), ui.text(UiText::InstallUpdate), {}, watch::textSize(ui)))
            return Action::OtaInstall;
        return Action::None;
    }
} // namespace screens
