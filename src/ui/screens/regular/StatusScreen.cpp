#include "ui/screens/Screens.h"

namespace screens {

    void status(ui::Context& ui, std::string_view title, std::string_view line1, std::string_view line2,
                int progressValue) {
        ui.beginFrame(static_cast<uint8_t>(Screen::Status));
        const int16_t width = std::min<int16_t>(static_cast<int16_t>(ui.width() - 24), 560);
        const int16_t x = static_cast<int16_t>((ui.width() - width) / 2);
        const int16_t totalHeight = static_cast<int16_t>(48 + (!line1.empty() ? 52 : 0) + (!line2.empty() ? 40 : 0)
                                                         + (progressValue >= 0 ? 22 : 0));
        int16_t y = std::max<int16_t>(8, static_cast<int16_t>((ui.height() - totalHeight) / 2));

        ui.label({x, y, width, 48}, title, 3, ui::themes::ColorRole::Accent, ui::TextAlign::Center, 2);
        y = static_cast<int16_t>(y + 48);
        if (!line1.empty()) {
            y = static_cast<int16_t>(y + 4);
            ui.label({x, y, width, 48}, line1, 3, ui::themes::ColorRole::Foreground, ui::TextAlign::Center, 2);
            y = static_cast<int16_t>(y + 48);
        }
        if (!line2.empty()) {
            y = static_cast<int16_t>(y + 4);
            ui.label({x, y, width, 36}, line2, 2, ui::themes::ColorRole::Muted, ui::TextAlign::Center, 2);
            y = static_cast<int16_t>(y + 36);
        }
        if (progressValue >= 0) {
            const int16_t progressWidth = std::min<int16_t>(width, 480);
            ui.progress({static_cast<int16_t>((ui.width() - progressWidth) / 2), static_cast<int16_t>(y + 8),
                         progressWidth, 14},
                        progressValue);
        }
        ui.endFrame();
    }

} // namespace screens
