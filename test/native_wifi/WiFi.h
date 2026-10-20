#pragma once
#include <string>
#include <vector>
inline constexpr int WIFI_STA=1,WIFI_AP_STA=3,WIFI_MODE_NULL=0,WIFI_OFF=0,WL_CONNECTED=3;
struct FakeWifi {
    struct Network {std::string ssid;int rssi;};
    int currentMode=0,scans=0,clears=0;
    bool connected=false,reconnect=false;
    std::string works;
    std::vector<Network> visible;
    std::vector<std::string> attempts;
    void persistent(bool){}
    void setAutoReconnect(bool enabled){reconnect=enabled;}
    int getMode(){return currentMode;}
    void mode(int m){currentMode=m;}
    void begin(const char* ssid,const char*){attempts.emplace_back(ssid);connected=works==ssid;}
    int status(){return connected?WL_CONNECTED:0;}
    int scanNetworks(bool,bool,bool,int){++scans;return visible.size();}
    std::string SSID(int i){return visible[i].ssid;}
    int RSSI(int i){return visible[i].rssi;}
    void scanDelete(){++clears;}
    void disconnect(bool off,bool){connected=false;if(off)currentMode=WIFI_OFF;}
};
inline FakeWifi WiFi;
