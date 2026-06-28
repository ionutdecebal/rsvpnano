#pragma once

#include <Arduino.h>

namespace StoragePaths {

    constexpr const char* kMountPoint = "/sdcard";
    constexpr const char* kBooksPath = "/books";
    constexpr const char* kBookFilesPath = "/books/books";
    constexpr const char* kArticleFilesPath = "/books/articles";
    constexpr const char* kConfigPath = "/config";
    constexpr const char* kTextExtension = ".txt";
    constexpr const char* kRsvpExtension = ".rsvp";
    constexpr const char* kEpubExtension = ".epub";
    constexpr const char* kIndexExtension = ".ridx";
    constexpr const char* kDataExtension = ".rdat";
    constexpr const char* kProgressExtension = ".rpos";
    constexpr const char* kTempExtension = ".tmp";
    constexpr const char* kFailedExtension = ".failed";
    constexpr const char* kConvertingExtension = ".converting";

    bool hasTextExtension(const String& path);
    bool hasRsvpExtension(const String& path);
    bool hasEpubExtension(const String& path);
    String parentDirectoryForPath(const String& path);
    String siblingPathWithExtension(const String& path, const char* extension);
    String epubSiblingPathForRsvp(const String& rsvpPath);
    String displayNameForPath(const String& path);
    String displayNameWithoutExtension(const String& path);
    String rsvpCachePathForEpub(const String& epubPath);
    String indexedIndexPathFor(const String& path);
    String indexedDataPathFor(const String& path);
    String progressSidecarPathFor(const String& path);
    String indexedTempPathFor(const String& path);
    bool isHiddenOrSidecarPath(const String& path);

} // namespace StoragePaths
