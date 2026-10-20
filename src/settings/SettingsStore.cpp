#include "settings/SettingsStore.h"
#include "settings/WifiProfiles.h"
#include <esp_log.h>

#include <Arduino.h>
#include <FS.h>

#include <string>
#include <utility>

#include "hash/Fnv1a.h"
#include "settings/SettingsRules.h"
#include "storage/fs/StoragePaths.h"
#include "text/LocaleTag.h"

namespace settings {
    namespace {

        constexpr char kFileHashKey[] = "file_hash";
        constexpr char kSecretsKey[] = "secrets";
        constexpr uint32_t kPersistenceDelayMs = 1500;

        SettingsError error(SettingsErrorCategory category, SettingsSource source, std::string message,
                            std::string path = {}) {
            return {.category = category, .source = source, .path = std::move(path), .message = std::move(message)};
        }

        SettingsResult<std::string> readBlob(Preferences& preferences, const char* key, size_t maximum,
                                             SettingsSource source) {
            const size_t size = preferences.getBytesLength(key);
            if (size == 0)
                return std::unexpected(error(SettingsErrorCategory::Missing, source, "NVS blob is missing", key));
            if (size > maximum)
                return std::unexpected(error(SettingsErrorCategory::TooLarge, source, "NVS blob is too large", key));

            std::string content(size, '\0');
            if (preferences.getBytes(key, content.data(), content.size()) != content.size())
                return std::unexpected(error(SettingsErrorCategory::Io, source, "NVS blob read was incomplete", key));
            return content;
        }

        SettingsResult<> writeBlob(Preferences& preferences, const char* key, std::string_view content,
                                   SettingsSource source) {
            if (preferences.putBytes(key, content.data(), content.size()) != content.size())
                return std::unexpected(error(SettingsErrorCategory::Io, source, "NVS blob write was incomplete", key));
            return {};
        }

        SettingsResult<std::string> readFile(fs::FS& filesystem) {
            File file = filesystem.open(StoragePaths::kSettingsConfigPath, FILE_READ);
            if (!file)
                return std::unexpected(error(SettingsErrorCategory::Missing, SettingsSource::Sd,
                                             "settings file is missing", StoragePaths::kSettingsConfigPath));
            if (file.isDirectory()) {
                file.close();
                return std::unexpected(error(SettingsErrorCategory::Io, SettingsSource::Sd,
                                             "settings path is a directory", StoragePaths::kSettingsConfigPath));
            }
            const size_t size = file.size();
            if (size > kMaxSettingsBytes) {
                file.close();
                return std::unexpected(error(SettingsErrorCategory::TooLarge, SettingsSource::Sd,
                                             "settings file is too large", StoragePaths::kSettingsConfigPath));
            }
            std::string content(size, '\0');
            const size_t count = file.read(reinterpret_cast<uint8_t*>(content.data()), content.size());
            file.close();
            if (count != content.size())
                return std::unexpected(error(SettingsErrorCategory::Io, SettingsSource::Sd,
                                             "settings file read was incomplete", StoragePaths::kSettingsConfigPath));
            return content;
        }

        uint32_t storedHash(std::string_view content) {
            const uint32_t value = Fnv1a::hash(content);
            return value == 0 ? 1 : value;
        }

        void truncate(std::string& value, size_t maximum) {
            if (value.size() > maximum)
                value.resize(maximum);
        }

        void sanitize(DeviceSettings& value) {
            auto& typography = value.reading.typography;
            if (typography.fontId.empty())
                typography.fontId = TypographySettings{}.fontId;
            truncate(typography.fontId, rules::kFontIdMaxLength);
            if (value.interface.selectedThemeId.empty())
                value.interface.selectedThemeId = "default";
            truncate(value.interface.selectedThemeId, rules::kThemeIdMaxLength);
            if (auto locale = LocaleTag::normalize(value.interface.locale))
                value.interface.locale = std::move(*locale);
            else
                value.interface.locale = "en";
            truncate(value.network.ssid, rules::kWifiSsidMaxLength);
            truncate(value.updates.repositoryOwner, rules::kRepositoryOwnerMaxLength);
            truncate(value.updates.releaseTag, rules::kReleaseTagMaxLength);
        }

        void sanitize(DeviceSecrets& value) {
            wifi::sanitize(value);
        }

    } // namespace

    SettingsStore::~SettingsStore() {
        closeNvs();
    }

    void SettingsStore::closeNvs() {
        if (nvsOpen_)
            preferences_.end();
        nvsOpen_ = false;
    }

    SettingsResult<> SettingsStore::reopenNvsAndPersist() {
        closeNvs();
        nvsOpen_ = preferences_.begin(kSettingsNvsNamespace, false);
        if (!nvsOpen_)
            return std::unexpected(error(SettingsErrorCategory::Io, SettingsSource::Nvs,
                                         "could not reopen settings NVS namespace"));
        dirty_ = true;
        secretsDirty_ = true;
        return flush();
    }

    SettingsResult<> SettingsStore::begin(fs::FS* filesystem) {
        if (nvsOpen_)
            preferences_.end();
        nvsOpen_ = preferences_.begin(kSettingsNvsNamespace, false);
        if (!nvsOpen_)
            return std::unexpected(error(SettingsErrorCategory::Io, SettingsSource::Nvs,
                                         "could not open settings NVS namespace"));

        filesystem_ = filesystem;
        mirrorEnabled_ = filesystem != nullptr;

        auto nvsContent = readBlob(preferences_, kSettingsNvsKey, kMaxSettingsBytes, SettingsSource::Nvs);
        auto nvsSettings = nvsContent.and_then([](const std::string& content) {
            return codec::decodeToml(content, SettingsSource::Nvs);
        });
        auto nvsSecretsContent = readBlob(preferences_, kSecretsKey, kMaxSecretsBytes, SettingsSource::Nvs);
        auto nvsSecrets = nvsSecretsContent.and_then([](const std::string& content) {
            return codec::decodeSecrets(content, SettingsSource::Nvs);
        });

        SettingsResult<std::string> fileContent =
            filesystem
                ? readFile(*filesystem)
                : SettingsResult<std::string>{std::unexpected(error(SettingsErrorCategory::Missing, SettingsSource::Sd,
                                                                    "SD storage is unavailable"))};

        SettingsResult<DeviceSettings> fileSettings = fileContent.and_then([](const std::string& content) {
            return codec::decodeToml(content, SettingsSource::Sd);
        });

        const uint32_t savedHash = preferences_.getUInt(kFileHashKey, 0);
        const bool fileWasEdited = fileContent && (savedHash == 0 || storedHash(*fileContent) != savedHash);
        const bool invalidFile = fileContent && !fileSettings;

        const bool hasNvsSettings = nvsSettings.has_value();
        const bool hasFileSettings = fileSettings.has_value();
        const bool importFile = hasFileSettings && (fileWasEdited || !hasNvsSettings);

        if (importFile) {
            settings_ = *fileSettings;
        } else if (nvsSettings) {
            settings_ = *nvsSettings;
        } else if (fileSettings) {
            settings_ = *fileSettings;
        } else {
            settings_ = DeviceSettings{};
        }

        sanitize(settings_);

        if (nvsSecrets)
            secrets_ = std::move(*nvsSecrets);
        sanitize(secrets_);

        mirrorEnabled_ = filesystem && !invalidFile;
        auto canonical = codec::encodeToml(settings_, SettingsSource::Nvs);
        if (!canonical)
            return std::unexpected(canonical.error());
        auto canonicalSecrets = codec::encodeSecrets(secrets_, SettingsSource::Nvs);
        if (!canonicalSecrets)
            return std::unexpected(canonicalSecrets.error());
        const bool nvsNeedsWrite = !nvsContent || *nvsContent != *canonical;
        const bool fileNeedsWrite = mirrorEnabled_ && (!fileContent || fileWasEdited || *fileContent != *canonical);
        dirty_ = nvsNeedsWrite || fileNeedsWrite;
        secretsDirty_ = !nvsSecretsContent || *nvsSecretsContent != *canonicalSecrets;
        dirtyAtMs_ = millis();

        if (auto result = flush(); !result)
            return result;
        if (invalidFile)
            return std::unexpected(fileSettings.error());
        return {};
    }

    void SettingsStore::acceptChanges() {
        sanitize(settings_);
        dirty_ = true;
        dirtyAtMs_ = millis();
    }

    void SettingsStore::acceptSecretChanges() {
        sanitize(secrets_);
        secretsDirty_ = true;
        dirtyAtMs_ = millis();
    }

    void SettingsStore::update(uint32_t nowMs) {
        if ((dirty_ || secretsDirty_) && nowMs - dirtyAtMs_ >= kPersistenceDelayMs) {
            if (auto result = flush(); !result) {
                // Keep pending changes, but allow the UI to run between retries
                // when an SD card or NVS write fails.
                dirtyAtMs_ = millis();
                ESP_LOGE("settings", "persistence failed: %s", result.error().message.c_str());
            }
        }
    }

    SettingsResult<> SettingsStore::writeSettings(std::string_view canonicalToml) {
        if (auto result = writeBlob(preferences_, kSettingsNvsKey, canonicalToml, SettingsSource::Nvs); !result)
            return result;
        if (!filesystem_ || !mirrorEnabled_)
            return {};
        if (auto result = writeFile(canonicalToml); !result)
            return result;
        if (preferences_.putUInt(kFileHashKey, storedHash(canonicalToml)) == 0)
            return std::unexpected(error(SettingsErrorCategory::Io, SettingsSource::Nvs,
                                         "settings file hash write failed", kFileHashKey));
        return {};
    }

    SettingsResult<> SettingsStore::writeSecrets() {
        return codec::encodeSecrets(secrets_, SettingsSource::Nvs).and_then([this](const std::string& content) {
            return writeBlob(preferences_, kSecretsKey, content, SettingsSource::Nvs);
        });
    }

    SettingsResult<> SettingsStore::flush() {
        SettingsResult<> result;
        if (dirty_) {
            result =
                codec::encodeToml(settings_, SettingsSource::Nvs).and_then([this](const std::string& content) {
                    return writeSettings(content);
                });
            if (result)
                dirty_ = false;
        }
        if (secretsDirty_) {
            // Credentials live in NVS. A failed SD mirror must not prevent
            // them from being saved independently of the public settings.
            auto secretResult = writeSecrets();
            if (secretResult)
                secretsDirty_ = false;
            else if (result)
                result = std::move(secretResult);
        }
        return result;
    }

    SettingsResult<> SettingsStore::writeFile(std::string_view content) {
        File directory = filesystem_->open(StoragePaths::kConfigPath);
        const bool directoryExists = directory && directory.isDirectory();
        if (directory)
            directory.close();
        if (!directoryExists && !filesystem_->mkdir(StoragePaths::kConfigPath))
            return std::unexpected(error(SettingsErrorCategory::Io, SettingsSource::Sd,
                                         "could not create config directory", StoragePaths::kConfigPath));

        filesystem_->remove(StoragePaths::kSettingsConfigTempPath);
        File file = filesystem_->open(StoragePaths::kSettingsConfigTempPath, FILE_WRITE);
        if (!file || file.isDirectory()) {
            if (file)
                file.close();
            return std::unexpected(error(SettingsErrorCategory::Io, SettingsSource::Sd,
                                         "could not open temporary settings file",
                                         StoragePaths::kSettingsConfigTempPath));
        }
        const size_t count = file.write(reinterpret_cast<const uint8_t*>(content.data()), content.size());
        file.flush();
        file.close();
        if (count != content.size()) {
            filesystem_->remove(StoragePaths::kSettingsConfigTempPath);
            return std::unexpected(error(SettingsErrorCategory::Io, SettingsSource::Sd,
                                         "settings file write was incomplete", StoragePaths::kSettingsConfigTempPath));
        }

        filesystem_->remove(StoragePaths::kSettingsConfigBackupPath);
        File current = filesystem_->open(StoragePaths::kSettingsConfigPath, FILE_READ);
        const bool hadCurrent = current && !current.isDirectory();
        if (current)
            current.close();
        if (hadCurrent
            && !filesystem_->rename(StoragePaths::kSettingsConfigPath, StoragePaths::kSettingsConfigBackupPath)) {
            filesystem_->remove(StoragePaths::kSettingsConfigTempPath);
            return std::unexpected(error(SettingsErrorCategory::Io, SettingsSource::Sd,
                                         "could not back up settings file", StoragePaths::kSettingsConfigPath));
        }
        if (!filesystem_->rename(StoragePaths::kSettingsConfigTempPath, StoragePaths::kSettingsConfigPath)) {
            if (hadCurrent)
                filesystem_->rename(StoragePaths::kSettingsConfigBackupPath, StoragePaths::kSettingsConfigPath);
            filesystem_->remove(StoragePaths::kSettingsConfigTempPath);
            return std::unexpected(error(SettingsErrorCategory::Io, SettingsSource::Sd,
                                         "could not replace settings file", StoragePaths::kSettingsConfigPath));
        }
        filesystem_->remove(StoragePaths::kSettingsConfigBackupPath);
        return {};
    }

} // namespace settings
