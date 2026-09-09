#include "ui/screens/ReaderAppearanceLayout.h"

namespace screens::appearanceLayout {
    Layout make(int16_t width, int16_t height) {
        Layout out{};
        out.back = {4, 2, 44, 40};
        out.page = {52, 2, 144, 40};
        out.reset = {200, 2, 72, 40};
        out.size = {4, static_cast<int16_t>(height - 44), 108, 40};
        out.font = {120, static_cast<int16_t>(height - 44), static_cast<int16_t>(width - 124), 40};
        out.preview = {0, static_cast<int16_t>(height / 2 - 44), width, 88};
        out.pagedDials = height < 300;
        if (out.pagedDials) {
            out.page.w = 84;
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
        return out;
    }
} // namespace screens::appearanceLayout
