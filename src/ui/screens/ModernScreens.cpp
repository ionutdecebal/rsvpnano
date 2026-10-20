#include "ui/screens/ModernScreens.h"
#include "ui/screens/OnboardingCopy.h"

namespace screens::modern {
    using ui::modern::button;
    using ui::modern::Style;

    Action dictionarySetup(ui::Context& ui, DictionarySetup& state, Screen& screen) {
        if (button(ui, {8,0,112,43}, "‹ Back", Style::Plain, true, {}, 13)) {
            state.confirming = false;
            screen = state.onboarding ? Screen::Onboarding : Screen::DictionaryHelp;
            return Action::None;
        }
        ui.label({136,0,490,43}, "English dictionary · offline", 17);
        ui::modern::rule(ui,{0,43,640,1});
        ui.label({16,52,346,30}, state.confirming ? "Replace your current pack?"
            : state.installed ? "Dictionary installed" : "Install once. Read offline.", 19);
        ui.label({16,88,346,72}, !state.message.empty() && !state.confirming ? state.message
            : state.confirming ? "This replaces the current English pack. Keep a copy of a custom dictionary first."
            : "About 48 MiB on SD. Download over Wi-Fi or copy to SD. More languages are planned.",
            13,ui::themes::Muted,ui::TextAlign::Start,4);
        if (button(ui,{386,54,238,50},state.confirming ? "Confirm replacement"
            : state.installed ? "Replace dictionary" : "Install dictionary",Style::Primary,true,{},13)) {
            if (state.installed && !state.confirming) state.confirming = true;
            else return Action::InstallDictionary;
        }
        if (button(ui,{386,116,238,48},state.confirming ? "Keep current dictionary" : "Wi-Fi settings",
                   Style::Secondary,true,{},13)) {
            if (state.confirming) state.confirming = false;
            else screen = Screen::NetworkSettings;
        }
        return Action::None;
    }

    Action header(ui::Context& ui, std::string_view title, Screen& screen, Screen parent, std::string_view trailing) {
        auto parentText = parent == Screen::NetworkSettings ? std::string_view{"Wi-Fi"} : parent == Screen::Help ? copy(ui,Copy::Help) : parent == Screen::Read ? caption(ui, UiText::Read, "Menu")
                               : parent == Screen::ReadingHub ? caption(ui, UiText::Reading, "Pace")
                               : parent == Screen::DisplayHub ? ui.text(UiText::Display)
                               : parent == Screen::Connections ? caption(ui, UiText::Network, "Connect")
                               : parent == Screen::FocusTimers ? ui.text(UiText::Focus)
                               : ui.text(parent == Screen::Read ? UiText::Read
                                       : parent == Screen::Library ? UiText::Library
                                       : parent == Screen::Device ? UiText::Device : UiText::Settings);
        if (parent == Screen::Dictionary) parentText = "Dictionary";
        const bool dictionaryParent = parent == Screen::Dictionary;
        if (button(ui, {8, 0, int16_t(dictionaryParent ? 112 : 94), 43},
                   std::string{"‹ "} + std::string{parentText}, Style::Plain, true, {}, 13))
            screen = parent;
        ui.label({int16_t(dictionaryParent ? 132 : 112), 0, int16_t(dictionaryParent ? 202 : 222), 43}, title, 15);
        ui.label({340, 0, 132, 43}, trailing, 11, ui::themes::Accent, ui::TextAlign::Right);
        if (button(ui, {480, 0, 152, 43}, std::string{copy(ui,Copy::BackBook)}+" ↗", Style::Accent, true, {}, 13))
            return Action::Resume;
        ui::modern::rule(ui, {0, 43, 640, 1});
        return Action::None;
    }

    Action menu(ui::Context& ui, std::string_view title, std::string_view author, uint8_t progress,
                bool hasBook, settings::InterfaceSettings& interface, Screen& screen, size_t chapter) {
        ui.label({16, 8, 318, 20}, !ui.focusBadge().empty() ? std::string_view{ui.focusBadge()} : hasBook ? copy(ui,Copy::Saved) : ui.text(UiText::NoLibraryItems), 1,
                 ui::themes::Muted);
        ui.label({16, 32, 318, 35}, title, 3);
        ui.label({16, 67, 318, 23}, author.empty() ? ui.text(UiText::Unknown) : author, 1, ui::themes::Muted);
        ui.progress({16, 103, 178, 2}, progress);
        const auto location = std::to_string(progress) + "%" + (chapter ? " · " + std::string{ui.text(UiText::Chapter)} + " " + std::to_string(chapter) : "");
        ui.label({204, 94, 130, 20}, location, 11, ui::themes::Muted, ui::TextAlign::Right);
        if (button(ui, {16, 120, 154, 44}, std::string{copy(ui,Copy::BackBook)}+" ↗", Style::Primary, hasBook))
            return Action::Resume;
        if (button(ui, {180, 120, 154, 44}, ui.focusBadge().empty()?copy(ui,Copy::StartFocus):std::string_view{ui.focusBadge()}, Style::Accent))
            screen = Screen::FocusTimers;
        ui::modern::rule(ui, {352, 8, 1, 156});
        constexpr UiText titles[]{UiText::Chapters, UiText::Library, UiText::Settings};
        constexpr Screen destinations[]{Screen::Chapters, Screen::Library, Screen::Settings};
        constexpr ui::modern::Symbol symbols[]{ui::modern::Symbol::Chapters, ui::modern::Symbol::Book, ui::modern::Symbol::Settings};
        for (int row = 0; row < 3; ++row) {
            const ui::Rect rect{364, static_cast<int16_t>(8 + 52 * row), 260, 52};
            if (ui::modern::row(ui, rect, ui.text(titles[row]), {}, symbols[row])) screen = destinations[row];
        }
        return Action::None;
    }

    Action hub(ui::Context& ui, Screen& screen) {
        const Screen current = screen;
        const auto title = current == Screen::Connections ? caption(ui, UiText::Network, "Connections")
                           : current == Screen::DictionaryHelp ? std::string_view{"Dictionary setup"}
                           : current == Screen::Help ? std::string_view{"How to read"}
                           : ui.text(current == Screen::ReadingHub ? UiText::Reading : UiText::Display);
        if (auto action = header(ui, title, screen, current == Screen::DictionaryHelp ? Screen::Dictionary : current == Screen::Help ? Screen::Device : Screen::Settings);
            action != Action::None) return action;
        if (current == Screen::ReadingHub) {
            if (button(ui, {16, 52, 298, 112}, ui.text(UiText::Reading), Style::Secondary, true,
                       "Speed, mode & reading controls")) screen = Screen::ReadingSettings;
            if (button(ui, {326, 52, 298, 112}, ui.text(UiText::WordPacing), Style::Secondary, true,
                       "Punctuation, length & complexity")) screen = Screen::PacingSettings;
        } else if (current == Screen::DisplayHub) {
            if (button(ui, {16, 52, 298, 112}, ui.text(UiText::ReaderLayout), Style::Secondary, true,
                       "Preview your font, guides & controls")) screen = Screen::ReaderAppearance;
            if (button(ui, {326, 52, 298, 112}, ui.text(UiText::Display), Style::Secondary, true,
                       "Theme & sleep")) screen = Screen::InterfaceSettings;
        } else if (current == Screen::Connections) {
            if (button(ui, {12, 51, 300, 52}, "Wi-Fi & identity", Style::Secondary, true, "Networks & device name",13)) screen = Screen::NetworkSettings;
            if (button(ui, {328, 51, 300, 52}, ui.text(UiText::CompanionSync), Style::Secondary, true, "Local companion",13)) return Action::CompanionSync;
            if (button(ui, {12, 111, 300, 53}, ui.text(UiText::UsbTransfer), Style::Secondary, true, "Transfer with a cable",13)) return Action::UsbTransfer;
            if (button(ui, {328, 111, 300, 53}, ui.text(UiText::RefreshRss), Style::Secondary, true, "Refresh saved feeds",13)) return Action::RssRefresh;
        } else if (current == Screen::DictionaryHelp) {
            ui.label({16,50,298,27},"Offline English dictionary",17);
            ui.label({16,80,298,40},"Install over Wi-Fi or copy to SD.\nMore languages are planned.",13,ui::themes::Muted,ui::TextAlign::Start,2);
            if(button(ui,{16,124,298,40},"Install / manage dictionary",Style::Primary,true,{},13))return Action::OpenDictionarySetup;
            ui.label({334,50,290,27},"Look online when needed",17);
            ui.label({334,82,290,43},"Wiktionary receives the word when you tap Look online. Requires Wi-Fi.",13,ui::themes::Muted,ui::TextAlign::Start,2);
            if(button(ui,{334,128,290,36},"Wi-Fi settings",Style::Secondary,true,{},13))screen=Screen::NetworkSettings;
        } else if (current == Screen::Help) {
            ui.label({16, 52, 608, 112},
                     "BOOT starts or pauses reading.\nPWR switches between the menu and your book.\nPage scrolls by touch. Find place previews a jump.\nFocus runs in the background; breaks pause reading.",
                     13, ui::themes::Foreground, ui::TextAlign::Start, 4);
        }
        return Action::None;
    }
}

namespace screens::modern {
Action quickControls(ui::Context& ui,settings::ReadingSettings& reading,settings::InterfaceSettings& interface,Screen& screen){
    using ui::modern::button;using ui::modern::Style;
    if(button(ui,{16,8,104,72},copy(ui,Copy::Menu),Style::Secondary,true,{},13))screen=Screen::Read;
    if(button(ui,{128,8,104,72},copy(ui,Copy::Page),Style::Secondary,true,{},13))return Action::OpenPage;
    if(button(ui,{16,90,216,74},copy(ui,Copy::Dictionary),Style::Secondary,true,{},13))return Action::OpenDictionary;
    ui::modern::rule(ui,{246,8,1,156});
    ui.label({260,8,308,20},copy(ui,Copy::TrayTitle),11,ui::themes::Muted);
    if(button(ui,{260,42,58,58},"−",Style::Secondary,true,{},26))reading.wpm=std::max<int>(reading.wpm.min(),int(reading.wpm)-10);
    ui.label({326,36,176,56},std::to_string(reading.wpm),39,ui::themes::Foreground,ui::TextAlign::Center);
    ui.label({326,94,176,18},"WPM",11,ui::themes::Muted,ui::TextAlign::Center);
    if(button(ui,{510,42,58,58},"+",Style::Secondary,true,{},26))reading.wpm=std::min<int>(reading.wpm.max(),int(reading.wpm)+10);
    if(button(ui,{260,120,308,44},copy(ui,Copy::BackBook),Style::Primary))return Action::Resume;
    int brightness=interface.brightnessPercent;
    if(ui.verticalSlider({580,8,52,156},brightness,5,100))interface.brightnessPercent=brightness;
    return Action::None;
}
}
