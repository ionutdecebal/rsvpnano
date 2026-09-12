#include "ui/screens/watch/Layout.h"

#include <cstdio>

namespace screens {
    Action FocusScreen::drawTimers(ui::Context& ui, Screen& screen) {
        detail::navigation(ui, Screen::FocusTimers, screen);
        const auto area = detail::tabContent(ui);
        const size_t count = timers_.timers.size() + (timers_.timers.size() < focus::kMaxTimers ? 1 : 0);
        if (!count)
            return Action::None;
        selectedIndex_ = std::min(selectedIndex_, count - 1);
        const int delta = carouselGesture_.update(ui.touch(), area);
        if (delta)
            selectedIndex_ = ui::rotateIndex(selectedIndex_, count, delta);
        const auto cards = watch::carousel(area);
        const auto name = [&](size_t index) -> std::string_view {
            return index < timers_.timers.size() ? std::string_view{timers_.timers[index].name} : ui.text(UiText::Add);
        };
        const size_t previous = ui::rotateIndex(selectedIndex_, count, -1),
                     next = ui::rotateIndex(selectedIndex_, count, 1);
        if (ui.card(cards[0], name(previous), "<", 2, ui::themes::BreakAccent, ui::Icon::None, count > 1 && !delta,
                    170))
            selectedIndex_ = previous;
        const bool adding = selectedIndex_ == timers_.timers.size();
        const int16_t editHeight = std::min<int16_t>(44, cards[1].h / 3);
        auto center = cards[1];
        center.h -= editHeight + 4;
        std::string detail;
        if (!adding) {
            const auto& timer = timers_.timers[selectedIndex_];
            detail = std::to_string(timer.focusMinutes) + "/" + std::to_string(timer.breakMinutes) + "m";
        }
        if (ui.card(center, name(selectedIndex_), detail, watch::textSize(ui), ui::themes::BreakAccent, ui::Icon::None,
                    !delta && (adding ? writable_ : orientation_.available()))) {
            if (adding)
                edit(timers_.timers.size(), true, screen);
            else {
                activeIndex_ = selectedIndex_;
                session_.begin(timers_.timers[activeIndex_]);
                screen = Screen::FocusSession;
            }
        }
        const ui::Rect editRect{cards[1].x, static_cast<int16_t>(cards[1].y + cards[1].h - editHeight), cards[1].w,
                                editHeight};
        if (ui.card(editRect, ui.text(adding ? UiText::Add : UiText::Settings), {}, 2, ui::themes::BreakAccent,
                    ui::Icon::None, writable_ && !delta))
            edit(selectedIndex_, adding, screen);
        if (ui.card(cards[2], name(next), ">", 2, ui::themes::BreakAccent, ui::Icon::None, count > 1 && !delta, 170))
            selectedIndex_ = next;
        return Action::None;
    }

    void FocusScreen::drawEditor(ui::Context& ui, Screen& screen) {
        auto area = watch::header(ui, ui.text(UiText::FocusTimer), Screen::FocusTimers, screen);
        auto grid = ui.pagedGrid(area, creating_ ? 5 : 6, 1, 56);
        if (watch::setting(ui, grid.item(0), UiText::TimerName, draft_.name)) {
            keyboard_ = {};
            screen = Screen::FocusNameEdit;
        }
        watch::stepper(ui, grid.item(1), UiText::FocusMinutes, draft_.focusMinutes, " min");
        watch::stepper(ui, grid.item(2), UiText::BreakMinutes, draft_.breakMinutes, " min");
        watch::stepper(ui, grid.item(3), UiText::Rounds, draft_.rounds);
        if (ui.card(grid.item(4), ui.text(creating_ ? UiText::Add : UiText::Save), {}, watch::textSize(ui),
                    ui::themes::BreakAccent, ui::Icon::None, writable_ && focus::valid(draft_))) {
            auto updated = timers_;
            if (creating_)
                updated.timers.push_back(draft_);
            else
                updated.timers[editIndex_] = draft_;
            if (persist(updated)) {
                timers_ = std::move(updated);
                selectedIndex_ = editIndex_;
                screen = Screen::FocusTimers;
            }
        }
        if (!creating_
            && ui.card(grid.item(5), ui.text(UiText::Delete),
                       deleteConfirm_ ? ui.text(UiText::AreYouSure) : std::string_view{}, watch::textSize(ui),
                       ui::themes::Accent, ui::Icon::None, writable_ && timers_.timers.size() > 1)) {
            if (!deleteConfirm_) {
                deleteConfirm_ = true;
                ui.invalidate();
            } else {
                auto updated = timers_;
                updated.timers.erase(updated.timers.begin() + editIndex_);
                if (persist(updated)) {
                    timers_ = std::move(updated);
                    deleteConfirm_ = false;
                    selectedIndex_ = 0;
                    screen = Screen::FocusTimers;
                }
            }
        }
    }

    bool FocusScreen::drawSession(ui::Context& ui, uint32_t nowMs) {
        const auto area = detail::content(ui);
        const auto& timer = timers_.timers[activeIndex_];
        const auto phase = session_.phase();
        const bool paused = phase == focus::Phase::PausedFocus || phase == focus::Phase::PausedBreak;
        const bool reversed = phase == focus::Phase::Break || phase == focus::Phase::PausedBreak;
        const bool complete = phase == focus::Phase::Complete;
        uint32_t remaining = session_.remainingMs(nowMs);
        if (phase == focus::Phase::WaitingFocus)
            remaining = static_cast<uint32_t>(timer.focusMinutes) * 60000;
        if (phase == focus::Phase::WaitingBreak)
            remaining = static_cast<uint32_t>(timer.breakMinutes) * 60000;
        const uint32_t seconds = (remaining + 999) / 1000;
        char time[12];
        std::snprintf(time, sizeof(time), "%02lu:%02lu", static_cast<unsigned long>(seconds / 60),
                      static_cast<unsigned long>(seconds % 60));
        const auto role = reversed ? ui::themes::BreakAccent : ui::themes::Accent;
        ui.label({area.x, area.y, area.w, 32}, timer.name, watch::textSize(ui), role, ui::TextAlign::Center);
        const int16_t controls = 36;
        const ui::Rect body{area.x, static_cast<int16_t>(area.y + 36), area.w, static_cast<int16_t>(area.h - 76)};
        const int16_t ring = std::min<int16_t>(body.h, body.w / 3);
        ui.progressRing({body.x, body.y, ring, ring}, session_.progressPermille(nowMs), 1000, role);
        ui.hourglass({static_cast<int16_t>(body.x + ring + 6), body.y, static_cast<int16_t>(body.w - ring - 6), body.h},
                     session_.progressPermille(nowMs), paused, complete, role, reversed, time);
        const int16_t y = area.y + area.h - controls;
        ui.label({area.x, y, static_cast<int16_t>(area.w / 2), controls},
                 std::to_string(session_.round()) + "/" + std::to_string(session_.rounds()), 2, role,
                 ui::TextAlign::Center);
        return ui.card({static_cast<int16_t>(area.x + area.w / 2 + 4), y, static_cast<int16_t>(area.w / 2 - 4),
                        controls},
                       ">>", {}, 2, role);
    }
} // namespace screens
