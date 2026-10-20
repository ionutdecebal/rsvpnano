#include "network/WifiConnection.h"
#include <WiFi.h>
#include <cassert>
#include <cstdio>
int main(){
    settings::NetworkSettings network{"Home"};settings::DeviceSecrets secrets;secrets.wifiPassword="home";
    settings::wifi::remember(network,secrets,"Hotspot","phone");
    WiFi.works="Home";WiFi.visible={{"Home",-42}};
    assert(net::connectSaved(network,secrets));assert(WiFi.attempts==std::vector<std::string>{"Home"});
    // Being connected doesn't rescan or interrupt an existing connection.
    assert(net::connectSaved(network,secrets));assert(WiFi.scans==1);
    WiFi={};fakeNow=0;WiFi.works="Hotspot";WiFi.visible={{"Home",-40},{"Hotspot",-60}};
    assert(net::connectSaved(network,secrets));assert(WiFi.attempts==std::vector<std::string>({"Home","Hotspot"}));
    assert(fakeNow==15000); // Wrong first network falls back, within the total budget.
    WiFi={};fakeNow=0;
    assert(!net::connectSaved(network,secrets));assert(fakeNow==30000);assert(WiFi.clears==1);
    assert(secrets.wifiNetworks.size()==2); // Failed/empty scan retains all saved data.
    WiFi={};WiFi.currentMode=WIFI_AP_STA;WiFi.visible={{"Home",-42}};
    auto order=net::savedConnectionOrder(network,secrets);assert(order.front().ssid=="Home");assert(WiFi.currentMode==WIFI_AP_STA);
    WiFi={};fakeNow=0;network={};secrets={};assert(!net::connectSaved(network,secrets));assert(WiFi.attempts.empty());assert(!fakeNow);
    network.ssid="Legacy";secrets.wifiPassword="old";WiFi.works="Legacy";
    assert(net::connectSaved(network,secrets));assert(!WiFi.scans);
    net::disconnect();assert(WiFi.currentMode==WIFI_OFF);
    puts("Saved Wi-Fi: visible selection, existing connection, failed-password fallback, bounded timeout, retained credentials, AP preservation, empty/legacy settings passed.");
}
