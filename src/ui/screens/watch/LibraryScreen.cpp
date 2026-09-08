#include "ui/screens/LibraryScreen.h"
#include "ui/screens/watch/Layout.h"

namespace screens {
    Action LibraryScreen::draw(ui::Context& ui, const std::vector<LibraryItem>& items, uint32_t nowMs, Screen& screen) {
        (void) nowMs;
        detail::navigation(ui, Screen::Library, screen);
        const auto area = detail::tabContent(ui);
        if (items.empty()) {
            ui.label(area, ui.text(UiText::Library), watch::textSize(ui), ui::themes::Muted, ui::TextAlign::Center);
            return Action::None;
        }
        selectedIndex_ = std::min(selectedIndex_, items.size() - 1);
        const int delta = carouselGesture_.update(ui.touch(), area);
        if (delta) {
            selectedIndex_ = ui::rotateIndex(selectedIndex_, items.size(), delta);
            ui.invalidate();
        }
        const int16_t peek = ui.height() < 240 ? 28 : 48;
        const auto title = [&](size_t index) {
            return items[index].book ? BookLibrary::displayName(*items[index].book) : std::string_view{};
        };
        if (ui.card({area.x, area.y, area.w, peek}, title(ui::rotateIndex(selectedIndex_, items.size(), -1)), {}, 2,
                    ui::themes::Accent, ui::Icon::None, items.size() > 1 && !delta, 180)) {
            selectedIndex_ = ui::rotateIndex(selectedIndex_, items.size(), -1);
            ui.invalidate();
        }
        const ui::Rect center{area.x, static_cast<int16_t>(area.y + peek + 4), area.w,
                              static_cast<int16_t>(area.h - peek * 2 - 8)};
        const auto& item = items[selectedIndex_];
        const int16_t ring = std::min<int16_t>(80, center.h);
        ui.progressRing({static_cast<int16_t>(center.x + center.w - ring),
                         static_cast<int16_t>(center.y + (center.h - ring) / 2), ring, ring},
                        item.progress);
        auto book = center;
        book.w -= ring + 6;
        if (ui.card(book, title(selectedIndex_), item.chapter, watch::textSize(ui), ui::themes::Accent, ui::Icon::None,
                    !delta && item.book))
            return Action::OpenBook;
        if (ui.card({area.x, static_cast<int16_t>(area.y + area.h - peek), area.w, peek},
                    title(ui::rotateIndex(selectedIndex_, items.size(), 1)), {}, 2, ui::themes::Accent, ui::Icon::None,
                    items.size() > 1 && !delta, 180)) {
            selectedIndex_ = ui::rotateIndex(selectedIndex_, items.size(), 1);
            ui.invalidate();
        }
        return Action::None;
    }
} // namespace screens
