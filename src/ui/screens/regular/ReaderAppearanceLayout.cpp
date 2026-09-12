#include "ui/screens/ReaderAppearanceLayout.h"

namespace screens::appearanceLayout {
    Layout make(int16_t width, int16_t height) {
        Layout out{};
        out.bounds = {0, 0, width, height};
        out.back = {4, 2, 44, 40};
        out.page = {52, 2, 144, 40};
        out.reset = {200, 2, 72, 40};
        out.size = {4, static_cast<int16_t>(height - 44), 108, 40};
        out.font = {120, static_cast<int16_t>(height - 44), static_cast<int16_t>(width - 336), 40};
        out.preview = {0, 44, static_cast<int16_t>(width - 212), static_cast<int16_t>(height - 88)};
        for (int i = 0; i < 4; ++i)
            out.dials[i] = {static_cast<int16_t>(width - 208 + (i % 2) * 104),
                            static_cast<int16_t>(2 + (i / 2) * (height / 2)), 100,
                            static_cast<int16_t>(height / 2 - 4)};
        return out;
    }
} // namespace screens::appearanceLayout
