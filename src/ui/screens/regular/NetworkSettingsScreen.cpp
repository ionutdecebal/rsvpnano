#include "ui/screens/ScreenCommon.h"

#include "update/ReleaseSource.h"

#include <algorithm>
#include <functional>

#include "network/WifiConnection.h"
#include "settings/SettingsStore.h"
#include "ui/screens/ModernScreens.h"

namespace screens {

    Action NetworkScreen::draw(ui::Context& ui, settings::SettingsStore& store, Screen& screen) {
        const ui::Rect content = detail::content(ui);
        const auto& settings = store.settings();
        const bool ssidStored = !settings.network.ssid.empty();
        if (ui.modernPresentation()) {
            using ui::modern::button;
            using ui::modern::Style;
            if (const auto action = modern::header(ui, ui.text(UiText::Network), screen, Screen::Connections);
                action != Action::None) return action;
            if (ui::modern::row(ui, {16, 52, 292, 50}, "Wi-Fi network", ssidStored ? std::string_view{settings.network.ssid} : ui.text(UiText::NotSet), ui::modern::Symbol::Connect)) {
                openWifiScan(); screen = Screen::WifiScan;
            }
            if (button(ui, {16, 110, 140, 50}, "Saved ("+std::to_string(settings::wifi::profiles(settings.network,store.secrets()).size())+")", Style::Secondary)) {openSavedWifi();screen=Screen::WifiScan;}
            if (button(ui, {164, 110, 144, 50}, "Forget selected", Style::Plain, ssidStored)) {
                password_.clear(); saveNetwork(store, {}); startupCheckPending = false;
            }
            if (ui.toggle({328, 52, 296, 30}, ui.text(UiText::StartupCheck), store.settings().updates.checkOnStartup)) store.acceptChanges();
            if (button(ui, {328, 88, 144, 34}, ui.text(UiText::OtaOwner), Style::Secondary, true, {}, 13)) {
                editField_ = EditField::Owner; editValue_ = settings.updates.repositoryOwner; keyboard_ = {}; screen = Screen::NetworkEdit;
            }
            if (button(ui, {480, 88, 144, 34}, ui.text(UiText::ReleaseTag), Style::Secondary, true, {}, 13)) {
                editField_ = EditField::Tag; editValue_ = settings.updates.releaseTag.empty() ? update::kDefaultUiReleaseTag : settings.updates.releaseTag; keyboard_ = {}; screen = Screen::NetworkEdit;
            }
            if (button(ui, {328, 130, 296, 34}, ui.text(UiText::FirmwareUpdates), Style::Accent)) screen = Screen::Ota;
            return Action::None;
        }
        constexpr int16_t gap = 4;
        constexpr int16_t backWidth = 56;
        const int16_t rowHeight = static_cast<int16_t>((content.h - gap * 3) / 4);
        if (ui.button({content.x, content.y, backWidth, rowHeight}, "<<"))
            screen = ui.modernPresentation() ? Screen::Connections : Screen::Settings;
        if (ui.setting({static_cast<int16_t>(content.x + backWidth + gap), content.y,
                        static_cast<int16_t>(content.w - backWidth - gap), rowHeight},
                       ui.text(UiText::Network),
                       ssidStored ? std::string_view{settings.network.ssid} : ui.text(UiText::NotSet),
                       ui::SettingLayout::Inline)) {
            openWifiScan();
            screen = Screen::WifiScan;
        }

        const int16_t secondRowY = static_cast<int16_t>(content.y + rowHeight + gap);
        if (ui.toggle({content.x, secondRowY, content.w, rowHeight}, ui.text(UiText::StartupCheck),
                      store.settings().updates.checkOnStartup)) {
            store.acceptChanges();
        }
        const int16_t thirdRowY = static_cast<int16_t>(secondRowY + rowHeight + gap);
        const int16_t halfWidth = static_cast<int16_t>((content.w - gap) / 2);
        if (ui.setting({content.x, thirdRowY, halfWidth, rowHeight}, ui.text(UiText::OtaOwner),
                       settings.updates.repositoryOwner.empty() ? settings::kDefaultRepositoryOwner
                                                                 : std::string_view{settings.updates.repositoryOwner})) {
            editField_ = EditField::Owner;
            editValue_ = settings.updates.repositoryOwner;
            keyboard_ = {};
            screen = Screen::NetworkEdit;
        }
        if (ui.setting({static_cast<int16_t>(content.x + halfWidth + gap), thirdRowY, halfWidth, rowHeight},
                       ui.text(UiText::ReleaseTag),
                       settings.updates.releaseTag.empty() ? ui.text(UiText::Latest)
                                                           : std::string_view{settings.updates.releaseTag})) {
            editField_ = EditField::Tag;
            editValue_ = settings.updates.releaseTag.empty() ? update::kDefaultUiReleaseTag : settings.updates.releaseTag;
            keyboard_ = {};
            screen = Screen::NetworkEdit;
        }

        const int16_t actionsY = static_cast<int16_t>(thirdRowY + rowHeight + gap);
        ui::Grid actions{{content.x, actionsY, content.w, static_cast<int16_t>(content.y + content.h - actionsY)},
                         static_cast<uint8_t>(ssidStored ? 3 : 2),
                         static_cast<int16_t>(content.y + content.h - actionsY),
                         gap};
        if (ui.button(actions.next(), ui.text(UiText::CompanionSetup)))
            return Action::CompanionSync;
        if (ui.button(actions.next(), ui.text(UiText::FirmwareUpdates)))
            screen = Screen::Ota;
        if (ssidStored && ui.button(actions.next(), ui.text(UiText::ForgetNetwork))) {
            password_.clear();
            saveNetwork(store, {});
            startupCheckPending = false;
        }
        return Action::None;
    }

    void NetworkScreen::drawWifiScan(ui::Context& ui, settings::SettingsStore& store, Screen& screen) {
        const ui::Rect content = detail::content(ui);
        if(savedNetworks_&&ui.modernPresentation()){
            using ui::modern::button;using ui::modern::Style;
            if(modern::header(ui,"Saved Wi-Fi",screen,Screen::NetworkSettings)==Action::Resume){screen=Screen::Reader;return;}
            if(button(ui,{340,0,132,43},"Add network",Style::Plain,true,{},13)){openWifiScan();return;}
            const auto saved=settings::wifi::profiles(store.settings().network,store.secrets());
            if(saved.empty())ui.label({16,58,608,98},"No saved networks. Add your home Wi-Fi or hotspot once.",15,ui::themes::Muted,ui::TextAlign::Center,2);
            for(size_t i=0;i<saved.size();++i){
                if(button(ui,{int16_t(12+i%3*208),int16_t(52+i/3*58),198,52},saved[i].ssid,
                    saved[i].ssid==store.settings().network.ssid?Style::Accent:Style::Secondary,true,
                    saved[i].ssid==store.settings().network.ssid?"Selected · auto-connect":"Saved · auto-connect",13)){
                    password_=saved[i].password;saveNetwork(store,saved[i].ssid);screen=Screen::NetworkSettings;return;
                }
            }
            return;
        }
        updateWifiScan();

        constexpr int16_t gap = 4;
        constexpr int16_t backWidth = 56;
        if (scanState_ == WifiScanState::Idle || scanState_ == WifiScanState::Scanning) {
            if (ui.button({content.x, content.y, backWidth, detail::kBackButtonHeight}, "<<")) {
                closeWifi();
                screen = Screen::NetworkSettings;
                return;
            }
            ui.label({static_cast<int16_t>(content.x + backWidth + gap), content.y,
                      static_cast<int16_t>(content.w - backWidth - gap), content.h},
                     ui.text(UiText::ScanningNetworks), 2, ui::themes::ColorRole::Muted, ui::TextAlign::Center);
            return;
        }
        if (scanState_ == WifiScanState::Failed || networkCount_ == 0) {
            if (ui.button({content.x, content.y, backWidth, detail::kBackButtonHeight}, "<<")) {
                closeWifi();
                screen = Screen::NetworkSettings;
                return;
            }
            ui::Column column{{static_cast<int16_t>(content.x + backWidth + gap), content.y,
                               static_cast<int16_t>(content.w - backWidth - gap), content.h},
                              8};
            ui.label(column.next(56),
                     ui.text(scanState_ == WifiScanState::Failed ? UiText::ScanFailed : UiText::NoNetworksFound), 2,
                     ui::themes::ColorRole::Muted, ui::TextAlign::Center);
            if (ui.button(column.next(34), ui.text(UiText::Retry)))
                openWifiScan();
            return;
        }

        const uint8_t columns = content.w >= 600 ? 4 : 2;
        const size_t itemCount = networkCount_ + 1;
        const size_t rows = (itemCount + columns - 1) / columns;
        const int16_t rowGapTotal = static_cast<int16_t>(gap * (rows - 1));
        const int16_t rowHeight = static_cast<int16_t>((content.h - rowGapTotal) / rows);
        ui::Grid grid{content, columns, rowHeight, gap};
        if (ui.button(grid.next(), "<<")) {
            closeWifi();
            screen = Screen::NetworkSettings;
            return;
        }
        for (size_t index = 0; index < networkCount_; ++index) {
            const WifiNetwork& network = networks_[index];
            const auto savedPassword=settings::wifi::password(store.settings().network,store.secrets(),network.ssid);
            const std::string signal = savedPassword ? "Saved" : std::to_string(network.rssi) + " dBm";
            if (ui.setting(grid.next(), network.ssid, signal, ui::SettingLayout::Inline)) {
                if(savedPassword&&network.secured==!savedPassword->empty()){
                    password_=*savedPassword;saveNetwork(store,network.ssid);closeWifi();screen=Screen::NetworkSettings;return;
                }
                if (!network.secured) {
                    password_.clear();
                    saveNetwork(store, network.ssid);
                    closeWifi();
                    screen = Screen::NetworkSettings;
                    return;
                }
                password_ = savedPassword.value_or("");
                selectedNetworkIndex_ = index;
                keyboard_ = {};
                connectionFailed_ = false;
                screen = Screen::WifiConnect;
                return;
            }
        }
    }

} // namespace screens
