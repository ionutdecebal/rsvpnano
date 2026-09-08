#include "ui/screens/watch/Layout.h"

namespace screens {
    Action NetworkScreen::draw(ui::Context& ui, settings::SettingsStore& store, Screen& screen) {
        auto area = watch::header(ui, ui.text(UiText::NetworkUpdates), Screen::Settings, screen);
        auto& config = store.settings();
        const bool saved = !config.network.ssid.empty();
        auto grid = ui.pagedGrid(area, saved ? 7 : 6, 1, 56);
        if (watch::setting(ui, grid.item(0), UiText::Network,
                           saved ? std::string_view{config.network.ssid} : ui.text(UiText::NotSet))) {
            openWifiScan();
            screen = Screen::WifiScan;
        }
        if (watch::toggle(ui, grid.item(1), UiText::StartupCheck, config.updates.checkOnStartup))
            store.acceptChanges();
        if (watch::setting(ui, grid.item(2), UiText::OtaOwner,
                           config.updates.repositoryOwner.empty() ? settings::kDefaultRepositoryOwner
                                                                  : std::string_view{config.updates.repositoryOwner})) {
            editField_ = EditField::Owner;
            editValue_ = config.updates.repositoryOwner;
            keyboard_ = {};
            screen = Screen::NetworkEdit;
        }
        if (watch::setting(ui, grid.item(3), UiText::ReleaseTag,
                           config.updates.releaseTag.empty() ? ui.text(UiText::Latest)
                                                             : std::string_view{config.updates.releaseTag})) {
            editField_ = EditField::Tag;
            editValue_ = config.updates.releaseTag;
            keyboard_ = {};
            screen = Screen::NetworkEdit;
        }
        if (ui.card(grid.item(4), ui.text(UiText::CompanionSetup), {}, watch::textSize(ui)))
            return Action::CompanionSync;
        if (ui.card(grid.item(5), ui.text(UiText::FirmwareUpdates), {}, watch::textSize(ui)))
            screen = Screen::Ota;
        if (saved && ui.card(grid.item(6), ui.text(UiText::ForgetNetwork), {}, watch::textSize(ui))) {
            password_.clear();
            saveNetwork(store, {});
            startupCheckPending = false;
        }
        return Action::None;
    }

    void NetworkScreen::drawWifiScan(ui::Context& ui, settings::SettingsStore& store, Screen& screen) {
        auto area = watch::header(ui, ui.text(UiText::Network), Screen::NetworkSettings, screen);
        if (screen != Screen::WifiScan) {
            closeWifi();
            return;
        }
        updateWifiScan();
        if (scanState_ == WifiScanState::Scanning || scanState_ == WifiScanState::Idle) {
            ui.label(area, ui.text(UiText::ScanningNetworks), watch::textSize(ui), ui::themes::Muted,
                     ui::TextAlign::Center, 2);
            return;
        }
        if (scanState_ == WifiScanState::Failed || networkCount_ == 0) {
            if (ui.card(area,
                        ui.text(scanState_ == WifiScanState::Failed ? UiText::ScanFailed : UiText::NoNetworksFound),
                        ui.text(UiText::Retry), watch::textSize(ui)))
                openWifiScan();
            return;
        }
        const auto grid = ui.pagedGrid(area, networkCount_, 1, 56);
        for (size_t index = grid.first; index < grid.first + grid.count; ++index) {
            const auto& network = networks_[index];
            if (!ui.card(grid.item(index), network.ssid, std::to_string(network.rssi) + " dBm", watch::textSize(ui)))
                continue;
            if (!network.secured) {
                password_.clear();
                saveNetwork(store, network.ssid);
                closeWifi();
                screen = Screen::NetworkSettings;
                return;
            }
            password_ = network.ssid == store.settings().network.ssid ? store.secrets().wifiPassword : std::string{};
            selectedNetworkIndex_ = index;
            keyboard_ = {};
            connectionFailed_ = false;
            screen = Screen::WifiConnect;
        }
    }
} // namespace screens
