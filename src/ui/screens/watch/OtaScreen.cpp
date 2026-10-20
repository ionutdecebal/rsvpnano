#include "ui/screens/watch/Layout.h"

namespace screens {
    Action ota(ui::Context& ui, std::string_view firmwareVersion, Screen& screen, const OtaUpdater::Offer* offer) {
        auto area = watch::header(ui, ui.text(UiText::FirmwareUpdates), Screen::Device, screen);
        ui.label({area.x, area.y, area.w, 30}, offer && !offer->version.empty() ? std::string_view{offer->version} : firmwareVersion, 1, ui::themes::Muted, ui::TextAlign::Center);
        ui.label({area.x, static_cast<int16_t>(area.y+32), area.w, 42}, offer ? std::string_view{offer->message} : std::string_view{}, 1, ui::themes::Muted, ui::TextAlign::Center, 2);
        area.y += 78;
        area.h -= 78;
        ui::Grid grid{area, 2, area.h, 6};
        if (ui.card(grid.next(), ui.text(UiText::CheckOnly), {}, watch::textSize(ui)))
            return Action::OtaCheck;
        if (ui.card(grid.next(), "Confirm install", {}, watch::textSize(ui)) && offer && offer->ready)
            return Action::OtaInstall;
        return Action::None;
    }
} // namespace screens
