#include "ui/screens/ReaderAppearanceLayout.h"
#include "ui/screens/watch/Layout.h"

namespace screens::appearanceLayout {
    Layout make(int16_t width, int16_t height) {
        const auto area = watch::contentBounds(width, height);
        width = area.w;
        height = area.h;
        Layout out{};
        out.bounds = area;
        out.back = {4, 2, 44, 40};
        out.page = {52, 2, std::clamp<int16_t>(width - 132, 44, 144), 40};
        const int16_t resetX = out.page.x + out.page.w + 4;
        out.reset = {resetX, 2, std::min<int16_t>(72, width - resetX - 4), 40};
        out.size = {4, static_cast<int16_t>(height - 42), std::min<int16_t>(108, width / 2 - 8), 40};
        const int16_t fontX = out.size.x + out.size.w + 8;
        out.font = {fontX, static_cast<int16_t>(height - 42), static_cast<int16_t>(width - fontX - 4), 40};
        out.preview = {0, static_cast<int16_t>(height / 2 - 44), width, 88};
        out.pagedDials = height < 300 && width >= 260;
        if (out.pagedDials) {
            out.page.w = 44;
            out.reset = {static_cast<int16_t>(width - 76), 2, 72, 40};
            out.dialPage = {140, 2, static_cast<int16_t>(width - 220), 40};
            out.preview = {0, 44, width, static_cast<int16_t>(height - 88)};
            const int16_t dialWidth = (width - 12) / 2;
            for (int i = 0; i < 4; ++i)
                out.dials[i] = {static_cast<int16_t>(4 + (i % 2) * (dialWidth + 4)), 44, dialWidth,
                                static_cast<int16_t>(height - 90)};
        } else {
            const int16_t dialWidth = std::min<int16_t>(144, (width - 24) / 2);
            const int16_t left = (width - dialWidth * 2 - 12) / 2;
            const int16_t dialHeight = (height - 184) / 2;
            for (int i = 0; i < 4; ++i)
                out.dials[i] = {static_cast<int16_t>(left + (i % 2) * (dialWidth + 12)),
                                static_cast<int16_t>(i < 2 ? 46 : height / 2 + 46), dialWidth, dialHeight};
        }
        for (auto* rect: {&out.back, &out.page, &out.reset, &out.size, &out.font, &out.preview, &out.dialPage})
            if (rect->w > 0 && rect->h > 0) {
                rect->x += area.x;
                rect->y += area.y;
            }
        for (auto& rect: out.dials) {
            rect.x += area.x;
            rect.y += area.y;
        }
        return out;
    }
} // namespace screens::appearanceLayout
