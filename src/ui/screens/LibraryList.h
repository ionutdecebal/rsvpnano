#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

#include "text/Utf8Text.h"

// Pure geometry for the vertical library list. Rows have individual heights
// (a title may wrap onto a second line); `Rows` carries the precomputed top
// offsets so every query stays cheap, constexpr, and testable off-device.
namespace screens::librarylist {

    constexpr int16_t kRowHeight = 46;
    constexpr int16_t kTitleLineHeight = 18;
    constexpr int16_t kRowGap = 4;

    constexpr int16_t rowHeight(size_t titleLines) {
        return static_cast<int16_t>(kRowHeight + (titleLines > 1 ? kTitleLineHeight * (titleLines - 1) : 0));
    }

    struct Rows {
        std::span<const int32_t> tops;
        std::span<const int16_t> heights;

        constexpr size_t size() const {
            return heights.size();
        }
        constexpr int32_t bottom(size_t index) const {
            return tops[index] + heights[index];
        }
    };

    // Fills `tops` from `heights`, one row after another with a gap between them.
    constexpr void layout(std::span<const int16_t> heights, std::vector<int32_t>& tops) {
        tops.resize(heights.size());
        int32_t cursor = 0;
        for (size_t index = 0; index < heights.size(); ++index) {
            tops[index] = cursor;
            cursor += heights[index] + kRowGap;
        }
    }

    constexpr int32_t contentHeight(Rows rows) {
        return rows.size() == 0 ? 0 : rows.bottom(rows.size() - 1);
    }

    constexpr int32_t maximumOffset(Rows rows, int16_t viewportHeight) {
        return std::max<int32_t>(0, contentHeight(rows) - viewportHeight);
    }

    constexpr int32_t clampOffset(Rows rows, int32_t offset, int16_t viewportHeight) {
        return std::clamp<int32_t>(offset, 0, maximumOffset(rows, viewportHeight));
    }

    // The nearest scroll offset to `offset` that keeps row `index` fully visible.
    constexpr int32_t offsetToReveal(Rows rows, size_t index, int32_t offset, int16_t viewportHeight) {
        if (rows.size() == 0)
            return 0;
        index = std::min(index, rows.size() - 1);
        if (rows.tops[index] < offset)
            offset = rows.tops[index];
        else if (rows.bottom(index) > offset + viewportHeight)
            offset = rows.bottom(index) - viewportHeight;
        return clampOffset(rows, offset, viewportHeight);
    }

    // First row whose bottom edge lies below the viewport top.
    constexpr size_t firstVisible(Rows rows, int32_t offset) {
        size_t low = 0;
        size_t high = rows.size();
        while (low < high) {
            const size_t middle = low + (high - low) / 2;
            if (rows.bottom(middle) <= offset)
                low = middle + 1;
            else
                high = middle;
        }
        return low;
    }

    // One past the last row whose top edge lies above the viewport bottom.
    constexpr size_t pastLastVisible(Rows rows, int32_t offset, int16_t viewportHeight) {
        const int32_t limit = offset + viewportHeight;
        size_t low = 0;
        size_t high = rows.size();
        while (low < high) {
            const size_t middle = low + (high - low) / 2;
            if (rows.tops[middle] < limit)
                low = middle + 1;
            else
                high = middle;
        }
        return low;
    }

    // Row under a viewport-local y, or `size()` when the point is in a gap or past the end.
    constexpr size_t rowAt(Rows rows, int32_t offset, int32_t localY) {
        const int32_t contentY = offset + localY;
        if (rows.size() == 0 || contentY < 0)
            return rows.size();
        const size_t next = pastLastVisible(rows, contentY, 1);
        if (next == 0)
            return rows.size();
        const size_t index = next - 1;
        return contentY < rows.bottom(index) ? index : rows.size();
    }

    // Greedy word wrap at `capacity` codepoints per line. Words longer than a line are
    // split. Returns the line count; `lines` (optional) receives the line views.
    inline size_t wrapLines(std::string_view text, size_t capacity, std::vector<std::string_view>* lines) {
        if (lines != nullptr)
            lines->clear();
        size_t count = 0;
        while (!text.empty()) {
            while (!text.empty() && text.front() == ' ')
                text.remove_prefix(1);
            if (text.empty())
                break;
            std::string_view line = text;
            if (capacity > 0 && Utf8Text::count(text) > capacity) {
                const size_t limit = Utf8Text::prefixBytes(text, capacity);
                const size_t space = text.rfind(' ', limit);
                line = text.substr(0, space == std::string_view::npos || space == 0 ? limit : space);
            }
            if (lines != nullptr)
                lines->push_back(line);
            ++count;
            text.remove_prefix(line.size());
        }
        if (count == 0) {
            if (lines != nullptr)
                lines->push_back({});
            count = 1;
        }
        return count;
    }

    struct Thumb {
        int16_t y = 0;
        int16_t h = 0;
    };

    // Scrollbar thumb inside a track of `trackHeight` pixels.
    constexpr Thumb scrollThumb(Rows rows, int32_t offset, int16_t viewportHeight, int16_t trackHeight) {
        const int32_t content = contentHeight(rows);
        if (content <= viewportHeight || trackHeight <= 0)
            return {0, trackHeight};
        const int16_t height =
            std::max<int16_t>(8, static_cast<int16_t>(static_cast<int32_t>(trackHeight) * viewportHeight / content));
        const int32_t travel = trackHeight - height;
        const int32_t maximum = maximumOffset(rows, viewportHeight);
        const int16_t y = static_cast<int16_t>(
            maximum == 0 ? 0 : travel * clampOffset(rows, offset, viewportHeight) / maximum);
        return {y, height};
    }

} // namespace screens::librarylist
