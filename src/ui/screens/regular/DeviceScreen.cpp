#include "ui/screens/ScreenCommon.h"
#include "ui/screens/ModernScreens.h"

namespace screens {
    namespace {
        bool encryptionAcknowledged = false;
        bool encryptionControlsVisible = false;
    } // namespace

    Action device(ui::Context& ui, bool storageReady, size_t bookCount, settings::NvsEncryptionState encryptionState,
                  Screen& screen) {
        if (!ui.modernPresentation())
            if (const Action action = detail::navigation(ui, Screen::Device, screen); action != Action::None) return action;
        const ui::Rect content = detail::tabContent(ui);
        constexpr int16_t gap = 4;

        std::string storageStatus;
        if (storageReady) {
            storageStatus = std::to_string(bookCount);
            storageStatus += " ";
            storageStatus += ui.text(UiText::Items);
        } else {
            storageStatus = ui.text(UiText::Unavailable);
        }
        const std::string_view encryptionStatus =
            ui.text(encryptionState == settings::NvsEncryptionState::Enabled                     ? UiText::On
                    : encryptionState == settings::NvsEncryptionState::Available && storageReady ? UiText::Off
                                                                                                 : UiText::Unavailable);

        if (ui.modernPresentation()) {
            using ui::modern::button;
            using ui::modern::Style;
            const auto current = screen;
            if (const auto action = modern::header(ui, current == Screen::StorageHub ? ui.text(UiText::Storage)
                                                       : current == Screen::PowerHub ? "Sleep & power"
                                                       : ui.text(UiText::Device),
                                                   screen, current == Screen::Device ? Screen::Settings : Screen::Device);
                action != Action::None) return action;
            if (current == Screen::Device) {
                const std::string_view language = ui.locale() == "en" ? "English" : ui.locale() == "de" ? "Deutsch"
                                               : ui.locale() == "ro" ? "Română" : ui.locale();
                if (button(ui,{12,51,198,53},"Storage",Style::Secondary,true,storageStatus)) return Action::StorageStatus;
                if (button(ui,{220,51,198,53},ui.text(UiText::Language),Style::Secondary,true,language)) return Action::CycleLanguage;
                if (button(ui,{428,51,200,53},"Firmware & updates",Style::Secondary,true,"Version and updates",13)) screen=Screen::Ota;
                if (button(ui,{12,111,300,53},"Settings encryption",Style::Secondary,true,encryptionStatus)) {
                    encryptionAcknowledged=false;encryptionControlsVisible=false;screen=Screen::StorageEncryption;
                }
                if (button(ui,{328,111,300,53},"Power off",Style::Secondary,true,"Save your place and shut down")) return Action::PowerOff;
                return Action::None;
            }
            if (current == Screen::PowerHub) {
                ui.label({16, 56, 330, 34}, "A place to pause.", 21);
                ui.label({16, 96, 330, 58}, "Your reading place stays saved.\nChoose when the screen rests.", 13, ui::themes::Muted, ui::TextAlign::Start, 2);
                if (button(ui, {374, 58, 250, 44}, "Sleep & screensaver →")) screen = Screen::InterfaceSettings;
                if (button(ui, {374, 116, 250, 44}, ui.text(UiText::PowerOff), Style::Primary)) return Action::PowerOff;
                return Action::None;
            }
            ui.label({16, 56, 330, 34}, storageStatus, 26);
            ui.label({16, 94, 330, 27}, storageReady ? "Your library, ready to read." : ui.text(UiText::CheckSdCard), 13, ui::themes::Muted);
            if (button(ui, {16, 128, 330, 36}, ui.text(UiText::RefreshRss), Style::Accent)) return Action::RssRefresh;
            if (button(ui, {374, 52, 250, 34}, "Check storage")) return Action::StorageStatus;
            const ui::Rect encryption{374, 91, 250, 34};
            const bool editEncryption = button(ui, encryption, ui.text(UiText::Encryption), Style::Secondary,
                                               true, {}, 13);
            const auto* touch = ui.touch();
            const bool holdEncryption = encryptionState == settings::NvsEncryptionState::Enabled
                                     && touch && ui::hasTouch(*touch, ui::TouchHold)
                                     && ui::contains(encryption, touch->x, touch->y);
            if ((editEncryption && storageReady && encryptionState == settings::NvsEncryptionState::Available)
                || holdEncryption) {
                encryptionAcknowledged = false;
                encryptionControlsVisible = false;
                screen = Screen::StorageEncryption;
            }
            if (button(ui, {374, 130, 250, 34}, ui.text(UiText::UsbTransfer), Style::Accent)) return Action::UsbTransfer;
            return Action::None;
        }

        ui::Rect storageButton;
        ui::Rect encryptionButton;
        ui::Grid actions;
        if (content.w >= 280) {
            constexpr int16_t statusHeight = 60;
            const int16_t statusWidth = static_cast<int16_t>((content.w - gap) / 2);
            storageButton = {content.x, content.y, statusWidth, statusHeight};
            encryptionButton = {static_cast<int16_t>(content.x + statusWidth + gap), content.y,
                                static_cast<int16_t>(content.w - statusWidth - gap), statusHeight};
            const int16_t actionsY = static_cast<int16_t>(content.y + statusHeight + gap);
            const int16_t actionsHeight = static_cast<int16_t>(content.y + content.h - actionsY);
            actions = {{content.x, actionsY, content.w, actionsHeight},
                       2,
                       static_cast<int16_t>((actionsHeight - gap) / 2),
                       gap};
        } else {
            ui::Grid all{content, 1, static_cast<int16_t>((content.h - gap * 5) / 6), gap};
            storageButton = all.next();
            encryptionButton = all.next();
            actions = all;
        }

        if (ui.setting(storageButton, ui.text(UiText::Storage), storageStatus, ui::SettingLayout::Inline))
            return Action::StorageStatus;
        if (ui.setting(encryptionButton, ui.text(UiText::Encryption), encryptionStatus, ui::SettingLayout::Inline)
            && storageReady && encryptionState == settings::NvsEncryptionState::Available) {
            encryptionAcknowledged = false;
            encryptionControlsVisible = false;
            screen = Screen::StorageEncryption;
        }
        const ui::Touch* touch = ui.touch();
        if (encryptionState == settings::NvsEncryptionState::Enabled && touch != nullptr
            && ui::hasTouch(*touch, ui::TouchHold) && ui::contains(encryptionButton, touch->x, touch->y)) {
            encryptionAcknowledged = false;
            encryptionControlsVisible = false;
            screen = Screen::StorageEncryption;
        }
        if (ui.button(actions.next(), ui.text(UiText::UsbTransfer), true, ui::Icon::None, 2))
            return Action::UsbTransfer;
        if (ui.button(actions.next(), ui.text(UiText::CompanionSync), true, ui::Icon::None, 2))
            return Action::CompanionSync;
        if (ui.button(actions.next(), ui.text(UiText::RefreshRss), true, ui::Icon::None, 2))
            return Action::RssRefresh;
        if (ui.button(actions.next(), ui.text(UiText::OtaUpdate), true, ui::Icon::None, 2))
            screen = Screen::Ota;
        return Action::None;
    }

    Action storageEncryption(ui::Context& ui, settings::NvsEncryptionState encryptionState, Screen& screen) {
        if (screen != Screen::StorageEncryption) {
            encryptionAcknowledged = false;
            encryptionControlsVisible = false;
            return Action::None;
        }

        if (ui.modernPresentation()) {
            using ui::modern::button;
            using ui::modern::Style;
            const auto back = [&] {
                encryptionAcknowledged=false;encryptionControlsVisible=false;screen=Screen::Device;
            };
            if (button(ui,{8,0,94,44},"‹ Device",Style::Plain)) back();
            ui.label({112,0,340,44},"Settings encryption",16);
            ui::modern::rule(ui,{0,43,640,1});
            if (!encryptionControlsVisible) {
                if (button(ui,{480,0,152,44},"Continue →",Style::Accent)) {
                    encryptionControlsVisible=true;ui.invalidate();
                }
                ui::modern::rule(ui,{0,43,640,1});
                ui.label({16,50,608,34},ui.text(UiText::StorageEncryptionExplanation),13,ui::themes::Foreground,ui::TextAlign::Start,2);
                ui.label({16,88,608,38},ui.text(UiText::StorageEncryptionPermanent),13,ui::themes::Accent,ui::TextAlign::Start,2);
                ui.label({16,132,608,36},ui.text(UiText::StorageEncryptionReset),13,ui::themes::Muted,ui::TextAlign::Start,2);
                return Action::None;
            }
            const bool available=encryptionState==settings::NvsEncryptionState::Available;
            ui.label({16,53,306,34},encryptionState==settings::NvsEncryptionState::Enabled?"Protection is enabled":available?"A permanent device change":"Protection unavailable",17);
            ui.label({16,92,306,72},ui.text(UiText::StorageEncryptionPermanent),13,ui::themes::Muted,ui::TextAlign::Start,4);
            if (button(ui,{342,51,286,53},encryptionAcknowledged?"Acknowledged":"I understand",Style::Secondary,available,
                       "Hardware fuse · NVS reset")) encryptionAcknowledged=!encryptionAcknowledged;
            if (button(ui,{342,111,286,53},ui.text(UiText::EnableProtection),Style::Secondary,available && encryptionAcknowledged)) {
                encryptionAcknowledged=false;encryptionControlsVisible=false;
                return Action::EnableStorageEncryption;
            }
            return Action::None;
        }

        const ui::Rect content = detail::content(ui);
        if (!encryptionControlsVisible) {
            ui::Column notes{content, 2};
            ui.label(notes.next(20), ui.text(UiText::StorageEncryption), 2, ui::themes::ColorRole::Foreground,
                     ui::TextAlign::Center);
            ui.label(notes.next(36), ui.text(UiText::StorageEncryptionExplanation), 2,
                     ui::themes::ColorRole::Foreground, ui::TextAlign::Start, 3);
            ui.label(notes.next(36), ui.text(UiText::StorageEncryptionPermanent), 2, ui::themes::ColorRole::Accent,
                     ui::TextAlign::Start, 3);
            ui.label(notes.next(36), ui.text(UiText::StorageEncryptionReset), 2, ui::themes::ColorRole::Foreground,
                     ui::TextAlign::Start, 3);
            ui.label(notes.next(20), ui.text(UiText::TapToContinue), 2, ui::themes::ColorRole::Muted,
                     ui::TextAlign::Center);
            if (ui.tap(content)) {
                encryptionControlsVisible = true;
                ui.invalidate();
            }
            return Action::None;
        }

        constexpr int16_t panelWidth = 440;
        constexpr int16_t panelHeight = 144;
        const int16_t width = std::min<int16_t>(content.w, panelWidth);
        const int16_t height = std::min<int16_t>(content.h, panelHeight);
        const ui::Rect panel{static_cast<int16_t>(content.x + (content.w - width) / 2),
                             static_cast<int16_t>(content.y + (content.h - height) / 2), width, height};
        ui::Column actions{panel, 6};
        ui.label(actions.next(22), ui.text(UiText::StorageEncryption), 2, ui::themes::ColorRole::Foreground,
                 ui::TextAlign::Center);
        const int16_t controlHeight = std::max<int16_t>(1, static_cast<int16_t>((height - 34) / 2));
        ui.toggle(actions.next(controlHeight), ui.text(UiText::IUnderstand), encryptionAcknowledged);

        ui::Row buttons{actions.next(controlHeight), 8};
        const int16_t buttonWidth = static_cast<int16_t>((buttons.bounds.w - buttons.gap) / 2);
        if (ui.button(buttons.next(buttonWidth), "<<")) {
            encryptionAcknowledged = false;
            encryptionControlsVisible = false;
            screen = Screen::Device;
            return Action::None;
        }
        if (ui.button(buttons.next(buttonWidth), ui.text(UiText::EnableProtection),
                      encryptionAcknowledged && encryptionState == settings::NvsEncryptionState::Available)) {
            encryptionAcknowledged = false;
            encryptionControlsVisible = false;
            return Action::EnableStorageEncryption;
        }
        return Action::None;
    }

} // namespace screens
