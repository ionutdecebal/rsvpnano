#include "ui/screens/ScreenCommon.h"

#include <WiFi.h>

#include <algorithm>
#include <functional>

#include "network/WifiConnection.h"
#include "settings/SettingsStore.h"

namespace screens {
    void NetworkScreen::updateWifiScan() {
        if (scanState_ == WifiScanState::Idle) {
            WiFi.mode(WIFI_STA);
            scanState_ = WiFi.scanNetworks(true) == WIFI_SCAN_RUNNING ? WifiScanState::Scanning : WifiScanState::Failed;
        }
        if (scanState_ == WifiScanState::Scanning) {
            const int16_t found = WiFi.scanComplete();
            if (found == WIFI_SCAN_FAILED) {
                scanState_ = WifiScanState::Failed;
            } else if (found >= 0) {
                for (int16_t index = 0; index < found; ++index) {
                    const String foundSsid = WiFi.SSID(index);
                    if (foundSsid.isEmpty())
                        continue;
                    const std::string candidate{foundSsid.c_str(), foundSsid.length()};
                    const int32_t rssi = WiFi.RSSI(index);
                    const bool secured = WiFi.encryptionType(index) != WIFI_AUTH_OPEN;
                    const auto activeNetworks = std::span{networks_}.first(networkCount_);
                    const auto existing = std::ranges::find(activeNetworks, candidate, &WifiNetwork::ssid);
                    if (existing == activeNetworks.end()) {
                        if (networkCount_ < networks_.size()) {
                            networks_[networkCount_] = {candidate, rssi, secured};
                            ++networkCount_;
                        } else {
                            const auto weakest =
                                std::ranges::min_element(networks_, std::ranges::less{}, &WifiNetwork::rssi);
                            if (rssi > weakest->rssi)
                                *weakest = {candidate, rssi, secured};
                        }
                    } else if (rssi > existing->rssi) {
                        existing->rssi = rssi;
                        existing->secured = secured;
                    }
                }
                std::ranges::sort(std::span{networks_}.first(networkCount_), std::ranges::greater{},
                                  &WifiNetwork::rssi);
                WiFi.scanDelete();
                scanState_ = WifiScanState::Complete;
            }
        }
    }
    void NetworkScreen::begin(settings::SettingsStore& store) {
        const auto& persisted = store.settings();
        password_.clear();
        selectedNetworkIndex_ = networks_.size();
        startupCheckPending = persisted.updates.checkOnStartup && !persisted.network.ssid.empty();
    }

    void NetworkScreen::openWifiScan() {
        closeWifi();
        networkCount_ = 0;
        selectedNetworkIndex_ = networks_.size();
        scanState_ = WifiScanState::Idle;
    }

    void NetworkScreen::closeWifi() {
        WiFi.scanDelete();
        net::disconnect();
    }

    bool NetworkScreen::drawWifiConnect(ui::Context& ui, settings::SettingsStore& store, Screen& screen) {
        const ui::Rect content = detail::content(ui);
        if (selectedNetworkIndex_ >= networkCount_) {
            screen = Screen::WifiScan;
            return false;
        }
        const std::string& ssid = networks_[selectedNetworkIndex_].ssid;
        const std::string_view label = connectionFailed_ ? ui.text(UiText::ConnectionFailed) : std::string_view{ssid};
        const ui::KeyboardAction action = ui.keyboard(content, password_, 63, keyboard_, label, true);
        if (action == ui::KeyboardAction::Cancel) {
            screen = Screen::WifiScan;
            return false;
        }
        if (action != ui::KeyboardAction::Submit)
            return false;

        ui.endFrame();
        status(ui, ui.text(UiText::Connecting), ssid);
        const auto connected = net::connectStation(ssid.c_str(), password_.c_str(), [&](int percent) {
            status(ui, ui.text(UiText::Connecting), ssid, {}, percent);
        });
        net::disconnect();
        if (!connected) {
            connectionFailed_ = true;
            return true;
        }
        saveNetwork(store, ssid);
        screen = Screen::NetworkSettings;
        return true;
    }

    void NetworkScreen::drawEdit(ui::Context& ui, settings::SettingsStore& store, Screen& screen) {
        const ui::Rect content = detail::content(ui);
        const ui::KeyboardAction action =
            ui.keyboard(content, editValue_, 63, keyboard_,
                        ui.text(editField_ == EditField::Owner ? UiText::OtaOwner : UiText::ReleaseTag));
        if (action == ui::KeyboardAction::Cancel) {
            screen = Screen::NetworkSettings;
        } else if (action == ui::KeyboardAction::Submit) {
            if (editField_ == EditField::Owner) {
                store.settings().updates.repositoryOwner = editValue_;
            } else {
                store.settings().updates.releaseTag = editValue_;
            }
            store.acceptChanges();
            screen = Screen::NetworkSettings;
        }
    }

    void NetworkScreen::saveNetwork(settings::SettingsStore& store, std::string_view ssid) {
        store.settings().network.ssid = ssid;
        store.secrets().wifiPassword = password_;
        store.acceptChanges();
        store.acceptSecretChanges();
    }

} // namespace screens
