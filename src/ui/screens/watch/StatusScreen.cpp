#include "ui/screens/watch/Layout.h"

namespace screens {
    void status(ui::Context& ui, std::string_view title, std::string_view line1, std::string_view line2, int progress) {
        ui.beginFrame(static_cast<uint8_t>(Screen::Status));
        auto area = detail::content(ui);
        const bool micro = ui.height() < 240;
        const int16_t ring = progress >= 0 ? std::min<int16_t>(micro ? 80 : 120, area.h) : 0;
        if (ring) {
            ui.progressRing({area.x, static_cast<int16_t>(area.y + (area.h - ring) / 2), ring, ring}, progress);
            area.x += ring + 8;
            area.w -= ring + 8;
        }
        ui::Column lines{area, 4};
        const int16_t row = (area.h - 8) / 3;
        ui.label(lines.next(row), title, watch::textSize(ui), ui::themes::Accent, ui::TextAlign::Center, 2);
        ui.label(lines.next(row), line1, 2, ui::themes::Foreground, ui::TextAlign::Center, 2);
        ui.label(lines.next(row), line2, 2, ui::themes::Muted, ui::TextAlign::Center, 2);
        ui.endFrame();
    }
} // namespace screens
