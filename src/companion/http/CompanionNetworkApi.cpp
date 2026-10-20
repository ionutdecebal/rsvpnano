#include "companion/http/CompanionApi.h"

#include <utility>

#include "settings/SettingsRules.h"
#include "settings/WifiProfiles.h"
#include "text/AsciiText.h"

namespace {

    namespace api = companion::api;

    [[nodiscard]] api::Result<api::NetworkUpdate> validateNetworkUpdate(api::NetworkUpdate update) {
        if (!update.ssid) {
            return std::unexpected(api::httpError(HTTP_CODE_UNPROCESSABLE_ENTITY, "invalid_network",
                                                  "Missing Wi-Fi SSID", "ssid"));
        }

        update.ssid = std::string{AsciiText::trim(*update.ssid)};
        if (update.ssid->empty()) {
            return std::unexpected(api::httpError(HTTP_CODE_UNPROCESSABLE_ENTITY, "invalid_network",
                                                  "Wi-Fi SSID is required", "ssid"));
        }
        if (update.ssid->size() > settings::rules::kWifiSsidMaxLength) {
            return std::unexpected(api::httpError(HTTP_CODE_UNPROCESSABLE_ENTITY, "invalid_network",
                                                  "Wi-Fi SSID is too long", "ssid"));
        }
        if (update.password && update.password->size() > settings::rules::kWifiPasswordMaxLength) {
            return std::unexpected(api::httpError(HTTP_CODE_UNPROCESSABLE_ENTITY, "invalid_network",
                                                  "Wi-Fi password is too long", "password"));
        }
        return update;
    }

} // namespace

void CompanionApi::storeNetwork(std::string ssid, std::string password) {
    if(ssid.empty()){
        settingsStore_.settings().network.ssid.clear();
        settingsStore_.secrets().wifiPassword.clear();
        settingsStore_.secrets().wifiNetworks.clear();
    }else settings::wifi::remember(settingsStore_.settings().network,settingsStore_.secrets(),std::move(ssid),std::move(password));
    settingsStore_.acceptChanges();
    settingsStore_.acceptSecretChanges();
    networkScreen_.begin(settingsStore_);
    networkScreen_.startupCheckPending = false;
}

companion::api::Result<const settings::NetworkSettings*> CompanionApi::getNetwork(httpd_req_t& request) {
    (void) request;
    return &settingsStore_.settings().network;
}

companion::api::Result<> CompanionApi::putNetwork(httpd_req_t& request) {
    return readJson<companion::api::NetworkUpdate>(request, 512, "Wi-Fi payload exceeds 512 bytes")
        .and_then([this](companion::api::NetworkUpdate update) { return updateNetwork(std::move(update)); });
}

companion::api::Result<> CompanionApi::updateNetwork(companion::api::NetworkUpdate update) {
    return validateNetworkUpdate(std::move(update)).transform([this](companion::api::NetworkUpdate validated) {
        storeNetwork(std::move(*validated.ssid), validated.password.value_or(""));
    });
}

companion::api::Result<> CompanionApi::deleteNetwork(httpd_req_t& request) {
    (void) request;
    storeNetwork({}, {});
    return {};
}
