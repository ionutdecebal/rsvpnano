#include "storage/index/ReadingProgress.h"

#include <FS.h>
#include <algorithm>
#include <cstdio>

#include "board/BoardStorage.h"
#include "storage/fs/StoragePaths.h"

namespace ReadingProgress {
    namespace {

        constexpr const char* kMagic = "rpos";
        constexpr uint32_t kVersion = 1;

        bool isValidIdentity(const BookIdentity& identity) {
            return identity.sourceSize > 0 && identity.wordCount > 0;
        }

    } // namespace

    bool readPositionSidecar(const String& bookPath, const BookIdentity& identity, uint32_t& wordIndex) {
        wordIndex = 0;
        if (bookPath.isEmpty() || !isValidIdentity(identity)) {
            return false;
        }

        File sidecar = Board::Storage::filesystem().open(StoragePaths::progressSidecarPathFor(bookPath), FILE_READ);
        if (!sidecar || sidecar.isDirectory()) {
            if (sidecar) {
                sidecar.close();
            }
            return false;
        }

        const String line = sidecar.readStringUntil('\n');
        sidecar.close();

        char magic[8] = {};
        unsigned long version = 0;
        unsigned long sourceSize = 0;
        unsigned long sourceFingerprint = 0;
        unsigned long wordCount = 0;
        unsigned long savedWordIndex = 0;
        const int parsed = std::sscanf(line.c_str(), "%7s %lu %lu %lu %lu %lu", magic, &version, &sourceSize,
                                       &sourceFingerprint, &wordCount, &savedWordIndex);

        if (parsed != 6 || String(magic) != kMagic || version != kVersion
            || sourceSize != identity.sourceSize || sourceFingerprint != identity.sourceFingerprint
            || wordCount != identity.wordCount) {
            Serial.printf("[storage-progress] ignored stale progress sidecar: %s\n", bookPath.c_str());
            return false;
        }

        wordIndex = std::min<uint32_t>(static_cast<uint32_t>(savedWordIndex), identity.wordCount - 1);
        return true;
    }

    bool writePositionSidecar(const String& bookPath, const BookIdentity& identity, uint32_t wordIndex) {
        if (bookPath.isEmpty() || !isValidIdentity(identity)) {
            return false;
        }

        const String sidecarPath = StoragePaths::progressSidecarPathFor(bookPath);
        File sidecar = Board::Storage::filesystem().open(sidecarPath, FILE_WRITE);
        if (!sidecar || sidecar.isDirectory()) {
            if (sidecar) {
                sidecar.close();
            }
            Serial.printf("[storage-progress] progress sidecar open failed: %s\n", sidecarPath.c_str());
            return false;
        }

        wordIndex = std::min<uint32_t>(wordIndex, identity.wordCount - 1);
        const size_t written = sidecar.printf("%s %lu %lu %lu %lu %lu\n",
                                             kMagic,
                                             static_cast<unsigned long>(kVersion),
                                             static_cast<unsigned long>(identity.sourceSize),
                                             static_cast<unsigned long>(identity.sourceFingerprint),
                                             static_cast<unsigned long>(identity.wordCount),
                                             static_cast<unsigned long>(wordIndex));
        sidecar.close();

        if (written == 0) {
            Serial.printf("[storage-progress] progress sidecar write failed: %s\n", sidecarPath.c_str());
            return false;
        }

        Serial.printf("[storage-progress] mirrored position word=%u count=%u sidecar=%s\n",
                      static_cast<unsigned int>(wordIndex), static_cast<unsigned int>(identity.wordCount),
                      sidecarPath.c_str());
        return true;
    }

} // namespace ReadingProgress
