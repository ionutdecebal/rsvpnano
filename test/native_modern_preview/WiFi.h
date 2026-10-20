#pragma once
#include <Arduino.h>
// Radio boundary only: the real NetworkScreen handles routing and keyboards.
inline constexpr int WIFI_STA=1, WIFI_SCAN_RUNNING=-1, WIFI_SCAN_FAILED=-2, WIFI_AUTH_OPEN=0;
struct PreviewWifi {
    int found=3;
    void mode(int) {}
    int scanNetworks(bool) { return WIFI_SCAN_RUNNING; }
    int scanComplete() { return found; }
    String SSID(int i) { return i==0 ? "Reading room" : i==1 ? "A very long network name" : "Guest network"; }
    int RSSI(int i) { return -42-i*15; }
    int encryptionType(int i) { return i==2 ? WIFI_AUTH_OPEN : 1; }
    void scanDelete() {}
};
inline PreviewWifi WiFi;
