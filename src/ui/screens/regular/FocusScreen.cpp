#include "logging/Logger.h"
#include "ui/screens/ScreenCommon.h"

#include <algorithm>
#include <cstdio>
#include <utility>

#include "storage/fs/StorageFiles.h"
#include "storage/fs/StoragePaths.h"
#include "focus/FocusTimerStorage.h"

namespace screens {
    Action FocusScreen::drawTimers(ui::Context& ui, Screen& screen) {
        if (const Action action = detail::navigation(ui, Screen::FocusTimers, screen); action != Action::None)
            return action;
        const ui::Rect content = detail::tabContent(ui);
        constexpr int16_t columns = 3;
        constexpr int16_t rows = 2;
        constexpr int16_t rowGap = 8;
        const int16_t cellWidth = static_cast<int16_t>(content.w / columns);
        const int16_t cellHeight = static_cast<int16_t>((content.h - rowGap) / rows);
        const size_t visibleCount = timers_.timers.size() + (timers_.timers.size() < focus::kMaxTimers ? 1 : 0);

        uint32_t state = static_cast<uint32_t>(timers_.timers.size());
        state = ui::Context::combine(state, writable_);
        state = ui::Context::combine(state, orientation_.available());
        for (const focus::Timer& timer: timers_.timers) {
            state = ui::Context::signature(timer.name, state);
            state = ui::Context::combine(state, timer.focusMinutes);
            state = ui::Context::combine(state, timer.breakMinutes);
            state = ui::Context::combine(state, timer.rounds);
        }
        const bool redraw = ui.redraw(content, state);

        for (size_t index = 0; index < visibleCount; ++index) {
            const ui::Rect cell{static_cast<int16_t>(content.x + (index % columns) * cellWidth),
                                static_cast<int16_t>(content.y + (index / columns) * (cellHeight + rowGap)), cellWidth,
                                cellHeight};

            if (index == timers_.timers.size()) {
                if (redraw) {
                    const uint16_t ink =
                        ui.color(writable_ ? ui::themes::ColorRole::Accent : ui::themes::ColorRole::Muted);
                    const int16_t centerX = static_cast<int16_t>(cell.x + cell.w / 2);
                    const int16_t centerY = static_cast<int16_t>(cell.y + cell.h / 2);
                    ui.gfx().drawCircle(centerX, centerY, 21, ink);
                    ui.gfx().fillRect(static_cast<int16_t>(centerX - 1), static_cast<int16_t>(centerY - 8), 3, 17, ink);
                    ui.gfx().fillRect(static_cast<int16_t>(centerX - 8), static_cast<int16_t>(centerY - 1), 17, 3, ink);
                }
                if (ui.tap(cell, writable_)) {
                    edit(timers_.timers.size(), true, screen);
                }
                continue;
            }

            const focus::Timer& timer = timers_.timers[index];
            if (redraw) {
                const uint16_t ink = ui.color(orientation_.available() ? ui::themes::ColorRole::Foreground
                                                                       : ui::themes::ColorRole::Muted);
                const uint16_t outline = ui.color(ui::themes::ColorRole::Outline);
                constexpr int16_t glassWidth = 52;
                constexpr int16_t roundsWidth = 36;
                constexpr int16_t groupGap = 4;
                constexpr int16_t visualHeight = 70;
                constexpr int16_t chamberHeight = 18;
                constexpr int16_t taperHeight = 8;
                constexpr int16_t baseOverhang = 5;
                constexpr int16_t baseHeight = 2;
                constexpr int16_t textLift = 2;
                const int16_t visualTop = static_cast<int16_t>(cell.y + (cell.h - visualHeight) / 2);
                const int16_t centerX = static_cast<int16_t>(cell.x + cell.w / 2);
                const int16_t left = static_cast<int16_t>(centerX - glassWidth / 2);
                const int16_t right = static_cast<int16_t>(left + glassWidth - 1);
                const int16_t top = static_cast<int16_t>(visualTop + 18);
                const int16_t topRectBottom = static_cast<int16_t>(top + chamberHeight);
                const int16_t waist = static_cast<int16_t>(topRectBottom + taperHeight);
                const int16_t bottomRectTop = static_cast<int16_t>(waist + taperHeight);
                const uint16_t focusSand = ui.blend(ui::themes::ColorRole::Accent, 64);
                const uint16_t breakSand = ui.blend(ui::themes::ColorRole::BreakAccent, 64);
                const uint16_t base = ui.color(ui::themes::ColorRole::SurfaceActive);
                const int16_t titleWidth = static_cast<int16_t>(cell.w - 4);
                const uint8_t titleSize = timer.name.size() * 12 <= static_cast<size_t>(titleWidth) ? 2 : 1;
                ui.drawText({static_cast<int16_t>(cell.x + 2), static_cast<int16_t>(visualTop - textLift), titleWidth,
                             16},
                            timer.name, titleSize, ink, ui::TextAlign::Center);
                ui.gfx().fillRect(left, top, glassWidth, chamberHeight, focusSand);
                ui.gfx().fillTriangle(left, topRectBottom, right, topRectBottom, centerX, waist, focusSand);
                ui.gfx().fillTriangle(centerX, waist, left, bottomRectTop, right, bottomRectTop, breakSand);
                ui.gfx().fillRect(left, bottomRectTop, glassWidth, chamberHeight, breakSand);
                ui.gfx().fillRect(static_cast<int16_t>(left - baseOverhang), static_cast<int16_t>(top - baseHeight),
                                  static_cast<int16_t>(glassWidth + baseOverhang * 2), baseHeight, base);
                ui.gfx().fillRect(static_cast<int16_t>(left - baseOverhang),
                                  static_cast<int16_t>(bottomRectTop + chamberHeight),
                                  static_cast<int16_t>(glassWidth + baseOverhang * 2), baseHeight, base);
                ui.gfx().drawFastHLine(left, top, glassWidth, outline);
                ui.gfx().drawFastVLine(left, top, chamberHeight, outline);
                ui.gfx().drawFastVLine(right, top, chamberHeight, outline);
                ui.gfx().drawLine(left, topRectBottom, centerX, waist, outline);
                ui.gfx().drawLine(right, topRectBottom, centerX, waist, outline);
                ui.gfx().drawLine(centerX, waist, left, bottomRectTop, outline);
                ui.gfx().drawLine(centerX, waist, right, bottomRectTop, outline);
                ui.gfx().drawFastVLine(left, bottomRectTop, chamberHeight, outline);
                ui.gfx().drawFastVLine(right, bottomRectTop, chamberHeight, outline);
                ui.gfx().drawFastHLine(left, static_cast<int16_t>(bottomRectTop + chamberHeight - 1), glassWidth,
                                       outline);
                char focusTime[8];
                char breakTime[8];
                char rounds[8];
                std::snprintf(focusTime, sizeof(focusTime), "%um", static_cast<unsigned int>(timer.focusMinutes));
                std::snprintf(breakTime, sizeof(breakTime), "%um", static_cast<unsigned int>(timer.breakMinutes));
                std::snprintf(rounds, sizeof(rounds), "x%u", static_cast<unsigned int>(timer.rounds));
                ui.drawText({left, static_cast<int16_t>(top + 2), glassWidth, chamberHeight}, focusTime, 2, ink,
                            ui::TextAlign::Center);
                ui.drawText({left, static_cast<int16_t>(bottomRectTop - textLift), glassWidth, chamberHeight},
                            breakTime, 2, ink, ui::TextAlign::Center);
                ui.drawText({static_cast<int16_t>(right + groupGap), static_cast<int16_t>(bottomRectTop - textLift),
                             roundsWidth, chamberHeight},
                            rounds, 2, ink, ui::TextAlign::Center);
            }

            const bool tapped = ui.tap(cell, orientation_.available());
            const ui::Touch* touch = ui.touch();
            const bool held = writable_ && touch != nullptr && ui::hasTouch(*touch, ui::TouchHold)
                           && ui::contains(cell, touch->x, touch->y);
            if (held) {
                edit(index, false, screen);
            } else if (tapped) {
                activeIndex_ = index;
                session_.begin(timer);
                screen = Screen::FocusSession;
            }
        }

        if (redraw)
            ui.markDrawn();
        return Action::None;
    }

    void FocusScreen::drawEditor(ui::Context& ui, Screen& screen) {
        const ui::Rect content = detail::content(ui);
        constexpr int16_t gap = 6;
        constexpr int16_t headerHeight = 40;
        constexpr int16_t saveWidth = 96;
        const bool wide = content.w >= 480;
        const bool confirming = deleteConfirm_;
        ui::Rect backAction;
        ui::Rect name;
        ui::Rect saveAction;
        ui::Rect focusControl;
        ui::Rect breakControl;
        ui::Rect roundsControl;
        ui::Rect deletePrompt;
        ui::Rect deleteAction;
        if (wide) {
            constexpr int16_t rowHeight = 48;
            constexpr int16_t backWidth = 64;
            constexpr int16_t deleteWidth = 86;
            ui::Row row{{content.x, content.y, content.w, rowHeight}, gap};
            backAction = row.next(backWidth);
            if (confirming) {
                deletePrompt = row.next(static_cast<int16_t>(content.w - backWidth - deleteWidth - gap * 2));
                deleteAction = row.next(deleteWidth);
            } else {
                const int16_t actionWidths =
                    static_cast<int16_t>(backWidth + saveWidth + (creating_ ? 0 : deleteWidth));
                const int16_t actionGaps = static_cast<int16_t>(gap * (creating_ ? 2 : 3));
                name = row.next(static_cast<int16_t>(content.w - actionWidths - actionGaps));
                if (!creating_)
                    deleteAction = row.next(deleteWidth);
                saveAction = row.next(saveWidth);
            }

            const int16_t controlsY = static_cast<int16_t>(content.y + rowHeight + gap);
            const int16_t controlWidth = static_cast<int16_t>((content.w - gap * 2) / 3);
            const int16_t controlHeight = static_cast<int16_t>(content.y + content.h - controlsY);
            focusControl = {content.x, controlsY, controlWidth, controlHeight};
            breakControl = {static_cast<int16_t>(content.x + controlWidth + gap), controlsY, controlWidth,
                            controlHeight};
            roundsControl = {static_cast<int16_t>(content.x + (controlWidth + gap) * 2), controlsY, controlWidth,
                             controlHeight};
        } else {
            constexpr int16_t controlGap = 4;
            const int16_t saveX = static_cast<int16_t>(content.x + content.w - saveWidth);
            backAction = {content.x, content.y, 64, headerHeight};
            saveAction = {saveX, content.y, saveWidth, headerHeight};
            ui.label({static_cast<int16_t>(content.x + 74), content.y, static_cast<int16_t>(saveX - content.x - 80),
                      headerHeight},
                     ui.text(UiText::FocusTimer), 2);
            const int16_t bodyY = static_cast<int16_t>(content.y + headerHeight + gap);
            const int16_t controlsY = static_cast<int16_t>(bodyY + 40);
            const int16_t deleteY = static_cast<int16_t>(content.y + content.h - 40);
            const int16_t controlHeight =
                std::min<int16_t>(50, std::max<int16_t>(30, (deleteY - controlsY - controlGap * 3) / 3));
            name = {content.x, bodyY, content.w, 34};
            focusControl = {content.x, controlsY, content.w, controlHeight};
            breakControl = {content.x, static_cast<int16_t>(controlsY + controlHeight + controlGap), content.w,
                            controlHeight};
            roundsControl = {content.x, static_cast<int16_t>(controlsY + (controlHeight + controlGap) * 2), content.w,
                             controlHeight};
            if (confirming) {
                const int16_t promptWidth = static_cast<int16_t>((content.w - gap) / 2);
                deletePrompt = {content.x, deleteY, promptWidth, 40};
                deleteAction = {static_cast<int16_t>(content.x + promptWidth + gap), deleteY,
                                static_cast<int16_t>(content.w - promptWidth - gap), 40};
            } else {
                deleteAction = {content.x, deleteY, content.w, 40};
            }
        }

        if (ui.button(backAction, "<<")) {
            if (deleteConfirm_)
                deleteConfirm_ = false;
            else
                screen = Screen::FocusTimers;
        }

        if (confirming) {
            ui.label(deletePrompt, ui.text(UiText::AreYouSure), wide ? 2 : 1, ui::themes::ColorRole::Muted,
                     ui::TextAlign::Center);
        } else if (ui.setting(name, ui.text(UiText::TimerName), draft_.name)) {
            keyboard_ = {};
            screen = Screen::FocusNameEdit;
        }

        bool save = false;
        if (confirming && !wide)
            ui.tap(saveAction, false);
        else if (!confirming)
            save = ui.button(saveAction, ui.text(creating_ ? UiText::Add : UiText::Save),
                             writable_ && focus::valid(draft_));

        ui.stepper(focusControl, ui.text(UiText::FocusMinutes), draft_.focusMinutes, " min",
                   ui::themes::ColorRole::Accent);
        ui.stepper(breakControl, ui.text(UiText::BreakMinutes), draft_.breakMinutes, " min",
                   ui::themes::ColorRole::BreakAccent);
        ui.stepper(roundsControl, ui.text(UiText::Rounds), draft_.rounds);

        if (!creating_ && ui.button(deleteAction, ui.text(UiText::Delete), writable_ && timers_.timers.size() > 1)) {
            if (!deleteConfirm_) {
                deleteConfirm_ = true;
            } else {
                focus::Timers updated = timers_;
                updated.timers.erase(updated.timers.begin() + editIndex_);
                if (persist(updated)) {
                    timers_ = std::move(updated);
                    deleteConfirm_ = false;
                    screen = Screen::FocusTimers;
                }
            }
        }

        if (save) {
            focus::Timers updated = timers_;
            if (creating_)
                updated.timers.push_back(draft_);
            else
                updated.timers[editIndex_] = draft_;
            if (persist(updated)) {
                timers_ = std::move(updated);
                screen = Screen::FocusTimers;
            }
        }
    }

    bool FocusScreen::drawSession(ui::Context& ui, uint32_t nowMs) {
        const ui::Rect area = detail::content(ui);
        const focus::Timer& timer = timers_.timers[activeIndex_];
        const focus::Phase phase = session_.phase();
        const bool paused = phase == focus::Phase::PausedFocus || phase == focus::Phase::PausedBreak;
        const bool reversed = phase == focus::Phase::Break || phase == focus::Phase::PausedBreak;
        const bool focusPhase =
            phase == focus::Phase::WaitingFocus || phase == focus::Phase::Focus || phase == focus::Phase::PausedFocus;
        const bool complete = phase == focus::Phase::Complete;
        uint32_t remaining = session_.remainingMs(nowMs);
        if (phase == focus::Phase::WaitingFocus)
            remaining = static_cast<uint32_t>(timer.focusMinutes) * 60UL * 1000UL;
        else if (phase == focus::Phase::WaitingBreak)
            remaining = static_cast<uint32_t>(timer.breakMinutes) * 60UL * 1000UL;
        const uint32_t seconds = (remaining + 999UL) / 1000UL;
        char time[8];
        std::snprintf(time, sizeof(time), "%02lu:%02lu", static_cast<unsigned long>(seconds / 60UL),
                      static_cast<unsigned long>(seconds % 60UL));
        const ui::themes::ColorRole phaseRole =
            focusPhase || complete ? ui::themes::ColorRole::Accent : ui::themes::ColorRole::BreakAccent;
        constexpr int16_t railWidth = 40;
        constexpr int16_t gap = 4;
        const ui::Rect hourglass{static_cast<int16_t>(area.x + railWidth + gap), area.y,
                                 static_cast<int16_t>(area.w - (railWidth + gap) * 2), area.h};
        ui.steps({area.x, area.y, railWidth, area.h}, session_.round(), session_.rounds(), phaseRole);
        ui.hourglass(hourglass, session_.progressPermille(nowMs), paused, complete, phaseRole, reversed, time);
        return ui.button({static_cast<int16_t>(area.x + area.w - railWidth), area.y, railWidth, area.h}, ">>");
    }

} // namespace screens
