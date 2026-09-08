#include "logging/Logger.h"
#include "ui/screens/ScreenCommon.h"

#include <algorithm>
#include <cstdio>
#include <utility>

#include "storage/fs/StorageFiles.h"
#include "storage/fs/StoragePaths.h"
#include "focus/FocusTimerStorage.h"

namespace screens {
    void FocusScreen::begin() {
        filesystem_ = nullptr;
        timers_ = focus::defaultTimers();
        writable_ = false;
        orientation_.begin();
    }

    void FocusScreen::begin(fs::FS& filesystem) {
        filesystem_ = &filesystem;
        timers_ = focus::defaultTimers();
        writable_ = true;
        {
            auto loaded = focus::load(*filesystem_);
            if (loaded) {
                timers_ = std::move(*loaded);
            } else if (loaded.error() == std::errc::no_such_file_or_directory) {
                auto saved = focus::save(*filesystem_, timers_);
                writable_ = saved.has_value();
                if (!saved)
                    Logger::failure("focus", "save defaults", StoragePaths::kFocusConfigPath, saved.error());
            } else {
                Logger::failure("focus", "load; using defaults", StoragePaths::kFocusConfigPath, loaded.error());
            }
        }
        orientation_.begin();
    }

    bool FocusScreen::update(uint32_t nowMs) {
        const focus::Orientation orientation = orientation_.update(nowMs);
        session_.update(nowMs, orientation);
        return session_.consumeCompletionCue();
    }

    Action FocusScreen::draw(ui::Context& ui, uint32_t nowMs, Screen& screen) {
        switch (screen) {
        case Screen::FocusTimers:
            return drawTimers(ui, screen);
        case Screen::FocusEditor:
            drawEditor(ui, screen);
            break;
        case Screen::FocusNameEdit:
            drawNameEditor(ui, screen);
            break;
        case Screen::FocusSession:
            if (drawSession(ui, nowMs))
                screen = Screen::FocusTimers;
            break;
        default:
            break;
        }
        return Action::None;
    }

    void FocusScreen::setTimers(focus::Timers timers) {
        timers_ = std::move(timers);
        editIndex_ = 0;
        activeIndex_ = 0;
        selectedIndex_ = 0;
        carouselGesture_ = {};
        creating_ = false;
        deleteConfirm_ = false;
    }

    void FocusScreen::close() {
        session_.stop();
    }

    void FocusScreen::drawNameEditor(ui::Context& ui, Screen& screen) {
        const ui::Rect content = detail::content(ui);
        const ui::KeyboardAction action =
            ui.keyboard(content, draft_.name, focus::kMaxTimerNameBytes, keyboard_, ui.text(UiText::TimerName));
        if (action == ui::KeyboardAction::Cancel || action == ui::KeyboardAction::Submit)
            screen = Screen::FocusEditor;
    }

    void FocusScreen::edit(size_t index, bool creating, Screen& screen) {
        editIndex_ = index;
        creating_ = creating;
        draft_ = creating ? focus::defaultTimer() : timers_.timers[index];
        if (creating)
            draft_.name.clear();
        deleteConfirm_ = false;
        keyboard_ = {};
        screen = Screen::FocusEditor;
    }

    bool FocusScreen::persist(const focus::Timers& timers) {
        if (!writable_ || !filesystem_)
            return false;
        auto saved = focus::save(*filesystem_, timers);
        if (!saved)
            Logger::failure("focus", "save", StoragePaths::kFocusConfigPath, saved.error());
        return saved.has_value();
    }

} // namespace screens
