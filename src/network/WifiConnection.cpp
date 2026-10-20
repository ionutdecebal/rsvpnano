#include "network/WifiConnection.h"

#include <WiFi.h>

namespace net {
    namespace {

        constexpr uint32_t kWifiConnectPollMs = 250;

    } // namespace

    std::expected<void, std::error_code> connectStation(const char* ssid, const char* password,
                                                        const WifiProgress& progress, uint32_t timeoutMs) {
        if (ssid == nullptr || *ssid == '\0' || timeoutMs == 0)
            return std::unexpected(std::make_error_code(std::errc::invalid_argument));
        WiFi.persistent(false);
        WiFi.setAutoReconnect(true);
        WiFi.mode(WIFI_STA);
        WiFi.begin(ssid, password);

        const uint32_t startMs = millis();
        while (WiFi.status() != WL_CONNECTED && millis() - startMs < timeoutMs) {
            if (progress) {
                const uint32_t elapsedMs = millis() - startMs;
                progress(5 + static_cast<int>((elapsedMs * 15) / timeoutMs));
            }
            delay(kWifiConnectPollMs);
        }

        if (WiFi.status() != WL_CONNECTED)
            return std::unexpected(std::make_error_code(std::errc::timed_out));
        return {};
    }

    std::vector<settings::WifiProfile> savedConnectionOrder(const settings::NetworkSettings& network,
                                                           const settings::DeviceSecrets& secrets) {
        auto saved=settings::wifi::profiles(network,secrets);
        if(saved.size()<2)return saved;
        WiFi.persistent(false);
        if(WiFi.getMode()!=WIFI_AP_STA)WiFi.mode(WIFI_STA);
        const int count=WiFi.scanNetworks(false,false,false,200);
        std::vector<settings::wifi::Visible> visible;
        // Only retain matching networks, not a large neighbourhood scan.
        for(const auto& profile:saved){
            int best=-1000;
            for(int i=0;i<count;++i)if(profile.ssid==WiFi.SSID(i).c_str())best=std::max(best,int(WiFi.RSSI(i)));
            if(best>-1000)visible.push_back({profile.ssid,best});
        }
        WiFi.scanDelete();
        return settings::wifi::connectionOrder(std::move(saved),visible);
    }

    std::expected<void, std::error_code> connectSaved(const settings::NetworkSettings& network,
                                                      const settings::DeviceSecrets& secrets,
                                                      const WifiProgress& progress) {
        if(WiFi.status()==WL_CONNECTED)return {};
        const auto saved=savedConnectionOrder(network,secrets);
        if(saved.empty())return std::unexpected(std::make_error_code(std::errc::invalid_argument));
        for(size_t i=0;i<saved.size();++i){
            // A bounded association budget across all saved profiles (plus scan).
            auto connected=connectStation(saved[i].ssid.c_str(),saved[i].password.c_str(),progress,
                                           saved.size()==1?15000:30000/saved.size());
            if(connected)return {};
            WiFi.disconnect(false,false);
        }
        return std::unexpected(std::make_error_code(std::errc::timed_out));
    }

    void disconnect() {
        if (WiFi.getMode() == WIFI_MODE_NULL)
            return;
        WiFi.disconnect(true, false);
        WiFi.mode(WIFI_OFF);
    }

} // namespace net
