#pragma once

#include "ui/Ui.h"

namespace ui {
    // A bounded grid page: invisible items have empty rectangles and cannot receive input.
    struct PagedGrid {
        Rect bounds;
        size_t first = 0;
        size_t count = 0;
        uint8_t columns = 1;
        int16_t rowHeight = 0;
        int16_t gap = 4;

        constexpr Rect item(size_t index) const {
            if (index < first || index - first >= count)
                return {};
            Grid grid{bounds, columns, rowHeight, gap, static_cast<uint16_t>(index - first)};
            return grid.next();
        }
    };

    struct CarouselGesture {
        int16_t startX = 0;
        int16_t startY = 0;
        bool active = false;

        int update(const Touch* touch, Rect bounds) {
            if (!touch)
                return 0;
            if (hasTouch(*touch, TouchStart)) {
                active = contains(bounds, touch->x, touch->y);
                startX = touch->x;
                startY = touch->y;
            }
            if (!active || !hasTouch(*touch, TouchRelease))
                return 0;
            active = false;
            const int dx = static_cast<int>(touch->x) - startX;
            const int dy = static_cast<int>(touch->y) - startY;
            return std::abs(dx) >= 24 && std::abs(dx) > std::abs(dy) ? (dx < 0 ? 1 : -1) : 0;
        }
    };

    constexpr size_t rotateIndex(size_t index, size_t count, int delta) {
        if (count == 0)
            return 0;
        index = std::min(index, count - 1);
        return delta < 0 ? (index == 0 ? count - 1 : index - 1) : delta > 0 ? (index + 1) % count : index;
    }
} // namespace ui
