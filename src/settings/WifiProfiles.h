#pragma once
#include "settings/SettingsModel.h"
#include <algorithm>

namespace settings::wifi {
inline constexpr size_t kMaxProfiles=5;
inline void sanitize(DeviceSecrets& secrets){
    if(secrets.wifiPassword.size()>rules::kWifiPasswordMaxLength)secrets.wifiPassword.resize(rules::kWifiPasswordMaxLength);
    std::vector<WifiProfile> valid;
    for(const auto& profile:secrets.wifiNetworks){
        if(profile.ssid.empty()||profile.ssid.size()>rules::kWifiSsidMaxLength||profile.password.size()>rules::kWifiPasswordMaxLength)continue;
        if(std::ranges::find(valid,profile.ssid,&WifiProfile::ssid)!=valid.end())continue;
        valid.push_back(profile);if(valid.size()==kMaxProfiles)break;
    }
    secrets.wifiNetworks=std::move(valid);
}
inline std::vector<WifiProfile> profiles(const NetworkSettings& network,const DeviceSecrets& secrets){
    std::vector<WifiProfile> result;
    if(!network.ssid.empty())result.push_back({network.ssid,secrets.wifiPassword});
    for(const auto& profile:secrets.wifiNetworks){
        if(profile.ssid.empty()||std::ranges::find(result,profile.ssid,&WifiProfile::ssid)!=result.end())continue;
        if(result.size()==kMaxProfiles)break;
        result.push_back(profile);
    }
    return result;
}
inline std::optional<std::string> password(const NetworkSettings& network,const DeviceSecrets& secrets,std::string_view ssid){
    if(ssid==network.ssid&&!ssid.empty())return secrets.wifiPassword;
    const auto found=std::ranges::find(secrets.wifiNetworks,ssid,&WifiProfile::ssid);
    if(found!=secrets.wifiNetworks.end())return found->password;
    return std::nullopt;
}
inline void remember(NetworkSettings& network,DeviceSecrets& secrets,std::string ssid,std::string password){
    if(ssid.empty()||ssid.size()>rules::kWifiSsidMaxLength||password.size()>rules::kWifiPasswordMaxLength)return;
    auto saved=profiles(network,secrets); // Migrate the legacy network before changing it.
    std::erase_if(saved,[&](const auto& item){return item.ssid==ssid;});
    saved.insert(saved.begin(),{ssid,password});
    if(saved.size()>kMaxProfiles)saved.resize(kMaxProfiles);
    network.ssid=std::move(ssid);secrets.wifiPassword=std::move(password);secrets.wifiNetworks=std::move(saved);
}
inline void forgetCurrent(NetworkSettings& network,DeviceSecrets& secrets){
    auto saved=profiles(network,secrets);
    std::erase_if(saved,[&](const auto& item){return item.ssid==network.ssid;});
    network.ssid=saved.empty()?std::string{}:saved.front().ssid;
    secrets.wifiPassword=saved.empty()?std::string{}:saved.front().password;
    secrets.wifiNetworks=std::move(saved);
}
struct Visible {std::string ssid;int rssi;};
inline std::vector<WifiProfile> connectionOrder(std::vector<WifiProfile> saved,const std::vector<Visible>& visible){
    // Prefer visible networks, strongest first; saved order breaks ties. Hidden
    // networks stay as bounded fallbacks. Scan failures cannot erase credentials.
    const auto signal=[&](const auto& profile){int best=-1000;for(const auto& item:visible)if(item.ssid==profile.ssid)best=std::max(best,item.rssi);return best;};
    std::stable_sort(saved.begin(),saved.end(),[&](const auto& a,const auto& b){return signal(a)>signal(b);});
    return saved;
}
}
