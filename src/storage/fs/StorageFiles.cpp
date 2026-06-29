#include "storage/fs/StorageFiles.h"

#include "board/BoardStorage.h"
#include <cerrno>
#include <cstring>

namespace StorageFiles {
    namespace {

        void logErrorMessage(const char* tag, const char* operation, const String& target, int error) {
            if (error != 0) {
                Serial.printf("[%s] %s failed %s errno=%d (%s)\n",
                              tag,
                              operation,
                              target.c_str(),
                              error,
                              std::strerror(error));
            } else {
                Serial.printf("[%s] %s failed %s errno=0\n", tag, operation, target.c_str());
            }
        }

    } // namespace

    void logError(const char* tag, const char* operation, const String& path, int error) {
        logErrorMessage(tag, operation, String("path=") + path, error);
    }

    void
    logError(const char* tag, const char* operation, const String& sourcePath, const String& targetPath, int error) {
        logErrorMessage(tag, operation, String("from=") + sourcePath + " to=" + targetPath, error);
    }

    bool directoryExists(const char* path) {
        File dir = Board::Storage::filesystem().open(path);
        const bool exists = dir && dir.isDirectory();
        if (dir) {
            dir.close();
        }
        return exists;
    }

    bool fileExists(const String& path) {
        File file = Board::Storage::filesystem().open(path);
        const bool exists = file && !file.isDirectory();
        if (file) {
            file.close();
        }
        return exists;
    }

    bool fileExistsWithBytes(const String& path) {
        File file = Board::Storage::filesystem().open(path);
        const bool exists = file && !file.isDirectory() && file.size() > 0;
        if (file) {
            file.close();
        }
        return exists;
    }

    bool ensureDirectory(const char* path, const char* tag) {
        if (directoryExists(path)) {
            Serial.printf("[%s] directory exists: %s\n", tag, path);
            return true;
        }
        if (fileExists(String(path))) {
            Serial.printf("[%s] path is a file, not a directory: %s\n", tag, path);
            return false;
        }

        Serial.printf("[%s] creating directory: %s\n", tag, path);
        errno = 0;
        const bool mkdirOk = Board::Storage::filesystem().mkdir(path);
        const int mkdirErrno = errno;
        const bool existsAfter = directoryExists(path);
        Serial.printf("[%s] mkdir path=%s ok=%u existsAfter=%u\n", tag, path, mkdirOk ? 1 : 0, existsAfter ? 1 : 0);
        if (!mkdirOk && !existsAfter) {
            logError(tag, "mkdir", String(path), mkdirErrno);
        }
        return mkdirOk || existsAfter;
    }

} // namespace StorageFiles
