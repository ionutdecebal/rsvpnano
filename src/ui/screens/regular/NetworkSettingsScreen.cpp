#include "ui/screens/ScreenCommon.h"

#include <WiFi.h>

#include <algorithm>
#include <functional>

#include "network/WifiConnection.h"
#include "settings/SettingsStore.h"

namespace screens {

    Action NetworkScreen::draw(ui::Context& ui, settings::SettingsStore& store, Screen& screen) {
        const ui::Rect content = detail::content(ui);
        const auto& settings = store.settings();
        const bool ssidStored = !settings.network.ssid.empty();
        constexpr int16_t gap = 4;
        constexpr int16_t backWidth = 56;
        const int16_t rowHeight = static_cast<int16_t>((content.h - gap * 3) / 4);
        if (ui.button({content.x, content.y, backWidth, rowHeight}, "<<"))
            screen = Screen::Settings;
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
            editValue_ = settings.updates.releaseTag;
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
            const std::string signal = std::to_string(network.rssi) + " dBm";
            if (ui.setting(grid.next(), network.ssid, signal, ui::SettingLayout::Inline)) {
                const bool savedNetwork = network.ssid == store.settings().network.ssid;
                if (!network.secured) {
                    password_.clear();
                    saveNetwork(store, network.ssid);
                    closeWifi();
                    screen = Screen::NetworkSettings;
                    return;
                }
                password_ = savedNetwork ? store.secrets().wifiPassword : std::string{};
                selectedNetworkIndex_ = index;
                keyboard_ = {};
                connectionFailed_ = false;
                screen = Screen::WifiConnect;
                return;
            }
        }
    }

} // namespace screens
