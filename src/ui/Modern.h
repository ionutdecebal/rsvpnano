#pragma once

#include "ui/Ui.h"

namespace ui::modern {
    enum class Style : uint8_t { Plain, Secondary, Primary, Accent };
    enum class Direction : uint8_t { Left, Right, Up, Down };
    enum class Symbol : uint8_t { Chapters, Book, Settings, Pace, Type, Connect, Device, Sun, Launch, Chevron, Back, Play, Pause };

    struct ListSwipe {
        int16_t x = 0, y = 0;
        bool tracking = false;
        int update(Context& ui, Rect area);
    };

    // Native, fixed-size controls for the 640 x 172 landscape presentation.
    bool button(Context& ui, Rect rect, std::string_view title, Style style = Style::Secondary,
                bool enabled = true, std::string_view detail = {}, uint8_t size = 14,
                TextAlign align = TextAlign::Center);
    void icon(Context& ui, Rect rect, Symbol symbol, themes::ColorRole role = themes::Muted);
    bool row(Context& ui, Rect rect, std::string_view title, std::string_view detail = {},
             Symbol symbol = Symbol::Chevron, bool enabled = true);
    bool arrow(Context& ui, Rect rect, Direction direction, bool enabled = true);
    void rule(Context& ui, Rect rect);
    themes::ThemeColors inkColors();
    themes::Theme paperTheme();
    void logo(Context& ui, int16_t x, int16_t y, uint32_t nowMs);
}
