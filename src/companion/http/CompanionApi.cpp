#include "companion/http/CompanionApi.h"

#include <ESPmDNS.h>
#include <WiFi.h>
#include <esp_log.h>
#include "board/BoardConfig.h"

#include <algorithm>
#include <cstdio>
#include <expected>
#include <string>
#include <utility>

#include "board/BoardStorage.h"
#include "logging/Logger.h"
#include "text/AsciiText.h"
#include "update/OtaUpdater.h"

namespace {

    struct StartupFailure {
        std::string status;
        std::string detail;
    };

    [[nodiscard]] std::string httpUrl(const IPAddress& address) {
        return std::string{"http://"} + address.toString().c_str();
    }

} // namespace

bool CompanionApi::begin() {
    if (active())
        return true;

    Logger::checkpoint("companion_start");
    stationConnected_.store(false);
    statusLine1_ = "Starting sync";
    statusLine2_ = "Preparing Wi-Fi";
    readerScreen_.releaseRuntimeCaches();

    auto startup = startAccessPoint()
                       .transform_error([](std::string detail) {
                           return StartupFailure{"Wi-Fi failed", std::move(detail)};
                       })
                       .and_then([this] {
                           return startServer().transform_error([](std::string detail) {
                               return StartupFailure{"Server failed", std::move(detail)};
                           });
                       })
                       .and_then([this] {
                           return startNetworkEvents().transform_error([](std::string detail) {
                               return StartupFailure{"Wi-Fi failed", std::move(detail)};
                           });
                       });
    if (!startup) {
        StartupFailure failure = std::move(startup.error());
        end();
        statusLine1_ = std::move(failure.status);
        statusLine2_ = std::move(failure.detail);
        return false;
    }

    active_.store(true);
    statusLine1_ = accessPointSsid_;
    statusLine2_ = httpUrl(WiFi.softAPIP());
    ESP_LOGI("companion", "ready ssid=%s url=%s", accessPointSsid_.c_str(), statusLine2_.c_str());

    if (auto station = startStation(); !station) {
        ESP_LOGW("companion", "station unavailable; direct connection remains active: %s", station.error().c_str());
    }
    queueNetworkState();
    Logger::checkpoint("companion_sync");
    return true;
}

void CompanionApi::end() {
    Logger::checkpoint("companion_stop");
    active_.store(false);
    stationConnected_.store(false);
    Logger::checkpoint("companion_stop_events");
    stopNetworkEvents();
    {
        const std::lock_guard lock{networkStateMutex_};
        Logger::checkpoint("companion_stop_mdns");
        stopMdns();

        Logger::checkpoint("companion_stop_wifi");
        WiFi.setAutoReconnect(false);
        WiFi.mode(WIFI_OFF);
    }
    Logger::checkpoint("companion_stop_http");
    drainServer();

    readerScreen_.refreshTypography();

    accessPointSsid_.clear();
    stationUrl_.clear();
    statusLine1_ = "Idle";
    statusLine2_.clear();
    Logger::checkpoint("running");
}

bool CompanionApi::active() const {
    return active_.load();
}

std::string_view CompanionApi::statusLine1() const {
    if (stationConnected_.load())
        return settingsStore_.settings().network.ssid;
    return statusLine1_;
}

std::string_view CompanionApi::statusLine2() const {
    if (stationConnected_.load())
        return stationUrl_;
    return statusLine2_;
}

CompanionApi::OperationResult CompanionApi::startAccessPoint() {
    accessPointSsid_ = "RSVP-Nano-" + deviceSuffix();

    WiFi.persistent(false);
    WiFi.setAutoReconnect(false);
    if (!WiFi.mode(WIFI_AP_STA))
        return std::unexpected("Could not enable AP+STA mode");

    const IPAddress address{192, 168, 4, 1};
    const IPAddress subnet{255, 255, 255, 0};
    if (!WiFi.softAPConfig(address, address, subnet))
        return std::unexpected("Could not configure the direct connection address");

    if (!WiFi.softAP(accessPointSsid_.c_str()))
        return std::unexpected("Could not start the direct connection access point");

    ESP_LOGI("companion", "softAP ssid=%s ip=%s", accessPointSsid_.c_str(), WiFi.softAPIP().toString().c_str());
    return {};
}

CompanionApi::OperationResult CompanionApi::startStation() {
    const std::string& ssid = settingsStore_.settings().network.ssid;
    if (ssid.empty())
        return {};

    if (WiFi.begin(ssid.c_str(), settingsStore_.secrets().wifiPassword.c_str()) == WL_CONNECT_FAILED)
        return std::unexpected("Could not start the saved Wi-Fi connection");

    ESP_LOGI("companion", "station connecting ssid=%s; softAP remains available", ssid.c_str());
    return {};
}

CompanionApi::OperationResult CompanionApi::startNetworkEvents() {
    wifiEventId_ = WiFi.onEvent([this](arduino_event_id_t event, arduino_event_info_t info) {
        switch (event) {
        case ARDUINO_EVENT_WIFI_AP_STACONNECTED:
            ESP_LOGI("companion", "softAP client connected");
            break;
        case ARDUINO_EVENT_WIFI_AP_STADISCONNECTED:
            ESP_LOGI("companion", "softAP client disconnected");
            break;
        case ARDUINO_EVENT_WIFI_AP_STAIPASSIGNED:
            ESP_LOGI("companion", "softAP assigned client ip=%s",
                     IPAddress(info.wifi_ap_staipassigned.ip.addr).toString().c_str());
            break;
        case ARDUINO_EVENT_WIFI_STA_GOT_IP:
        case ARDUINO_EVENT_WIFI_STA_LOST_IP:
        case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
            queueNetworkState();
            break;
        default:
            break;
        }
    });
    if (wifiEventId_ == 0)
        return std::unexpected("Could not register Wi-Fi events");
    return {};
}

void CompanionApi::stopNetworkEvents() {
    if (wifiEventId_ == 0)
        return;
    WiFi.removeEvent(wifiEventId_);
    wifiEventId_ = 0;
}

void CompanionApi::queueNetworkState() {
    if (!active() || server_ == nullptr)
        return;
    if (const esp_err_t error = httpd_queue_work(server_, &CompanionApi::applyNetworkState, this); error != ESP_OK) {
        ESP_LOGW("companion", "could not queue Wi-Fi event: %s", esp_err_to_name(error));
    }
}

void CompanionApi::applyNetworkState(void* context) {
    auto& self = *static_cast<CompanionApi*>(context);
    const std::lock_guard lock{self.networkStateMutex_};
    if (!self.active())
        return;
    const bool stationConnected = WiFi.status() == WL_CONNECTED;
    self.stationConnected_.store(false);

    if (!stationConnected) {
        self.stopMdns();
        ESP_LOGW("companion", "station disconnected; direct connection remains available at %s",
                 self.statusLine2_.c_str());
        screens::status(self.ui_, self.ui_.text(UiText::Sync), self.statusLine1_, self.statusLine2_);
        return;
    }

    if (auto mdns = self.startMdns(); !mdns)
        ESP_LOGW("companion", "station connected without mDNS discovery: %s", mdns.error().c_str());
    self.stationUrl_ = httpUrl(WiFi.localIP());
    self.stationConnected_.store(true);
    const std::string& ssid = self.settingsStore_.settings().network.ssid;
    ESP_LOGI("companion", "station ready ssid=%s ip=%s fallback=%s", ssid.c_str(), WiFi.localIP().toString().c_str(),
             self.accessPointSsid_.c_str());
    screens::status(self.ui_, self.ui_.text(UiText::Sync), ssid, self.stationUrl_);
}

CompanionApi::OperationResult CompanionApi::startMdns() {
    if (mdnsStarted_)
        return {};

    const std::string suffix = deviceSuffix();
    std::string hostname = "rsvp-nano-" + suffix;
    const std::string instanceName = "RSVP-Nano-" + suffix;
    std::ranges::transform(hostname, hostname.begin(), AsciiText::toLower);

    if (!MDNS.begin(hostname.c_str()))
        return std::unexpected("Could not start the mDNS responder");

    MDNS.setInstanceName(instanceName.c_str());
    if (!MDNS.addService("rsvpnano", "tcp", 80)) {
        MDNS.end();
        return std::unexpected("Could not advertise the companion service");
    }

    MDNS.addServiceTxt("rsvpnano", "tcp", "id", suffix.c_str());
    MDNS.addServiceTxt("rsvpnano", "tcp", "api", "2");
    mdnsStarted_ = true;
    return {};
}

void CompanionApi::stopMdns() {
    if (!mdnsStarted_)
        return;
    MDNS.end();
    mdnsStarted_ = false;
}

companion::api::Result<companion::api::DeviceInfo> CompanionApi::getDevice(httpd_req_t& request) {
    (void) request;
    return deviceInfo();
}

companion::api::DeviceInfo CompanionApi::deviceInfo() const {
    return companion::api::DeviceInfo{
        .ssid = accessPointSsid_.empty() ? "RSVP-Nano-" + deviceSuffix() : accessPointSsid_,
        .firmwareVersion = std::string{OtaUpdater::currentVersion()},
        .otaAsset = Board::Config::OTA_ASSET_NAME,
    };
}

companion::api::Result<StorageMigration::Report> CompanionApi::repairStorage(httpd_req_t& request) {
    (void) request;
    auto report = StorageMigration::repair(storage_.mounted(), {
                                                                   .libraryItems = storage_.books().size(),
                                                                   .fonts = readerScreen_.fonts.families().size() - 1,
                                                                   .themes = interfaceScreen_.themes.themes().size() - 1,
                                                               });
    if (storage_.mounted()) {
        storage_.refreshBooks();
        readerScreen_.fonts.loadFromSd();
        interfaceScreen_.themes.loadFromSd();
        localeCatalog_ = locales::scanInstalled(Board::Storage::filesystem(), static_cast<size_t>(UiText::Count));
    }
    return report;
}

std::string CompanionApi::deviceSuffix() const {
    const uint64_t mac = ESP.getEfuseMac();
    char suffix[7]{};
    std::snprintf(suffix, sizeof(suffix), "%06X", static_cast<unsigned int>(mac & 0xFFFFFF));
    return suffix;
}
