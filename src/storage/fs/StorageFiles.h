#pragma once

#include <Arduino.h>

namespace StorageFiles {

    void logError(const char* tag, const char* operation, const String& path, int error);
    void
    logError(const char* tag, const char* operation, const String& sourcePath, const String& targetPath, int error);

    bool directoryExists(const char* path);
    bool fileExists(const String& path);
    bool fileExistsWithBytes(const String& path);
    bool ensureDirectory(const char* path, const char* tag = "storage");

} // namespace StorageFiles
