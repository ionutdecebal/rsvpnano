#pragma once

#include <string>
#include <string_view>
#include "settings/SettingsModel.h"
#include "update/UpdateOffer.h"

namespace OtaUpdater {

    using StatusCallback = void (*)(void* context, const char* title, const char* line1, const char* line2,
                                    int progressPercent);

    struct Result {
        std::string summary;
        std::string detail;
        bool rebootRequired = false;
    };

    std::string_view currentVersion();
    Result checkOnly(const settings::DeviceSettings& settings, const settings::DeviceSecrets& secrets,
                     StatusCallback callback = nullptr, void* context = nullptr,
                     Target target = Target::Current, Offer* offer = nullptr);
    Result checkAndInstall(const settings::DeviceSettings& settings, const settings::DeviceSecrets& secrets,
                           StatusCallback callback = nullptr, void* context = nullptr,
                           const Offer* offer = nullptr);

} // namespace OtaUpdater
