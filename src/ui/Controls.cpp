#include "ui/Layouts.h"

#include <cmath>
#include <cstdio>
#include "text/Utf8Text.h"

namespace ui {
    bool Context::card(Rect rect, std::string_view title, std::string_view detail, uint8_t textSize,
                       themes::ColorRole role, Icon icon, bool enabled, uint8_t alpha) {
        if (rect.w <= 0 || rect.h <= 0)
            return false;
        auto state = signature(title, signature(detail));
        state = combine(state, textSize);
        state = combine(state, role);
        state = combine(state, static_cast<uint8_t>(icon));
        state = combine(state, enabled);
        state = combine(state, alpha);
        if (redraw(rect, state)) {
            const auto surface = blend(themes::SurfaceActive, alpha / 2);
            const auto ink = blend(enabled ? themes::Foreground : themes::Muted, alpha);
            gfx_.fillRoundRect(rect.x, rect.y, rect.w, rect.h, 8, surface);
            gfx_.drawRoundRect(rect.x, rect.y, rect.w, rect.h, 8, blend(role, alpha));
            const int16_t iconWidth = icon == Icon::None ? 0 : std::min<int16_t>(32, rect.w / 4);
            if (iconWidth)
                drawIcon({static_cast<int16_t>(rect.x + 6), rect.y, iconWidth, rect.h}, icon, blend(role, alpha),
                         surface);
            const int16_t x = rect.x + 6 + iconWidth;
            const int16_t width = std::max<int16_t>(0, rect.w - 12 - iconWidth);
            const int16_t detailHeight = detail.empty() ? 0 : std::min<int16_t>(36, rect.h / 3);
            fixedText({x, static_cast<int16_t>(rect.y + 3), width, static_cast<int16_t>(rect.h - detailHeight - 6)},
                      title, textSize, ink, TextAlign::Center, 8);
            if (!detail.empty())
                fixedText({x, static_cast<int16_t>(rect.y + rect.h - detailHeight - 3), width, detailHeight}, detail, 2,
                          blend(role, alpha), TextAlign::Center, 1);
        }
        return tap(rect, enabled);
    }

    size_t Context::fixedText(Rect rect, std::string_view text, uint8_t textSize, uint16_t ink, TextAlign align,
                              uint8_t maxLines, bool ellipsis) {
        if (rect.w <= 0 || rect.h <= 0 || text.empty())
            return 0;
        const size_t originalLength = text.size();
        textSize = std::max<uint8_t>(1, textSize);
        const auto* assets = fontAssetsFor(text);
        const int cell = (assets ? locales::uiFontCellWidth(assets->font) : 6) * textSize;
        const int height = textHeightFor(text, textSize);
        const size_t capacity = rect.w / std::max(1, cell);
        const int lines = std::min<int>(maxLines, rect.h / std::max(1, height));
        if (!capacity || lines <= 0)
            return 0;
        std::array<std::string_view, 8> wrapped{};
        int used = 0;
        while (!text.empty() && used < std::min<int>(lines, wrapped.size())) {
            size_t end = Utf8Text::prefixBytes(text, capacity);
            if (end < text.size() && used + 1 < lines) {
                const auto space = text.rfind(' ', end);
                if (space != std::string_view::npos && space > 0)
                    end = space;
            }
            wrapped[used++] = text.substr(0, end);
            text.remove_prefix(end);
            while (!text.empty() && text.front() == ' ')
                text.remove_prefix(1);
        }
        int16_t y = rect.y + (rect.h - used * height) / 2;
        for (int i = 0; i < used; ++i) {
            std::string clipped;
            auto line = wrapped[i];
            if (ellipsis && i + 1 == used && !text.empty()) {
                const size_t dots = std::min<size_t>(3, capacity);
                clipped =
                    std::string{line.substr(0, Utf8Text::prefixBytes(line, capacity - dots))} + std::string(dots, '.');
                line = clipped;
            }
            drawText({rect.x, y, rect.w, static_cast<int16_t>(height)}, line, textSize, ink, align);
            y += height;
        }
        return originalLength - text.size();
    }

    bool Context::dockItem(Rect rect, std::string_view label, Icon icon, uint16_t accent) {
        if (redraw(rect, combine(signature(label), accent))) {
            const auto surface = color(label.empty() ? themes::Surface : themes::SurfaceActive);
            gfx_.fillRoundRect(rect.x, rect.y, rect.w, rect.h, 7, surface);
            gfx_.drawRoundRect(rect.x, rect.y, rect.w, rect.h, 7, accent);
            const int16_t iconWidth = std::min<int16_t>(30, rect.w - 8);
            const int16_t x = label.empty() ? rect.x + (rect.w - iconWidth) / 2 : rect.x + 6;
            drawIcon({x, static_cast<int16_t>(rect.y + 4), iconWidth, static_cast<int16_t>(rect.h - 8)}, icon, accent,
                     surface);
            if (!label.empty())
                fixedText({static_cast<int16_t>(x + iconWidth + 3), rect.y,
                           static_cast<int16_t>(rect.w - iconWidth - 15), rect.h},
                          label, 2, color(themes::Foreground));
        }
        return tap(rect);
    }

    void Context::progressRing(Rect rect, int value, int maximum, themes::ColorRole role) {
        if (rect.w < 8 || rect.h < 8)
            return;
        const int percent = maximum > 0 ? static_cast<int>(std::clamp<int64_t>(100LL * value / maximum, 0, 100)) : 0;
        if (!redraw(rect, combine(combine(signature("ring"), percent), role)))
            return;
        const int16_t radius = std::min(rect.w, rect.h) / 2 - 2;
        const int16_t cx = rect.x + rect.w / 2, cy = rect.y + rect.h / 2;
        constexpr int segments = 60;
        for (int i = 0; i < segments; ++i) {
            const float angle = (i * 6 - 90) * 0.01745329252f;
            const auto ink = color(i * 100 < percent * segments ? role : themes::ProgressTrack);
            gfx_.drawLine(cx + std::cos(angle) * (radius - 4), cy + std::sin(angle) * (radius - 4),
                          cx + std::cos(angle) * radius, cy + std::sin(angle) * radius, ink);
        }
        char label[6];
        std::snprintf(label, sizeof(label), "%d%%", percent);
        drawText({static_cast<int16_t>(rect.x + 7), rect.y, static_cast<int16_t>(rect.w - 14), rect.h}, label, 2,
                 color(role), TextAlign::Center);
    }

    bool Context::rotary(Rect rect, int& value, int minimum, int maximum, int step, std::string_view label) {
        if (rect.w <= 0 || rect.h <= 0 || maximum <= minimum || step <= 0)
            return false;
        const int before = value;
        const Touch* event = touch();
        if (event && hasTouch(*event, TouchStart)) {
            rotaryDragging_ = contains(rect, event->x, event->y);
            rotaryStartX_ = event->x;
            rotaryStartValue_ = value;
        }
        if (rotaryDragging_ && event) {
            const int delta = (static_cast<int>(event->x) - rotaryStartX_) / 8;
            value = std::clamp(rotaryStartValue_ + delta * step, minimum, maximum);
            if (hasTouch(*event, TouchRelease))
                rotaryDragging_ = false;
        }
        auto state = combine(signature("rotary"), value);
        state = combine(state, minimum);
        state = combine(state, maximum);
        state = signature(label, state);
        if (redraw(rect, state)) {
            const int16_t r = std::min(rect.w, rect.h) / 2 - 3;
            const int16_t cx = rect.x + rect.w / 2, cy = rect.y + rect.h / 2;
            const int fill = (value - minimum) * 48 / (maximum - minimum);
            for (int i = 0; i <= 48; ++i) {
                const float a = (130 + i * 280.f / 48) * 0.01745329252f;
                gfx_.drawLine(cx + std::cos(a) * (r - 5), cy + std::sin(a) * (r - 5), cx + std::cos(a) * r,
                              cy + std::sin(a) * r, color(i <= fill ? themes::Accent : themes::ProgressTrack));
            }
            drawText({static_cast<int16_t>(rect.x + 8), static_cast<int16_t>(cy - 24),
                      static_cast<int16_t>(rect.w - 16), 27},
                     std::to_string(value), 3, color(themes::Foreground), TextAlign::Center);
            drawText({static_cast<int16_t>(rect.x + 8), static_cast<int16_t>(cy + 4), static_cast<int16_t>(rect.w - 16),
                      18},
                     label, 2, color(themes::Muted), TextAlign::Center);
        }
        return before != value;
    }

    PagedGrid Context::pagedGrid(Rect rect, size_t count, uint8_t columns, int16_t minimumHeight) {
        columns = std::max<uint8_t>(1, columns);
        constexpr int16_t gap = 4;
        int rows = std::max(1, (rect.h + gap) / (minimumHeight + gap));
        const bool paging = count > static_cast<size_t>(rows * columns);
        if (paging) {
            rect.h = std::max<int16_t>(1, rect.h - 36);
            rows = std::max(1, (rect.h + gap) / (minimumHeight + gap));
        } else {
            rows = std::min<size_t>(rows, std::max<size_t>(1, (count + columns - 1) / columns));
        }
        const size_t capacity = rows * columns;
        const size_t pages = std::max<size_t>(1, (count + capacity - 1) / capacity);
        gridPage_ = std::min(gridPage_, pages - 1);
        if (paging) {
            const int16_t y = rect.y + rect.h + 4;
            if (button({rect.x, y, 48, 32}, "<", gridPage_ > 0)) {
                --gridPage_;
                invalidate();
            }
            if (button({static_cast<int16_t>(rect.x + rect.w - 48), y, 48, 32}, ">", gridPage_ + 1 < pages)) {
                ++gridPage_;
                invalidate();
            }
            label({static_cast<int16_t>(rect.x + 52), y, static_cast<int16_t>(rect.w - 104), 32},
                  std::to_string(gridPage_ + 1) + "/" + std::to_string(pages), 2, themes::Muted, TextAlign::Center);
        }
        const size_t first = gridPage_ * capacity;
        return {rect,
                first,
                std::min(capacity, count - std::min(first, count)),
                columns,
                static_cast<int16_t>((rect.h - (rows - 1) * gap) / rows),
                gap};
    }
} // namespace ui
