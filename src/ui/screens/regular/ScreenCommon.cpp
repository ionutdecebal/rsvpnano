#include "ui/screens/ScreenCommon.h"
#include "ui/screens/ModernScreens.h"

namespace screens::detail {

    namespace {
        constexpr int16_t kRailWidth = 136;
        constexpr int16_t kContentGap = 12;
        constexpr int16_t kRightInset = 48;
    } // namespace

    Action navigation(ui::Context& ui, Screen active, Screen& screen) {
        if (ui.modernPresentation()) {
            const UiText title = active == Screen::Chapters ? UiText::Chapters
                               : active == Screen::Library ? UiText::Library
                               : active == Screen::FocusTimers ? UiText::Focus
                               : active == Screen::Ota ? UiText::FirmwareUpdates
                               : active == Screen::Device ? UiText::Device : UiText::Settings;
            return modern::header(ui, ui.text(title), screen,
                                  active == Screen::Ota ? Screen::Device
                                  : active == Screen::Device ? Screen::Settings : Screen::Read);
        }
        if (ui.width() < 620 || ui.height() < 150 || ui.height() > 240) {
            return Action::None;
        }

        const int16_t tabHeight = static_cast<int16_t>(ui.height() / 4);
        ui::Column tabs{{0, 0, kRailWidth, ui.height()}};
        if (ui.tab(tabs.next(tabHeight), ui.text(UiText::Read),
                   active == Screen::Read || active == Screen::Library || active == Screen::Chapters
                       || active == Screen::BookFonts,
                   ui::Icon::Books)) {
            screen = Screen::Read;
        }
        if (ui.tab(tabs.next(tabHeight), ui.text(UiText::Settings),
                   active == Screen::Settings || active == Screen::ReadingSettings
                       || active == Screen::InterfaceSettings || active == Screen::PacingSettings
                       || active == Screen::ReaderAppearance || active == Screen::NetworkSettings,
                   ui::Icon::Edit)) {
            screen = Screen::Settings;
        }
        if (ui.tab(tabs.next(tabHeight), ui.text(UiText::Device),
                   active == Screen::Device || active == Screen::StorageEncryption || active == Screen::Sync
                       || active == Screen::Ota,
                   ui::Icon::Device)) {
            screen = Screen::Device;
        }
        if (ui.tab(tabs.next(static_cast<int16_t>(ui.height() - tabHeight * 3)), ui.text(UiText::Focus),
                   active == Screen::FocusTimers || active == Screen::FocusEditor || active == Screen::FocusNameEdit
                       || active == Screen::FocusSession,
                   ui::Icon::Hourglass)) {
            screen = Screen::FocusTimers;
        }
        if (ui.iconButton({static_cast<int16_t>(ui.width() - 46), 4, 36, 36}, ui::Icon::Power)) {
            return Action::PowerOff;
        }
        return Action::None;
    }

    ui::Rect content(ui::Context& ui) {
        return {8, 8, static_cast<int16_t>(ui.width() - 16), static_cast<int16_t>(ui.height() - 16)};
    }

    ui::Rect tabContent(ui::Context& ui) {
        if (ui.modernPresentation()) return {16, 52, 608, 112};
        if (ui.width() >= 620 && ui.height() >= 150 && ui.height() <= 240) {
            const int16_t x = static_cast<int16_t>(kRailWidth + kContentGap);
            return {x, 8, static_cast<int16_t>(ui.width() - x - kRightInset), static_cast<int16_t>(ui.height() - 16)};
        }
        return content(ui);
    }

} // namespace screens::detail
