#pragma once

#include "ui/Layouts.h"
#include "ui/screens/ScreenCommon.h"

namespace screens::watch {
    constexpr ui::Rect contentBounds(int16_t width, int16_t height) {
        const int16_t inset = std::clamp<int16_t>(std::min(width, height) / 15, 12, 28) & ~1;
        return {inset, inset, static_cast<int16_t>(width - 2 * inset), static_cast<int16_t>(height - 2 * inset)};
    }

    inline uint8_t textSize(const ui::Context& ui) {
        return std::min(ui.width(), ui.height()) < 240 ? 2 : 3;
    }

    inline ui::Rect header(ui::Context& ui, std::string_view title, Screen back, Screen& screen) {
        auto area = detail::content(ui);
        if (ui.button({area.x, area.y, 48, 36}, "<"))
            screen = back;
        ui.label({static_cast<int16_t>(area.x + 54), area.y, static_cast<int16_t>(area.w - 54), 36}, title,
                 textSize(ui), ui::themes::Foreground, ui::TextAlign::Center);
        area.y += 40;
        area.h -= 40;
        return area;
    }

    inline bool setting(ui::Context& ui, ui::Rect rect, UiText label, std::string_view value) {
        return ui.card(rect, ui.text(label), value, textSize(ui));
    }

    inline bool toggle(ui::Context& ui, ui::Rect rect, UiText label, bool& value) {
        if (!setting(ui, rect, label, ui.text(value ? UiText::On : UiText::Off)))
            return false;
        value = !value;
        return true;
    }

    template<typename T>
    bool stepper(ui::Context& ui, ui::Rect rect, UiText label, T& value, std::string_view suffix = {}) {
        if (rect.w <= 0 || rect.h <= 0)
            return false;
        return ui.stepper(rect, ui.text(label), value, suffix);
    }

    inline std::array<ui::Rect, 3> carousel(ui::Rect area) {
        const int16_t side = (area.w - 12) * 3 / 10;
        const int16_t inset = std::min<int16_t>(24, area.h / 10);
        return {
            {{area.x, static_cast<int16_t>(area.y + inset), side, static_cast<int16_t>(area.h - inset * 2)},
             {static_cast<int16_t>(area.x + side + 6), area.y, static_cast<int16_t>(area.w - side * 2 - 12), area.h},
             {static_cast<int16_t>(area.x + area.w - side), static_cast<int16_t>(area.y + inset), side,
              static_cast<int16_t>(area.h - inset * 2)}}};
    }
} // namespace screens::watch
