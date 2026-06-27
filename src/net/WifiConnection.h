#pragma once

#include <Arduino.h>

#include <functional>

// The device's only station-mode WiFi lifecycle: bring the radio up to reach
// the internet (RSS feed checks, OTA update checks) and tear it back down.
// Both callers shared a byte-identical connect/poll/timeout loop before this
// module; the timeout policy now lives here in one place.
namespace net {

// Reports association progress as a percentage in [0, 100] while connecting.
using WifiProgress = std::function<void(int percent)>;

// Brings up WIFI_STA and blocks until associated or the connect timeout
// elapses. Returns true only when connected. progress may be null.
bool connectStation(const String &ssid, const String &password,
                    const WifiProgress &progress = nullptr);

// Disconnects and powers the radio off (WIFI_OFF).
void disconnect();

}  // namespace net
