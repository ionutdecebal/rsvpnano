#include <canvas/Arduino_Canvas.h>
#include <cstdio>
#include <filesystem>
#include "ui/screens/ChaptersScreen.h"
#include "ui/screens/ReaderLayout.h"
#include "ui/screens/ScreenCommon.h"

int main(int argc, char** argv) {
    if (argc != 2)
        return 1;
    std::filesystem::create_directories(argv[1]);
    for (const auto [width, height]: {std::pair{320, 172}, {448, 368}, {502, 410}, {480, 480}, {600, 450}}) {
        Arduino_Canvas panel(width, height, nullptr);
        if (!panel.begin(GFX_SKIP_OUTPUT_BEGIN))
            return 1;
        ui::Context ui(panel);
        const auto theme = ui::themes::defaultTheme();
        ui.setTheme(theme);
        settings::ReadingSettings reading;
        settings::PacingSettings pacing;
        screens::ChaptersScreen chapters;
        ReadingSession reader;
        const std::array<ChapterMarker, 3> markers{
            {{"A Parade in Erhenrang", 0}, {"The Place Inside the Blizzard", 5}, {"The Escape", 10}}};
        for (auto screen: {screens::Screen::Read, screens::Screen::Settings, screens::Screen::Device,
                           screens::Screen::ReadingSettings, screens::Screen::PacingSettings, screens::Screen::Chapters,
                           screens::Screen::Ota, screens::Screen::Reader}) {
            ui.beginFrame(static_cast<uint8_t>(screen));
            switch (screen) {
            case screens::Screen::Read:
                screens::read(ui, "The Left Hand of Darkness", "Ursula K. Le Guin", 42, screen);
                break;
            case screens::Screen::Settings:
                screens::settings(ui, screen);
                break;
            case screens::Screen::Device:
                screens::device(ui, true, 18, settings::NvsEncryptionState::Available, screen);
                break;
            case screens::Screen::ReadingSettings:
                screens::readingSettings(ui, reading, screen);
                break;
            case screens::Screen::PacingSettings:
                screens::pacingSettings(ui, pacing, screen);
                break;
            case screens::Screen::Chapters:
                chapters.draw(ui, markers, reader, reading, 0, screen);
                break;
            case screens::Screen::Ota:
                screens::ota(ui, "v0.1.1", screen);
                break;
            case screens::Screen::Reader: {
                const Board::Power::BatteryState battery{{true, 3.9f, 64}, 0, false};
                screens::readerLayout::chrome(ui, {.chapter = "Chapter 12", .footer = "42%", .batteryLabel = "64%"},
                                              reading, battery);
                screens::readerLayout::drawArrows(ui, reading, false, 68);
                break;
            }
            default:
                break;
            }
            ui.endFrame();
            const auto file = std::filesystem::path{argv[1]}
                            / (std::to_string(width) + "x" + std::to_string(height) + "-"
                               + std::to_string(static_cast<int>(screen)) + ".ppm");
            auto* output = std::fopen(file.string().c_str(), "wb");
            if (!output)
                return 1;
            std::fprintf(output, "P6\n%d %d\n255\n", width, height);
            for (int i = 0; i < width * height; ++i) {
                const uint16_t pixel = panel.getFramebuffer()[i];
                const unsigned char rgb[]{static_cast<unsigned char>(((pixel >> 11) & 31) * 255 / 31),
                                          static_cast<unsigned char>(((pixel >> 5) & 63) * 255 / 63),
                                          static_cast<unsigned char>((pixel & 31) * 255 / 31)};
                std::fwrite(rgb, 1, 3, output);
            }
            std::fclose(output);
        }
    }
}
