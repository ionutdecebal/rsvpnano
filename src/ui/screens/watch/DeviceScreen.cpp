#include "ui/screens/watch/Layout.h"

namespace screens {
    namespace {
        unsigned explanation = 0;
        size_t explanationOffset = 0;
        size_t explanationBytes = 0;
        bool acknowledged = false;
    } // namespace

    Action device(ui::Context& ui, bool storageReady, size_t bookCount, settings::NvsEncryptionState encryptionState,
                  Screen& screen) {
        detail::navigation(ui, Screen::Device, screen);
        const auto area = detail::tabContent(ui);
        const auto grid = ui.pagedGrid(area, 7, 2, 56);
        if (ui.card(grid.item(0), ui.text(UiText::UsbTransfer), {}, watch::textSize(ui)))
            return Action::UsbTransfer;
        if (ui.card(grid.item(1), ui.text(UiText::CompanionSync), {}, watch::textSize(ui)))
            return Action::CompanionSync;
        if (ui.card(grid.item(2), ui.text(UiText::RefreshRss), {}, watch::textSize(ui)))
            return Action::RssRefresh;
        if (ui.card(grid.item(3), ui.text(UiText::OtaUpdate), {}, watch::textSize(ui)))
            screen = Screen::Ota;
        if (ui.card(grid.item(4), ui.text(UiText::Storage),
                    storageReady ? std::to_string(bookCount) + " " + std::string{ui.text(UiText::Items)}
                                 : std::string{ui.text(UiText::Unavailable)},
                    watch::textSize(ui)))
            return Action::StorageStatus;
        const auto encryptionStatus = encryptionState == settings::NvsEncryptionState::Enabled ? UiText::On
                                    : storageReady && encryptionState == settings::NvsEncryptionState::Available
                                        ? UiText::Off
                                        : UiText::Unavailable;
        if (ui.card(grid.item(5), ui.text(UiText::Encryption), ui.text(encryptionStatus), watch::textSize(ui))
            && storageReady && encryptionState == settings::NvsEncryptionState::Available) {
            explanation = 0;
            explanationOffset = 0;
            acknowledged = false;
            screen = Screen::StorageEncryption;
        }
        if (ui.card(grid.item(6), ui.text(UiText::PowerOff), {}, watch::textSize(ui), ui::themes::Accent,
                    ui::Icon::Power))
            return Action::PowerOff;
        return Action::None;
    }

    Action storageEncryption(ui::Context& ui, settings::NvsEncryptionState encryptionState, Screen& screen) {
        auto area = watch::header(ui, ui.text(UiText::StorageEncryption), Screen::Device, screen);
        constexpr std::array notes{UiText::StorageEncryptionExplanation, UiText::StorageEncryptionPermanent,
                                   UiText::StorageEncryptionReset};
        if (explanation < notes.size()) {
            const auto note = ui.text(notes[explanation]);
            const auto remaining = note.substr(std::min(explanationOffset, note.size()));
            const ui::Rect textArea{area.x, area.y, area.w, static_cast<int16_t>(area.h - 38)};
            if (ui.redraw(textArea, ui::Context::signature(remaining)))
                explanationBytes =
                    ui.fixedText(textArea, remaining, 2,
                                 ui.color(explanation == 1 ? ui::themes::Accent : ui::themes::Foreground),
                                 ui::TextAlign::Center, 8, false);
            if (ui.card({area.x, static_cast<int16_t>(area.y + area.h - 36), area.w, 36},
                        ui.text(UiText::TapToContinue), {}, 2)) {
                explanationOffset += explanationBytes;
                if (explanationOffset >= note.size()) {
                    ++explanation;
                    explanationOffset = 0;
                }
                ui.invalidate();
            }
            return Action::None;
        }
        ui::Grid grid{area, 1, static_cast<int16_t>((area.h - 4) / 2), 4};
        watch::toggle(ui, grid.next(), UiText::IUnderstand, acknowledged);
        if (ui.card(grid.next(), ui.text(UiText::EnableProtection), {}, watch::textSize(ui), ui::themes::Accent,
                    ui::Icon::None, acknowledged && encryptionState == settings::NvsEncryptionState::Available)) {
            acknowledged = false;
            explanation = 0;
            return Action::EnableStorageEncryption;
        }
        return Action::None;
    }
} // namespace screens
