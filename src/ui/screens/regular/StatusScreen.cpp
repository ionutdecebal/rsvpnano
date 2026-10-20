#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "ui/screens/Screens.h"
#include "ui/Modern.h"

namespace screens {

    namespace {
        void startupStatus(ui::Context& ui, std::string_view title, std::string_view line1,
                           std::string_view line2, int progressValue) {
            using namespace ui::themes;
            ui::modern::logo(ui, 22, 24, millis());
            ui.label({304, 12, 316, 27}, ui.startupPresentation() ? "STARTING UP" : "YOUR READING SPACE", 11, Muted);
            // Own all text regions on every call, including empty lines, so
            // changing stages cannot leave an old filename or failure behind.
            ui.label({304, 43, 316, 38}, title, 2, Foreground, ui::TextAlign::Start, 2);
            ui.label({304, 85, 316, 40}, line1, 1, Muted, ui::TextAlign::Start, 2);
            ui.label({304, 130, 246, 22}, line2, 1, Muted);
            const int progress = std::clamp(progressValue, 0, 100);
            ui.label({560, 132, 56, 22}, progressValue < 0 ? std::string{} : std::to_string(progress) + "%",
                     1, Accent, ui::TextAlign::Right);
            constexpr ui::Rect rail{24, 160, 592, 2};
            const uint32_t state = progressValue < 0 ? 101 : static_cast<uint32_t>(progress);
            if (ui.redraw(rail, state)) {
                ui.paint(rail, [&](Arduino_GFX& gfx, ui::Rect target) {
                    gfx.fillRect(target.x, target.y, target.w, target.h, ui.color(ProgressTrack));
                    // Unknown progress remains a quiet rule, not a made-up
                    // percentage or a timer pretending to measure real work.
                    if (progressValue >= 0 && progress > 0)
                        gfx.fillRect(target.x, target.y, target.w * progress / 100, target.h, ui.color(Accent));
                });
            }
        }
    } // namespace

    void status(ui::Context& ui, std::string_view title, std::string_view line1, std::string_view line2,
                int progressValue) {
        rememberStatus(title, line1, line2, progressValue);
        if (ui.width() == 640 && ui.height() == 172) {
            ui.setModernPresentation(true);
            ui.setModernReadingPalette(false);
            ui.beginFrame(static_cast<uint8_t>(Screen::Status));
            startupStatus(ui, title, line1, line2, progressValue);
            ui.endFrame();
            return;
        }
        // Other panels keep their established layout. Status may be entered
        // outside App's dispatcher, so explicitly select presentation here.
        ui.setModernPresentation(false);
        ui.setModernReadingPalette(false);
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
