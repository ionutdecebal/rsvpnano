#include "ui/Layouts.h"

#include <cmath>
#include <cstdio>
#include "text/Utf8Text.h"

namespace ui {
    bool Context::card(Rect rect, std::string_view title, std::string_view detail, uint8_t textSize,
                       themes::ColorRole role, Icon icon, bool enabled, uint8_t alpha) {
        rect = paintBounds(rect);
        if (rect.w <= 0 || rect.h <= 0)
            return false;
        auto state = signature(title, signature(detail));
        state = combine(state, textSize);
        state = combine(state, role);
        state = combine(state, static_cast<uint8_t>(icon));
        state = combine(state, enabled);
        state = combine(state, alpha);
        if (claim(Kind::Card, rect, state).changed) {
            paint(rect, [&](Arduino_GFX& output, Rect rect) {
                const auto surface = blend(themes::SurfaceActive, alpha / 2);
                const auto ink = blend(enabled ? themes::Foreground : themes::Muted, alpha);
                output.fillRoundRect(rect.x, rect.y, rect.w, rect.h, 8, surface);
                output.drawRoundRect(rect.x, rect.y, rect.w, rect.h, 8, blend(role, alpha));
                const int16_t iconWidth = icon == Icon::None ? 0 : std::min<int16_t>(32, rect.w / 4);
                if (iconWidth)
                    drawIcon(output, {static_cast<int16_t>(rect.x + 6), rect.y, iconWidth, rect.h}, icon,
                             blend(role, alpha), surface);
                const int16_t x = rect.x + 6 + iconWidth;
                const int16_t width = std::max<int16_t>(0, rect.w - 12 - iconWidth);
                const int16_t detailHeight = detail.empty() ? 0 : std::min<int16_t>(36, rect.h / 3);
                fixedText(output,
                          {x, static_cast<int16_t>(rect.y + 3), width, static_cast<int16_t>(rect.h - detailHeight - 6)},
                          title, textSize, ink, TextAlign::Center, 8);
                if (!detail.empty())
                    fixedText(output,
                              {x, static_cast<int16_t>(rect.y + rect.h - detailHeight - 3), width, detailHeight},
                              detail, 2, blend(role, alpha), TextAlign::Center, 1);
            });
        }
        return tap(rect, enabled);
    }

    size_t Context::fixedText(Rect rect, std::string_view text, uint8_t textSize, uint16_t ink, TextAlign align,
                              uint8_t maxLines, bool ellipsis) {
        rect = paintBounds(rect);
        size_t consumed = 0;
        paint(rect, [&](Arduino_GFX& output, Rect translated) {
            consumed = fixedText(output, translated, text, textSize, ink, align, maxLines, ellipsis);
        });
        return consumed;
    }

    size_t Context::fixedText(Arduino_GFX& output, Rect rect, std::string_view text, uint8_t textSize, uint16_t ink,
                              TextAlign align, uint8_t maxLines, bool ellipsis) {
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
            drawText(output, {rect.x, y, rect.w, static_cast<int16_t>(height)}, line, textSize, ink, align);
            y += height;
        }
        return originalLength - text.size();
    }

    bool Context::dockItem(Rect rect, std::string_view label, Icon icon, uint16_t accent) {
        rect = paintBounds(rect);
        if (claim(Kind::Dock, rect, combine(signature(label), accent)).changed) {
            paint(rect, [&](Arduino_GFX& output, Rect rect) {
                const auto surface = color(label.empty() ? themes::Surface : themes::SurfaceActive);
                output.fillRoundRect(rect.x, rect.y, rect.w, rect.h, 7, surface);
                output.drawRoundRect(rect.x, rect.y, rect.w, rect.h, 7, accent);
                const int16_t iconWidth = std::min<int16_t>(30, rect.w - 8);
                const int16_t x = label.empty() ? rect.x + (rect.w - iconWidth) / 2 : rect.x + 6;
                drawIcon(output, {x, static_cast<int16_t>(rect.y + 4), iconWidth, static_cast<int16_t>(rect.h - 8)},
                         icon, accent, surface);
                if (!label.empty())
                    fixedText(output,
                              {static_cast<int16_t>(x + iconWidth + 3), rect.y,
                               static_cast<int16_t>(rect.w - iconWidth - 15), rect.h},
                              label, 2, color(themes::Foreground));
            });
        }
        return tap(rect);
    }

    void Context::progressRing(Rect rect, int value, int maximum, themes::ColorRole role) {
        rect = paintBounds(rect);
        if (rect.w < 8 || rect.h < 8)
            return;
        const int percent = maximum > 0 ? static_cast<int>(std::clamp<int64_t>(100LL * value / maximum, 0, 100)) : 0;
        if (!claim(Kind::Ring, rect, combine(combine(signature("ring"), percent), role)).changed)
            return;
        const int16_t radius = std::min(rect.w, rect.h) / 2 - 2;
        const int16_t cx = rect.x + rect.w / 2, cy = rect.y + rect.h / 2;
        constexpr int segments = 60;
        std::array<std::array<int16_t, 4>, segments> ticks{};
        for (int i = 0; i < segments; ++i) {
            const float angle = (i * 6 - 90) * 0.01745329252f;
            ticks[i] = {static_cast<int16_t>(cx + std::cos(angle) * (radius - 4)),
                        static_cast<int16_t>(cy + std::sin(angle) * (radius - 4)),
                        static_cast<int16_t>(cx + std::cos(angle) * radius),
                        static_cast<int16_t>(cy + std::sin(angle) * radius)};
        }
        char label[6];
        std::snprintf(label, sizeof(label), "%d%%", percent);
        const Rect bounds = rect;
        paint(rect, [&](Arduino_GFX& output, Rect rect) {
            const int16_t dx = rect.x - bounds.x, dy = rect.y - bounds.y;
            for (int i = 0; i < segments; ++i) {
                const auto& tick = ticks[i];
                const auto ink = color(i * 100 < percent * segments ? role : themes::ProgressTrack);
                output.drawLine(tick[0] + dx, tick[1] + dy, tick[2] + dx, tick[3] + dy, ink);
            }
            drawText(output, {static_cast<int16_t>(rect.x + 7), rect.y, static_cast<int16_t>(rect.w - 14), rect.h},
                     label, 2, color(role), TextAlign::Center);
        });
    }

    bool Context::rotary(Rect rect, int& value, int minimum, int maximum, int step, std::string_view label) {
        rect = paintBounds(rect);
        if (rect.w <= 0 || rect.h <= 0 || maximum <= minimum || step <= 0)
            return false;
        const int before = value;
        const Touch* event = touch();
        if (event && hasTouch(*event, TouchStart) && contains(rect, event->x, event->y)) {
            rotaryDragging_ = true;
            rotaryRect_ = rect;
            rotaryStartX_ = event->x;
            rotaryStartValue_ = value;
        }
        if (rotaryDragging_ && rotaryRect_ == rect && event) {
            const int delta = (static_cast<int>(event->x) - rotaryStartX_) / 8;
            value = std::clamp(rotaryStartValue_ + delta * step, minimum, maximum);
            if (hasTouch(*event, TouchRelease))
                rotaryDragging_ = false;
        }
        auto state = combine(signature("rotary"), value);
        state = combine(state, minimum);
        state = combine(state, maximum);
        state = signature(label, state);
        if (claim(Kind::Rotary, rect, state).changed) {
            const int16_t labelHeight = label.empty() ? 0 : textHeightFor(label, 2) + 2;
            const int16_t diameter = std::min<int16_t>(rect.w, rect.h - labelHeight);
            const int16_t r = std::max<int16_t>(6, diameter / 2 - 3);
            const int16_t cx = rect.x + rect.w / 2, cy = rect.y + (rect.h - labelHeight) / 2;
            const int fill = (value - minimum) * 48 / (maximum - minimum);
            std::array<std::array<int16_t, 4>, 49> ticks{};
            for (int i = 0; i <= 48; ++i) {
                const float a = (130 + i * 280.f / 48) * 0.01745329252f;
                ticks[i] = {static_cast<int16_t>(cx + std::cos(a) * (r - 5)),
                            static_cast<int16_t>(cy + std::sin(a) * (r - 5)),
                            static_cast<int16_t>(cx + std::cos(a) * r), static_cast<int16_t>(cy + std::sin(a) * r)};
            }
            const auto number = std::to_string(value);
            const uint8_t size = textWidth(number, 3) <= diameter - 16 && diameter >= 44 ? 3 : 2;
            const Rect bounds = rect;
            paint(rect, [&](Arduino_GFX& output, Rect rect) {
                const int16_t dx = rect.x - bounds.x, dy = rect.y - bounds.y;
                for (int i = 0; i <= 48; ++i) {
                    const auto& tick = ticks[i];
                    output.drawLine(tick[0] + dx, tick[1] + dy, tick[2] + dx, tick[3] + dy,
                                    color(i <= fill ? themes::Accent : themes::ProgressTrack));
                }
                drawText(output,
                         {static_cast<int16_t>(rect.x + 4), static_cast<int16_t>(cy + dy - textHeight(size) / 2),
                          static_cast<int16_t>(rect.w - 8), textHeight(size)},
                         number, size, color(themes::Foreground), TextAlign::Center);
                if (!label.empty())
                    fixedText(output,
                              {rect.x, static_cast<int16_t>(rect.y + rect.h - labelHeight), rect.w, labelHeight}, label,
                              2, color(themes::Muted), TextAlign::Center, 1);
            });
        }
        return before != value;
    }

    PagedGrid Context::pagedGrid(Rect rect, size_t count, uint8_t columns, int16_t minimumHeight) {
        rect = paintBounds(rect);
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
