#include "storage/fs/SdDiagnostics.h"

#include <Preferences.h>
#include <SD_MMC.h>
#include <algorithm>
#include <array>
#include <cerrno>
#include <cstring>
#include <driver/sdmmc_types.h>

#include "board/BoardConfig.h"
#include "storage/fs/StorageFiles.h"
#include "storage/fs/StoragePaths.h"

namespace SdDiagnostics {
    namespace {

        constexpr size_t kSdFrequencyCount = 4;
        constexpr std::array<int, kSdFrequencyCount> kSdFrequenciesKhz = {{
            SDMMC_FREQ_HIGHSPEED,
            SDMMC_FREQ_DEFAULT,
            10000, // Some cards are unstable at Arduino's default SDMMC clock.
            SDMMC_FREQ_PROBING,
        }};
        using FrequencyList = std::array<int, kSdFrequencyCount>;
        constexpr uint64_t kBytesPerMegabyte = 1024ULL * 1024ULL;
        constexpr uint64_t kSdxcMinSizeMb = 32ULL * 1024ULL;
        constexpr size_t kFrequencyProbeBytes = 256UL * 1024UL;
        constexpr size_t kFolderProbeBytes = 64UL * 1024UL;
        constexpr size_t kProbeChunkBytes = 4096;
        constexpr const char* kPreferencesNamespace = "sd_diag";
        constexpr const char* kPreferenceFrequencyKhz = "freq_khz";
        constexpr const char* kPreferenceCardType = "card_type";
        constexpr const char* kPreferenceCardSizeMb = "size_mb";
        constexpr const char* kFrequencyProbePath = "/.sdfreq.tmp";
        constexpr const char* kFolderProbeName = ".sdcheck.tmp";
        int sMountedFrequencyKhz = 0;

        struct FrequencyCache {
            int frequencyKhz = 0;
            uint8_t cardType = CARD_NONE;
            uint32_t sizeMb = 0;
            bool valid = false;
        };

        const char* cardTypeLabel(uint8_t cardType, uint64_t sizeMb) {
            switch (cardType) {
            case CARD_MMC:
                return "MMC";
            case CARD_SD:
                return "SDSC";
            case CARD_SDHC:
                return sizeMb > kSdxcMinSizeMb ? "SDXC" : "SDHC";
            default:
                return "Unknown";
            }
        }

        bool ensureLibraryFolderLayout() {
            return StorageFiles::ensureDirectory(StoragePaths::kBooksPath, "sd-check")
                && StorageFiles::ensureDirectory(StoragePaths::kBookFilesPath, "sd-check")
                && StorageFiles::ensureDirectory(StoragePaths::kArticleFilesPath, "sd-check")
                && StorageFiles::ensureDirectory(StoragePaths::kConfigPath, "sd-check");
        }

        bool isSupportedFrequency(int frequencyKhz) {
            return std::find(kSdFrequenciesKhz.begin(), kSdFrequenciesKhz.end(), frequencyKhz)
                != kSdFrequenciesKhz.end();
        }

        uint32_t currentCardSizeMb() {
            return static_cast<uint32_t>(SD_MMC.cardSize() / kBytesPerMegabyte);
        }

        FrequencyCache readFrequencyCache() {
            Preferences preferences;
            if (!preferences.begin(kPreferencesNamespace, true)) {
                Serial.println("[sd-check] frequency cache unavailable");
                return {};
            }
            FrequencyCache cache;
            cache.frequencyKhz = preferences.getInt(kPreferenceFrequencyKhz, 0);
            cache.cardType = preferences.getUChar(kPreferenceCardType, CARD_NONE);
            cache.sizeMb = preferences.getUInt(kPreferenceCardSizeMb, 0);
            preferences.end();
            if (!isSupportedFrequency(cache.frequencyKhz) || cache.cardType == CARD_NONE || cache.sizeMb == 0) {
                if (cache.frequencyKhz != 0) {
                    Serial.printf("[sd-check] ignoring incomplete cached frequency %d kHz\n", cache.frequencyKhz);
                }
                return {};
            }
            cache.valid = true;
            return cache;
        }

        bool cacheMatchesMountedCard(const FrequencyCache& cache) {
            return cache.valid && cache.cardType == SD_MMC.cardType() && cache.sizeMb == currentCardSizeMb();
        }

        void writeFrequencyCache(int frequencyKhz) {
            if (!isSupportedFrequency(frequencyKhz)) {
                return;
            }

            const uint8_t cardType = SD_MMC.cardType();
            const uint32_t sizeMb = currentCardSizeMb();
            const FrequencyCache cache = readFrequencyCache();
            if (cache.valid && cache.frequencyKhz == frequencyKhz && cache.cardType == cardType
                && cache.sizeMb == sizeMb) {
                return;
            }

            Preferences preferences;
            if (!preferences.begin(kPreferencesNamespace, false)) {
                Serial.println("[sd-check] frequency cache write unavailable");
                return;
            }
            preferences.putInt(kPreferenceFrequencyKhz, frequencyKhz);
            preferences.putUChar(kPreferenceCardType, cardType);
            preferences.putUInt(kPreferenceCardSizeMb, sizeMb);
            preferences.end();
        }

        size_t buildFrequencyProbeOrder(FrequencyList& frequencies) {
            size_t count = 0;

            for (int candidate: kSdFrequenciesKhz) {
                const bool alreadyQueued = std::find(frequencies.begin(), frequencies.begin() + count, candidate)
                                        != frequencies.begin() + count;
                if (!alreadyQueued && count < frequencies.size()) {
                    frequencies[count++] = candidate;
                }
            }
            return count;
        }

        void fillProbeBuffer(uint8_t* buffer, size_t bytes, uint32_t offset) {
            for (size_t i = 0; i < bytes; ++i) {
                const uint32_t value = offset + static_cast<uint32_t>(i);
                buffer[i] = static_cast<uint8_t>((value * 33U) ^ (value >> 3) ^ 0xA5U);
            }
        }

        bool removeProbeFile(const String& path, const char* tag) {
            errno = 0;
            const bool removed = SD_MMC.remove(path);
            const int removeErrno = errno;
            if (!removed && StorageFiles::fileExists(path)) {
                StorageFiles::logError(tag, "remove probe", path, removeErrno);
                return false;
            }
            return true;
        }

        bool writeReadProbeFile(const String& path, size_t bytes, const char* tag) {
            Serial.printf("[%s] write/read probe path=%s bytes=%u\n", tag, path.c_str(),
                          static_cast<unsigned int>(bytes));
            SD_MMC.remove(path);

            static uint8_t writeBuffer[kProbeChunkBytes];
            static uint8_t readBuffer[kProbeChunkBytes];

            {
                // Write the deterministic probe payload.
                errno = 0;
                File file = SD_MMC.open(path, FILE_WRITE);
                const int openErrno = errno;
                if (!file) {
                    StorageFiles::logError(tag, "open FILE_WRITE", path, openErrno);
                    return false;
                }

                size_t writtenTotal = 0;
                while (writtenTotal < bytes) {
                    const size_t chunk = std::min(kProbeChunkBytes, bytes - writtenTotal);
                    fillProbeBuffer(writeBuffer, chunk, static_cast<uint32_t>(writtenTotal));
                    const size_t written = file.write(writeBuffer, chunk);
                    if (written != chunk) {
                        Serial.printf("[%s] probe short write path=%s offset=%u wanted=%u got=%u\n", tag, path.c_str(),
                                      static_cast<unsigned int>(writtenTotal), static_cast<unsigned int>(chunk),
                                      static_cast<unsigned int>(written));
                        file.close();
                        removeProbeFile(path, tag);
                        return false;
                    }
                    writtenTotal += written;
                    yield();
                    delay(0);
                }
                file.close();
            }

            {
                // Reopen and verify the exact bytes to catch flaky card timings.
                File file = SD_MMC.open(path, FILE_READ);
                if (!file || file.isDirectory()) {
                    if (file) {
                        file.close();
                    }
                    Serial.printf("[%s] probe reopen failed path=%s\n", tag, path.c_str());
                    removeProbeFile(path, tag);
                    return false;
                }

                if (file.size() != bytes) {
                    Serial.printf("[%s] probe size mismatch path=%s size=%u expected=%u\n", tag, path.c_str(),
                                  static_cast<unsigned int>(file.size()), static_cast<unsigned int>(bytes));
                    file.close();
                    removeProbeFile(path, tag);
                    return false;
                }

                size_t readTotal = 0;
                while (readTotal < bytes) {
                    const size_t chunk = std::min(kProbeChunkBytes, bytes - readTotal);
                    fillProbeBuffer(writeBuffer, chunk, static_cast<uint32_t>(readTotal));
                    const size_t read = file.read(readBuffer, chunk);
                    if (read != chunk || std::memcmp(readBuffer, writeBuffer, chunk) != 0) {
                        Serial.printf("[%s] probe verify failed path=%s offset=%u wanted=%u got=%u\n", tag,
                                      path.c_str(), static_cast<unsigned int>(readTotal),
                                      static_cast<unsigned int>(chunk), static_cast<unsigned int>(read));
                        file.close();
                        removeProbeFile(path, tag);
                        return false;
                    }
                    readTotal += read;
                    yield();
                    delay(0);
                }

                file.close();
            }

            return removeProbeFile(path, tag);
        }

        String probePathForDirectory(const char* directoryPath, const char* name) {
            String path = String(directoryPath);
            if (!path.endsWith("/")) {
                path += "/";
            }
            path += name;
            return path;
        }

        bool tryMountFrequency(bool& mounted, int frequencyKhz) {
            Serial.printf("[sd-check] trying mount at %d kHz\n", frequencyKhz);
            SD_MMC.end();
            mounted = SD_MMC.begin(StoragePaths::kMountPoint, true, false, frequencyKhz, 5);
            if (!mounted) {
                return false;
            }
            if (!writeReadProbeFile(kFrequencyProbePath, kFrequencyProbeBytes, "sd-probe")) {
                Serial.printf("[sd-check] frequency %d kHz failed sustained probe\n", frequencyKhz);
                SD_MMC.end();
                mounted = false;
                return false;
            }
            return true;
        }

        void recordMountedFrequency(int frequencyKhz, int* mountedFrequencyKhz) {
            sMountedFrequencyKhz = frequencyKhz;
            if (mountedFrequencyKhz != nullptr) {
                *mountedFrequencyKhz = frequencyKhz;
            }
        }

        void unmountCard(bool& mounted) {
            SD_MMC.end();
            mounted = false;
            sMountedFrequencyKhz = 0;
        }

    } // namespace

    bool mountCardWithCache(bool& mounted, int* mountedFrequencyKhz, bool allowCache) {
        if (mounted) {
            if (mountedFrequencyKhz != nullptr) {
                *mountedFrequencyKhz = sMountedFrequencyKhz;
            }
            return true;
        }

        if (!SD_MMC.setPins(Board::Config::PIN_SD_CLK, Board::Config::PIN_SD_CMD, Board::Config::PIN_SD_D0)) {
            Serial.println("[sd-check] SD_MMC pin setup failed");
            return false;
        }

        FrequencyList frequencyOrder;
        const size_t frequencyCount = buildFrequencyProbeOrder(frequencyOrder);

        const FrequencyCache cache = allowCache ? readFrequencyCache() : FrequencyCache{};
        if (allowCache && cache.valid) {
            Serial.printf("[sd-check] trying cached SD frequency %d kHz\n", cache.frequencyKhz);
            if (tryMountFrequency(mounted, cache.frequencyKhz)) {
                if (cacheMatchesMountedCard(cache)) {
                    recordMountedFrequency(cache.frequencyKhz, mountedFrequencyKhz);
                    Serial.printf("[sd-check] selected cached SD frequency %d kHz\n", cache.frequencyKhz);
                    return true;
                }

                Serial.println("[sd-check] cached SD frequency belongs to a different card; rediscovering");
                unmountCard(mounted);
            }
        }

        for (size_t i = 0; i < frequencyCount; ++i) {
            const int frequencyKhz = frequencyOrder[i];
            if (tryMountFrequency(mounted, frequencyKhz)) {
                recordMountedFrequency(frequencyKhz, mountedFrequencyKhz);
                Serial.printf("[sd-check] selected SD frequency %d kHz\n", frequencyKhz);
                writeFrequencyCache(frequencyKhz);
                return true;
            }
        }
        sMountedFrequencyKhz = 0;
        return false;
    }

    bool mountCard(bool& mounted, int* mountedFrequencyKhz) {
        return mountCardWithCache(mounted, mountedFrequencyKhz, true);
    }

    bool validateMountedFrequency(bool& mounted, int* mountedFrequencyKhz) {
        if (!mounted) {
            return mountCard(mounted, mountedFrequencyKhz);
        }
        if (!isSupportedFrequency(sMountedFrequencyKhz)) {
            Serial.println("[sd-check] mounted frequency unknown; rediscovering");
            unmountCard(mounted);
            return mountCardWithCache(mounted, mountedFrequencyKhz, false);
        }

        Serial.printf("[sd-check] validating mounted frequency %d kHz\n", sMountedFrequencyKhz);
        if (writeReadProbeFile(kFrequencyProbePath, kFrequencyProbeBytes, "sd-probe")) {
            if (mountedFrequencyKhz != nullptr) {
                *mountedFrequencyKhz = sMountedFrequencyKhz;
            }
            return true;
        }

        Serial.printf("[sd-check] mounted frequency %d kHz failed; rediscovering\n", sMountedFrequencyKhz);
        unmountCard(mounted);
        return mountCardWithCache(mounted, mountedFrequencyKhz, false);
    }

    bool verifyWritableFolder(const char* directoryPath) {
        if (!StorageFiles::directoryExists(directoryPath)) {
            Serial.printf("[sd-check] write probe skipped, not a directory: %s\n", directoryPath);
            return false;
        }

        return writeReadProbeFile(probePathForDirectory(directoryPath, kFolderProbeName), kFolderProbeBytes,
                                  "sd-check");
    }

    DiagnosticResult diagnoseCard(bool& mounted, StatusCallback statusCallback, void* statusContext) {
        DiagnosticResult result;
        auto report = [&](const char* line1, const char* line2 = "", int progressPercent = -1) {
            if (statusCallback != nullptr) {
                statusCallback(statusContext, "SD check", line1, line2, progressPercent);
            }
        };
        report("Mounting card", "", 5);

        // Validate the selected SD clock before checking the filesystem contract.
        if (!validateMountedFrequency(mounted, &result.frequencyKhz)) {
            result.summary = "Card not mounted";
            result.detail = "Format FAT32 MBR";
        }

        result.mounted = mounted;
        if (!mounted) {
            result.summary = "Card not mounted";
            result.detail = "Format FAT32 MBR";
            Serial.println("[sd-check] mount failed; likely format/partition issue, "
                           "seating, or card fault");
            return result;
        }

        result.sizeMb = SD_MMC.cardSize() / kBytesPerMegabyte;
        result.cardType = cardTypeLabel(SD_MMC.cardType(), result.sizeMb);
        result.frequencyKhz = sMountedFrequencyKhz;
        Serial.printf("[sd-check] mounted type=%s size=%llu MB freq=%d kHz\n", result.cardType.c_str(), result.sizeMb,
                      result.frequencyKhz);

        report("Checking folders", "", 30);
        result.booksDirectory = StorageFiles::directoryExists(StoragePaths::kBooksPath);
        result.bookFilesDirectory = StorageFiles::directoryExists(StoragePaths::kBookFilesPath);
        result.articleFilesDirectory = StorageFiles::directoryExists(StoragePaths::kArticleFilesPath);
        result.configDirectory = StorageFiles::directoryExists(StoragePaths::kConfigPath);
        if (!result.booksDirectory || !result.bookFilesDirectory || !result.articleFilesDirectory
            || !result.configDirectory) {
            result.summary = "Folders missing";
            result.detail = "Can create layout";
            Serial.printf("[sd-check] v0.0.4 folders missing /books=%u /books/books=%u "
                          "/books/articles=%u /config=%u\n",
                          result.booksDirectory ? 1 : 0, result.bookFilesDirectory ? 1 : 0,
                          result.articleFilesDirectory ? 1 : 0, result.configDirectory ? 1 : 0);
            report("Folders missing", "Confirm repair", 38);
            return result;
        }

        result.summary = "Storage OK";
        result.detail = result.cardType + " " + String(static_cast<unsigned int>(result.sizeMb)) + " MB "
                      + String(result.frequencyKhz / 1000) + " MHz";
        return result;
    }

    void probeWritableFolders(DiagnosticResult& result, StatusCallback statusCallback, void* statusContext) {
        if (statusCallback != nullptr) {
            statusCallback(statusContext, "SD check", "Testing write", "", 70);
        }
        // Probe each required folder with the same sustained write/read check.
        result.writable = verifyWritableFolder(StoragePaths::kBooksPath);
        result.booksWritable = verifyWritableFolder(StoragePaths::kBookFilesPath);
        result.articlesWritable = verifyWritableFolder(StoragePaths::kArticleFilesPath);
        result.configWritable = verifyWritableFolder(StoragePaths::kConfigPath);
        if (!result.writable) {
            result.summary = "Write test failed";
            result.detail = "Format FAT32 MBR";
            Serial.println("[sd-check] /books write/delete probe failed");
            return;
        }
        if (!result.booksWritable || !result.articlesWritable || !result.configWritable) {
            result.summary = "Folder write failed";
            result.detail = "Format FAT32 MBR";
            Serial.printf("[sd-check] folder write failed books=%u articles=%u config=%u\n",
                          result.booksWritable ? 1 : 0, result.articlesWritable ? 1 : 0, result.configWritable ? 1 : 0);
            return;
        }

        result.summary = "Storage OK";
        result.detail = result.cardType + " " + String(static_cast<unsigned int>(result.sizeMb)) + " MB "
                      + String(result.frequencyKhz / 1000) + " MHz";
    }

    bool repairFolderLayout(bool mounted) {
        if (!mounted) {
            Serial.println("[sd-check] folder repair skipped: card not mounted");
            return false;
        }

        Serial.println("[sd-check] repairing v0.0.4 folder layout");
        const bool rootWritable = verifyWritableFolder("/");
        Serial.printf("[sd-check] root write probe=%u\n", rootWritable ? 1 : 0);

        const bool foldersOk = ensureLibraryFolderLayout();
        const bool booksOk = StorageFiles::directoryExists(StoragePaths::kBooksPath);
        const bool bookFilesOk = StorageFiles::directoryExists(StoragePaths::kBookFilesPath);
        const bool articleFilesOk = StorageFiles::directoryExists(StoragePaths::kArticleFilesPath);
        const bool configOk = StorageFiles::directoryExists(StoragePaths::kConfigPath);
        const bool ok = rootWritable && foldersOk;
        if (ok) {
            Serial.println("[sd-check] repaired v0.0.4 folder layout");
        } else {
            Serial.printf("[sd-check] folder repair failed rootWritable=%u /books=%u "
                          "/books/books=%u "
                          "/books/articles=%u /config=%u\n",
                          rootWritable ? 1 : 0, booksOk ? 1 : 0, bookFilesOk ? 1 : 0, articleFilesOk ? 1 : 0,
                          configOk ? 1 : 0);
        }
        return ok;
    }

} // namespace SdDiagnostics
