#pragma once

#include <Arduino.h>

#include <expected>
#include <functional>
#include <system_error>
#include "settings/WifiProfiles.h"

namespace net {

    // Reports association progress as a percentage in [0, 100] while connecting.
    using WifiProgress = std::function<void(int percent)>;

    // Brings up WIFI_STA and blocks until associated or the connect timeout
    // elapses. Returns true only when connected. progress may be null.
    std::expected<void, std::error_code> connectStation(const char* ssid, const char* password,
                                                        const WifiProgress& progress = nullptr,
                                                        uint32_t timeoutMs = 15000);

    // Scan without disabling an existing companion access point. Credentials
    // are never copied to public settings or logged.
    std::vector<settings::WifiProfile> savedConnectionOrder(const settings::NetworkSettings& network,
                                                           const settings::DeviceSecrets& secrets);
    std::expected<void, std::error_code> connectSaved(const settings::NetworkSettings& network,
                                                      const settings::DeviceSecrets& secrets,
                                                      const WifiProgress& progress = nullptr);

    // Disconnects and powers the radio off (WIFI_OFF).
    void disconnect();

} // namespace net
