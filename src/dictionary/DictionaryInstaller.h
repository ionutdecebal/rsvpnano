#pragma once
#include "network/VerifiedDownload.h"
#include "settings/SettingsModel.h"
#include <expected>
#include <string>

namespace dictionary {
inline constexpr size_t kEnglishBytes = 50137998;
inline constexpr char kEnglishSha256[] = "9f893776ba6b9b5f93abae7c06cdc5ff90ebf7cb4427d343160f26b1488ad9c6";
using InstallStatus = std::function<void(const char*, const char*, int)>;
bool installedPack();
bool recoverInstall();
std::expected<void, std::string> installEnglish(const settings::DeviceSettings& settings,
    const settings::DeviceSecrets& secrets, bool replace, const InstallStatus& status,
    const std::function<bool()>& cancelled);
}
