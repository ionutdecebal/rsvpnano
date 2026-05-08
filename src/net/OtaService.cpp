// SPDX-License-Identifier: MIT

#include "net/OtaService.h"

#include <ArduinoOTA.h>
#include <WiFi.h>

#include "display/DisplayManager.h"

namespace OtaService {
namespace {

String ssid_;
String password_;
String hostname_;
String otaPassword_;
DisplayManager *display_ = nullptr;
bool otaStarted_ = false;
uint32_t lastReconnectMs_ = 0;

void startOtaIfReady() {
  if (otaStarted_ || WiFi.status() != WL_CONNECTED) return;

  ArduinoOTA.setHostname(hostname_.c_str());
  ArduinoOTA.setPassword(otaPassword_.c_str());

  ArduinoOTA.onStart([]() {
    Serial.println("[ota] update starting");
    if (display_) display_->renderProgress("OTA", "Update starting", "", 0);
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    static int lastPct = -1;
    int pct = (int)((progress * 100UL) / total);
    if (pct != lastPct) {
      lastPct = pct;
      if (display_) display_->renderProgress("OTA", "Updating firmware", "", pct);
    }
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("[ota] update complete");
    if (display_) display_->renderProgress("OTA", "Update complete", "Rebooting", 100);
  });
  ArduinoOTA.onError([](ota_error_t e) {
    Serial.printf("[ota] error %u\n", e);
  });

  ArduinoOTA.begin();
  otaStarted_ = true;
  Serial.printf("[ota] ready on %s.local (IP=%s)\n",
                hostname_.c_str(),
                WiFi.localIP().toString().c_str());
}

}  // namespace

void begin(const char *ssid, const char *password, const char *hostname,
           const char *otaPassword, DisplayManager *display) {
  ssid_ = ssid;
  password_ = password;
  hostname_ = hostname;
  otaPassword_ = otaPassword;
  display_ = display;

  // Skip if no SSID configured — keeps existing offline-only behaviour intact.
  if (ssid_.isEmpty()) {
    Serial.println("[ota] no SSID configured, skipping Wi-Fi");
    return;
  }

  WiFi.mode(WIFI_STA);
  WiFi.setHostname(hostname_.c_str());
  WiFi.begin(ssid_.c_str(), password_.c_str());
  Serial.printf("[wifi] connecting to %s ...\n", ssid_.c_str());

  // Brief blocking wait so first OTA after boot works without an extra cycle.
  // Don't block forever if the network is down.
  const uint32_t deadline = millis() + 8000;
  while (WiFi.status() != WL_CONNECTED && millis() < deadline) {
    delay(200);
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("[wifi] connected, IP=%s\n", WiFi.localIP().toString().c_str());
    startOtaIfReady();
  } else {
    Serial.println("[wifi] not connected yet — will retry in handle()");
  }
}

void handle() {
  if (ssid_.isEmpty()) return;  // no Wi-Fi configured, do nothing

  if (WiFi.status() != WL_CONNECTED) {
    otaStarted_ = false;
    const uint32_t now = millis();
    if (now - lastReconnectMs_ > 10000) {
      lastReconnectMs_ = now;
      WiFi.reconnect();
    }
    return;
  }

  startOtaIfReady();
  if (otaStarted_) ArduinoOTA.handle();
}

bool wifiConnected() { return WiFi.status() == WL_CONNECTED; }

}  // namespace OtaService
