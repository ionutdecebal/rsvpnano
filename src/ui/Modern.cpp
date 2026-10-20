#include "ui/Modern.h"

namespace ui::modern {
    using themes::rgb565;
    int ListSwipe::update(Context& ui, Rect area) {
        const auto* touch=ui.touch();if(!touch)return 0;
        if(hasTouch(*touch,TouchStart)) { tracking=contains(area,touch->x,touch->y);x=touch->x;y=touch->y; }
        if(!tracking || !hasTouch(*touch,TouchRelease)) return 0;
        tracking=false;
        const int dy=int(touch->y)-y,dx=int(touch->x)-x;
        return std::abs(dy)>=28 && std::abs(dy)>std::abs(dx) ? (dy<0?1:-1) : 0;
    }

    themes::ThemeColors inkColors() {
        themes::ThemeColors c;
        c.background=rgb565(21,20,19);c.foreground=rgb565(247,244,241);
        c.muted=rgb565(177,167,159);c.subtle=rgb565(128,113,102);
        c.accent=c.accentBar=c.guideFocus=rgb565(253,111,66);
        c.breakAccent=rgb565(255,180,138);c.onAccent=rgb565(33,19,14);
        c.surface=c.background;c.surfaceMuted=rgb565(39,34,30);
        c.surfaceActive=c.outline=c.progressTrack=rgb565(66,59,53);
        c.guide=c.phantom=c.muted;return c;
    }
    themes::Theme paperTheme() {
        themes::Theme theme{.id="paper", .definition={.name="Paper"}, .builtIn=true};
        auto& c=theme.definition.colors;
        c.background=rgb565(246,240,230);c.foreground=rgb565(48,39,31);
        c.muted=c.subtle=rgb565(114,96,80);
        c.accent=c.accentBar=c.guideFocus=rgb565(174,62,27);
        c.breakAccent=rgb565(135,82,42);c.onAccent=rgb565(255,249,243);
        c.surface=c.background;c.surfaceMuted=rgb565(234,224,210);
        c.surfaceActive=c.outline=c.progressTrack=rgb565(196,181,163);
        c.guide=c.phantom=c.muted;return theme;
    }

    bool button(Context& ui, Rect rect, std::string_view title, Style style, bool enabled,
                std::string_view detail, uint8_t size, TextAlign align) {
        auto state = Context::signature(title, Context::signature(detail));
        state = Context::combine(state, static_cast<uint8_t>(style));
        state = Context::combine(state, enabled);
        state = Context::combine(state, size);
        state = Context::combine(state, static_cast<uint8_t>(align));
        if (ui.redraw(rect, state)) {
            const bool primary = style == Style::Primary && enabled;
            const auto surface = ui.color(primary ? themes::AccentBar
                                          : style == Style::Secondary ? themes::SurfaceMuted : themes::Background);
            const auto ink = ui.color(!enabled ? themes::Muted : primary ? themes::OnAccent
                                     : style == Style::Accent ? themes::Accent : themes::Foreground);
            const int16_t groupH = std::min<int16_t>(46, rect.h);
            const int16_t titleH = std::min<int16_t>(25, groupH / 2 + 2);
            const int16_t groupY = static_cast<int16_t>(rect.y + (rect.h - groupH) / 2);
            const auto titleLayout = ui.prepareText(
                {static_cast<int16_t>(rect.x + 10), detail.empty() ? rect.y : groupY,
                 static_cast<int16_t>(rect.w - 20), static_cast<int16_t>(detail.empty() ? rect.h : titleH)},
                title, size, detail.empty() ? align : TextAlign::Start, detail.empty() ? 2 : 1);
            const auto detailLayout = ui.prepareText(
                {static_cast<int16_t>(rect.x + 10), static_cast<int16_t>(groupY + titleH),
                 static_cast<int16_t>(rect.w - 20), static_cast<int16_t>(groupH - titleH)},
                detail, 11);
            ui.paint(rect, [&](Arduino_GFX& gfx, Rect target) {
                gfx.fillRoundRect(target.x, target.y, target.w, target.h, 3, surface);
                ui.drawText(gfx, titleLayout, ink, target.x - rect.x, target.y - rect.y, surface);
                ui.drawText(gfx, detailLayout, ui.color(themes::Muted), target.x - rect.x, target.y - rect.y, surface);
            });
        }
        return ui.tap(rect, enabled);
    }

    bool arrow(Context& ui, Rect rect, Direction direction, bool enabled) {
        if (ui.redraw(rect, Context::combine(static_cast<uint8_t>(direction), enabled))) {
            ui.paint(rect, [&](Arduino_GFX& gfx, Rect target) {
                gfx.fillRoundRect(target.x, target.y, target.w, target.h, 3, ui.color(themes::SurfaceMuted));
                const int x = target.x + target.w / 2, y = target.y + target.h / 2;
                const auto ink = ui.color(enabled ? themes::Accent : themes::Subtle);
                // A 3-pixel stroke stays clear on the actual small panel.
                for (int stroke = -1; stroke <= 1; ++stroke) {
                    const int sign = direction == Direction::Left || direction == Direction::Up ? -1 : 1;
                    if (direction == Direction::Left || direction == Direction::Right) {
                        gfx.drawLine(x - sign * 4 + stroke, y - 9, x + sign * 5 + stroke, y, ink);
                        gfx.drawLine(x + sign * 5 + stroke, y, x - sign * 4 + stroke, y + 9, ink);
                    } else {
                        gfx.drawLine(x - 9, y - sign * 4 + stroke, x, y + sign * 5 + stroke, ink);
                        gfx.drawLine(x, y + sign * 5 + stroke, x + 9, y - sign * 4 + stroke, ink);
                    }
                }
            });
        }
        return ui.tap(rect, enabled);
    }

    void rule(Context& ui, Rect rect) {
        if (ui.redraw(rect, 1))
            ui.paint(rect, [&](Arduino_GFX& gfx, Rect target) {
                gfx.fillRect(target.x, target.y, target.w, target.h, ui.color(themes::Outline));
            });
    }

    void icon(Context& ui, Rect rect, Symbol symbol, themes::ColorRole role) {
        if (!ui.redraw(rect, Context::combine(static_cast<uint8_t>(symbol), static_cast<uint8_t>(role)))) return;
        ui.paint(rect, [&](Arduino_GFX& gfx, Rect target) {
            const int size = std::min(target.w, target.h);
            const int x = target.x + (target.w - size) / 2, y = target.y + (target.h - size) / 2;
            const auto ink = ui.color(role);
            const auto line = [&](int x1, int y1, int x2, int y2) {
                gfx.drawLine(x + x1 * size / 24, y + y1 * size / 24, x + x2 * size / 24, y + y2 * size / 24, ink);
            };
            switch (symbol) {
            case Symbol::Chapters:
                for (int row : {5, 12, 19}) { line(3, row, 4, row); line(8, row, 21, row); } break;
            case Symbol::Book:
                line(3,3,10,3); line(10,3,12,5); line(12,5,14,3); line(14,3,21,3);
                line(3,3,3,20); line(21,3,21,20); line(3,20,10,20); line(10,20,12,22);
                line(12,22,14,20); line(14,20,21,20); line(12,5,12,22); break;
            case Symbol::Settings:
                line(3,7,21,7); line(3,17,21,17); line(8,3,8,11); line(16,13,16,21); break;
            case Symbol::Pace:
                line(3,18,9,6); line(9,6,13,15); line(13,15,20,3); line(3,21,21,21); break;
            case Symbol::Type:
                line(3,20,9,4); line(9,4,15,20); line(5,14,13,14); line(17,10,22,10); line(19,10,19,20); break;
            case Symbol::Connect:
                line(7,4,3,8); line(3,8,3,12); line(3,12,7,16); line(7,16,12,11);
                line(12,13,17,8); line(17,8,21,12); line(21,12,21,16); line(21,16,17,20); line(8,16,16,8); break;
            case Symbol::Device:
                line(5,3,19,3); line(5,3,5,21); line(19,3,19,21); line(5,21,19,21); line(10,17,14,17); break;
            case Symbol::Sun:
                gfx.drawCircle(x + size / 2, y + size / 2, size / 5, ink);
                line(12,1,12,4); line(12,20,12,23); line(1,12,4,12); line(20,12,23,12);
                line(4,4,6,6); line(18,18,20,20); line(4,20,6,18); line(18,6,20,4); break;
            case Symbol::Launch: line(5,19,19,5); line(10,5,19,5); line(19,5,19,14); break;
            case Symbol::Chevron: line(9,5,16,12); line(16,12,9,19); break;
            case Symbol::Back: line(15,5,8,12); line(8,12,15,19); break;
            case Symbol::Play: line(7,4,19,12); line(19,12,7,20); line(7,20,7,4); break;
            case Symbol::Pause: line(8,4,8,20); line(16,4,16,20); break;
            }
        });
    }

    bool row(Context& ui, Rect rect, std::string_view title, std::string_view detail, Symbol symbol, bool enabled) {
        const int16_t left = rect.x + (symbol == Symbol::Chevron ? 10 : 42);
        if (symbol != Symbol::Chevron)
            icon(ui, {static_cast<int16_t>(rect.x + 10), static_cast<int16_t>(rect.y + (rect.h - 22) / 2), 22, 22},
                 symbol, enabled ? themes::Accent : themes::Muted);
        ui.label({left, rect.y, static_cast<int16_t>(rect.x + rect.w - 32 - left),
                  static_cast<int16_t>(detail.empty() ? rect.h - 1 : rect.h / 2 + 3)}, title,
                 detail.empty() ? 17 : 15, enabled ? themes::Foreground : themes::Muted);
        if (!detail.empty())
            ui.label({left, static_cast<int16_t>(rect.y + rect.h / 2 + 3),
                      static_cast<int16_t>(rect.x + rect.w - 30 - left), static_cast<int16_t>(rect.h / 2 - 4)},
                     detail, 11, themes::Muted);
        icon(ui, {static_cast<int16_t>(rect.x + rect.w - 23), static_cast<int16_t>(rect.y + (rect.h - 20) / 2), 20, 20}, Symbol::Chevron);
        rule(ui, {rect.x, static_cast<int16_t>(rect.y + rect.h - 1), rect.w, 1});
        return ui.tap(rect, enabled);
    }
}
