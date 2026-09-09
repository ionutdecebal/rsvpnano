#pragma once

#include "ui/screens/ReaderLayout.h"

namespace screens::appearanceLayout {
    struct Layout {
        ui::Rect back, page, reset, size, font, preview, dialPage;
        std::array<ui::Rect, 4> dials;
        bool pagedDials;
    };

    Layout make(int16_t width, int16_t height);

    struct Chrome {
        readerLayout::HorizontalChrome reader;
        ui::Rect footerFormat, batteryFormat, preview;
    };

    inline Chrome chrome(ui::Context& ui, bool leftHanded, ui::Rect page) {
        Chrome out{};
        out.reader = readerLayout::horizontalChrome(ui.width(), ui.height(), leftHanded, 96);
        auto& reader = out.reader;
        reader.chapter.y = reader.progress.y = ui.height() - 44;
        reader.chapter.h = reader.progress.h = 40;
        out.footerFormat = {static_cast<int16_t>(leftHanded ? reader.progress.x + reader.progress.w + 4
                                                            : reader.progress.x - 48),
                            static_cast<int16_t>(ui.height() - 44), 44, 40};
        if (leftHanded) {
            const int16_t right = reader.chapter.x + reader.chapter.w;
            reader.chapter.x = out.footerFormat.x + out.footerFormat.w + 4;
            reader.chapter.w = right - reader.chapter.x;
        } else {
            reader.chapter.w = out.footerFormat.x - 4 - reader.chapter.x;
        }
        out.batteryFormat = {static_cast<int16_t>(reader.battery.x - 48), 2, 44, 40};
        if (out.batteryFormat.x < page.x + page.w + 4) {
            reader.battery.y += 44;
            out.batteryFormat.y += 44;
        }
        // Reserve the longest label format; neither element moves when its text or visibility changes.
        reader.batteryParts = ui.batteryLayout(reader.battery, "0.00V");
        const int16_t top = std::max<int16_t>(44, reader.battery.y + reader.battery.h + 2);
        out.preview = {0, top, ui.width(), static_cast<int16_t>(ui.height() - 44 - top)};
        return out;
    }

    inline ui::Rect wordTarget(int16_t x, int16_t width, int16_t centerY, int16_t inkHeight, int16_t screenWidth,
                               int16_t screenHeight) {
        const int16_t w = std::max<int16_t>(44, width + 20), h = std::max<int16_t>(44, inkHeight + 12);
        const int16_t left = std::clamp<int>(x + (width - w) / 2, 0, screenWidth);
        const int16_t right = std::clamp<int>(x + (width + w) / 2, 0, screenWidth);
        const int16_t top = std::max<int16_t>(44, centerY - h / 2);
        return {left, top, static_cast<int16_t>(right - left),
                static_cast<int16_t>(std::max<int>(0, std::min<int>(screenHeight - 44, centerY + h / 2) - top))};
    }
} // namespace screens::appearanceLayout
