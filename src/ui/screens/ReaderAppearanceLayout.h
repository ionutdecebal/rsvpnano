#pragma once

#include "ui/Ui.h"

namespace screens::appearanceLayout {
    struct Layout {
        ui::Rect back, page, reset, size, font, preview, dialPage;
        std::array<ui::Rect, 4> dials;
        bool pagedDials;
    };

    Layout make(int16_t width, int16_t height);

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
