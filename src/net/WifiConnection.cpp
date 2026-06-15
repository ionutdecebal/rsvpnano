#include "net/WifiConnection.h"

#include <WiFi.h>

namespace net {
namespace {

constexpr uint32_t kWifiConnectTimeoutMs = 15000;
constexpr uint32_t kWifiConnectPollMs = 250;

}  // namespace

bool connectStation(const String &ssid, const String &password, const WifiProgress &progress) {
  WiFi.persistent(false);
  WiFi.setAutoReconnect(false);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid.c_str(), password.c_str());

  const uint32_t startMs = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startMs < kWifiConnectTimeoutMs) {
    if (progress) {
      const uint32_t elapsedMs = millis() - startMs;
      progress(5 + static_cast<int>((elapsedMs * 15) / kWifiConnectTimeoutMs));
    }
    delay(kWifiConnectPollMs);
  }

  return WiFi.status() == WL_CONNECTED;
}

void disconnect() {
  WiFi.disconnect(true, false);
  WiFi.mode(WIFI_OFF);
}

}  // namespace net
