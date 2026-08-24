#pragma once

#include <string>
#include <string_view>

namespace StoragePaths {

    constexpr const char* kMountPoint = "/sdcard";
    constexpr const char* kLibraryPath = "/library";
    constexpr const char* kBookFilesPath = "/library/books";
    constexpr const char* kArticleFilesPath = "/library/articles";
    constexpr const char* kArticleFilesPrefix = "/library/articles/";
    constexpr const char* kConfigPath = "/config";
    constexpr const char* kSettingsConfigPath = "/config/settings.toml";
    constexpr const char* kSettingsConfigTempPath = "/config/settings.toml.tmp";
    constexpr const char* kSettingsConfigBackupPath = "/config/settings.toml.bak";
    constexpr const char* kRssConfigPath = "/config/rss.toml";
    constexpr const char* kRssConfigTempPath = "/config/rss.toml.tmp";
    constexpr const char* kRssConfigBackupPath = "/config/rss.toml.bak";
    constexpr const char* kFocusConfigPath = "/config/focus.toml";
    constexpr const char* kFocusConfigTempPath = "/config/focus.toml.tmp";
    constexpr const char* kFocusConfigBackupPath = "/config/focus.toml.bak";
    constexpr const char* kSdFrequencyProbePath = "/.sdfreq.tmp";
    constexpr const char* kThemesPath = "/themes";
    constexpr const char* kFontsPath = "/fonts";
    constexpr const char* kLocalesPath = "/locales";
    constexpr const char* kTextExtension = ".txt";
    constexpr const char* kRsvpExtension = ".rsvp";
    constexpr const char* kEpubExtension = ".epub";
    constexpr const char* kIndexExtension = ".ridx";
    constexpr const char* kDataExtension = ".rdat";
    constexpr const char* kBookStateExtension = ".rstate.toml";
    constexpr const char* kTempExtension = ".tmp";
    constexpr const char* kFailedExtension = ".failed";
    constexpr const char* kConvertingExtension = ".converting";

    bool hasTextExtension(std::string_view path);
    bool hasRsvpExtension(std::string_view path);
    bool hasEpubExtension(std::string_view path);
    std::string sanitizeFilename(std::string_view name);
    std::string parentDirectoryForPath(std::string_view path);
    std::string siblingPathWithExtension(std::string_view path, std::string_view extension);
    std::string epubSiblingPathForRsvp(std::string_view rsvpPath);
    std::string displayNameForPath(std::string_view path);
    std::string displayNameWithoutExtension(std::string_view path);
    std::string rsvpCachePathForEpub(std::string_view epubPath);
    std::string indexedIndexPathFor(std::string_view path);
    std::string indexedDataPathFor(std::string_view path);
    std::string bookStatePathFor(std::string_view path);
    std::string indexedTempPathFor(std::string_view path);
    bool isHiddenOrSidecarPath(std::string_view path);

} // namespace StoragePaths
