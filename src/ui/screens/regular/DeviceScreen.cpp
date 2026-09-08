#include "ui/screens/ScreenCommon.h"

namespace screens {
    namespace {
        bool encryptionAcknowledged = false;
        bool encryptionControlsVisible = false;
    } // namespace

    Action device(ui::Context& ui, bool storageReady, size_t bookCount, settings::NvsEncryptionState encryptionState,
                  Screen& screen) {
        if (const Action action = detail::navigation(ui, Screen::Device, screen); action != Action::None)
            return action;
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
