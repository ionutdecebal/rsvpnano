#pragma once

#include <glaze/core/context.hpp>

#include <cstdint>
#include <expected>
#include <string>
#include <string_view>

#include "settings/SettingsModel.h"

namespace settings {

    inline constexpr size_t kMaxSettingsBytes = 8192;
    inline constexpr size_t kMaxSecretsBytes = 4096;

    enum class SettingsSource : uint8_t {
        Nvs,
        Sd,
        Companion,
        Programmatic,
        Theme,
    };

    enum class SettingsErrorCategory : uint8_t {
        Missing,
        Io,
        TooLarge,
        Syntax,
        InvalidEnum,
        Constraint,
        Contextual,
    };

    struct SettingsError {
        SettingsErrorCategory category = SettingsErrorCategory::Syntax;
        SettingsSource source = SettingsSource::Programmatic;
        std::string path;
        std::string message;
        glz::error_code glazeCode = glz::error_code::none;
    };

    template<typename T = void>
    using SettingsResult = std::expected<T, SettingsError>;

    namespace codec {

        SettingsResult<DeviceSettings> decodeToml(std::string_view input, SettingsSource source);
        SettingsResult<std::string> encodeToml(const DeviceSettings& value, SettingsSource source);
        SettingsResult<DeviceSecrets> decodeSecrets(std::string_view input, SettingsSource source);
        SettingsResult<std::string> encodeSecrets(const DeviceSecrets& value, SettingsSource source);

    } // namespace codec

} // namespace settings
