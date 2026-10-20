#pragma once

#include "ui/Modern.h"
#include "ui/screens/Screens.h"

namespace screens::modern {
    constexpr bool enabled(int16_t width, int16_t height, Screen screen) {
        if (width != 640 || height != 172) return false;
        switch (screen) {
        case Screen::Read: case Screen::Library: case Screen::Chapters: case Screen::Settings:
        case Screen::ReadingSettings: case Screen::PacingSettings: case Screen::InterfaceSettings:
        case Screen::ReaderAppearance: case Screen::BookFonts: case Screen::NetworkSettings:
        case Screen::Device: case Screen::Ota: case Screen::FocusTimers:
        case Screen::FocusEditor: case Screen::FocusNameEdit: case Screen::FocusSession:
        case Screen::WifiScan: case Screen::WifiConnect: case Screen::NetworkEdit:
        case Screen::ReadingHub: case Screen::DisplayHub: case Screen::Connections: case Screen::Help:
        case Screen::StorageHub: case Screen::StorageEncryption: case Screen::PowerHub: case Screen::FocusPresets:
        case Screen::FindPlace: case Screen::FindChapters: case Screen::BookSearch: case Screen::BookSearchKeyboard:
        case Screen::Dictionary: case Screen::DictionarySearch: case Screen::DictionaryHelp: case Screen::DictionaryInstall:
        case Screen::QuickControls: case Screen::Onboarding: case Screen::ButtonMap: case Screen::AddBooks: case Screen::TouchCheck:
            return true;
        default:
            // The reading canvas opts into its independent chrome path.
            return false;
        }
    }
    // Reuse existing locale keys without changing the installed pack/API schema.
    inline std::string_view caption(ui::Context& ui, UiText key, std::string_view english) {
        const auto localized = ui.text(key);
        return localized == Localization::text(key) ? english : localized;
    }
    Action header(ui::Context& ui, std::string_view title, Screen& screen, Screen parent = Screen::Read,
                  std::string_view trailing = {});
    Action menu(ui::Context& ui, std::string_view title, std::string_view author, uint8_t progress,
                bool hasBook, settings::InterfaceSettings& interface, Screen& screen, size_t chapter = 0);
    Action hub(ui::Context& ui, Screen& screen);
    struct DictionarySetup {
        bool installed = false;
        bool confirming = false;
        bool onboarding = false;
        std::string message;
    };
    Action dictionarySetup(ui::Context& ui, DictionarySetup& state, Screen& screen);
    Action quickControls(ui::Context& ui, settings::ReadingSettings& reading,
                         settings::InterfaceSettings& interface, Screen& screen);
}
