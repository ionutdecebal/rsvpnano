#include "storage/fs/SdCard.h"
#include <esp_log.h>
#include "logging/Logger.h"

#include <Preferences.h>
#include <algorithm>
#include <array>
#include <cerrno>
#include <cstring>
#include <driver/sdmmc_types.h>
#include <string>
#include <string_view>
#include <system_error>

#include "board/BoardStorage.h"
#include "storage/fs/StorageFiles.h"
#include "storage/fs/StoragePaths.h"

namespace SdCard {
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
        constexpr size_t kFrequencyProbeBytes = 256UL * 1024UL;
        constexpr size_t kProbeChunkBytes = 4096;
        constexpr const char* kPreferencesNamespace = "sd_diag";
        constexpr const char* kPreferenceFrequencyKhz = "freq_khz";
        constexpr const char* kPreferenceCardType = "card_type";
        constexpr const char* kPreferenceCardSizeMb = "size_mb";
        int sMountedFrequencyKhz = 0;

        struct FrequencyCache {
            int frequencyKhz = 0;
            Board::Storage::CardType cardType = Board::Storage::CardType::None;
            uint32_t sizeMb = 0;
            bool valid = false;
        };

        Board::Storage::CardType cardTypeFromByte(uint8_t value) {
            switch (static_cast<Board::Storage::CardType>(value)) {
            case Board::Storage::CardType::Mmc:
            case Board::Storage::CardType::Sd:
            case Board::Storage::CardType::Sdhc:
                return static_cast<Board::Storage::CardType>(value);
            case Board::Storage::CardType::None:
            default:
                return Board::Storage::CardType::None;
            }
        }

        uint8_t cardTypeByte(Board::Storage::CardType cardType) {
            return static_cast<uint8_t>(cardType);
        }

        bool isSupportedFrequency(int frequencyKhz) {
            return std::ranges::find(kSdFrequenciesKhz, frequencyKhz) != kSdFrequenciesKhz.end();
        }

        uint32_t currentCardSizeMb() {
            return static_cast<uint32_t>(Board::Storage::cardSize() / kBytesPerMegabyte);
        }

        FrequencyCache readFrequencyCache() {
            Preferences preferences;
            if (!preferences.begin(kPreferencesNamespace, true)) {
                ESP_LOGW("sd-check", "frequency cache unavailable");
                return FrequencyCache{};
            }
            FrequencyCache cache;
            cache.frequencyKhz = preferences.getInt(kPreferenceFrequencyKhz, 0);
            cache.cardType = cardTypeFromByte(preferences.getUChar(kPreferenceCardType,
                                                                   cardTypeByte(Board::Storage::CardType::None)));
            cache.sizeMb = preferences.getUInt(kPreferenceCardSizeMb, 0);
            preferences.end();
            if (!isSupportedFrequency(cache.frequencyKhz) || cache.cardType == Board::Storage::CardType::None
                || cache.sizeMb == 0) {
                if (cache.frequencyKhz != 0) {
                    ESP_LOGD("sd-check", "ignoring incomplete cached frequency %d kHz", cache.frequencyKhz);
                }
                return FrequencyCache{};
            }
            cache.valid = true;
            return cache;
        }

        bool cacheMatchesMountedCard(const FrequencyCache& cache) {
            return cache.valid && cache.cardType == Board::Storage::cardType() && cache.sizeMb == currentCardSizeMb();
        }

        void writeFrequencyCache(int frequencyKhz) {
            if (!isSupportedFrequency(frequencyKhz)) {
                return;
            }

            const Board::Storage::CardType cardType = Board::Storage::cardType();
            const uint32_t sizeMb = currentCardSizeMb();
            const FrequencyCache cache = readFrequencyCache();
            if (cache.valid && cache.frequencyKhz == frequencyKhz && cache.cardType == cardType
                && cache.sizeMb == sizeMb) {
                return;
            }

            Preferences preferences;
            if (!preferences.begin(kPreferencesNamespace)) {
                ESP_LOGW("sd-check", "frequency cache write unavailable");
                return;
            }
            preferences.putInt(kPreferenceFrequencyKhz, frequencyKhz);
            preferences.putUChar(kPreferenceCardType, cardTypeByte(cardType));
            preferences.putUInt(kPreferenceCardSizeMb, sizeMb);
            preferences.end();
        }

        size_t buildFrequencyProbeOrder(FrequencyList& frequencies) {
            size_t count = 0;

            for (int candidate: kSdFrequenciesKhz) {
                const bool alreadyQueued =
                    std::ranges::find(frequencies.begin(), frequencies.begin() + count, candidate)
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

        bool probeBufferMatches(const uint8_t* buffer, size_t bytes, uint32_t offset) {
            for (size_t i = 0; i < bytes; ++i) {
                const uint32_t value = offset + static_cast<uint32_t>(i);
                if (buffer[i] != static_cast<uint8_t>((value * 33U) ^ (value >> 3) ^ 0xA5U))
                    return false;
            }
            return true;
        }

        bool removeProbeFile(std::string_view path, const char* tag) {
            const std::string ownedPath{path};
            errno = 0;
            const bool removed = Board::Storage::filesystem().remove(ownedPath.c_str());
            const int removeErrno = errno;
            if (!removed && StorageFiles::fileExists(ownedPath.c_str())) {
                Logger::failure(tag, "remove probe", ownedPath.c_str(),
                                std::error_code{removeErrno, std::generic_category()});
                return false;
            }
            return true;
        }

    } // namespace

    bool probe(std::string_view path, size_t bytes, const char* tag) {
        const std::string ownedPath{path};
        ESP_LOGD(tag, "write/read probe path=%s bytes=%u", ownedPath.c_str(), static_cast<unsigned int>(bytes));
        Board::Storage::filesystem().remove(ownedPath.c_str());

        static uint8_t buffer[kProbeChunkBytes];

        {
            // Write the deterministic probe payload.
            errno = 0;
            File file = Board::Storage::filesystem().open(ownedPath.c_str(), FILE_WRITE);
            const int openErrno = errno;
            if (!file) {
                Logger::failure(tag, "open FILE_WRITE", ownedPath.c_str(),
                                std::error_code{openErrno, std::generic_category()});
                return false;
            }

            size_t writtenTotal = 0;
            while (writtenTotal < bytes) {
                const size_t chunk = std::min(kProbeChunkBytes, bytes - writtenTotal);
                fillProbeBuffer(buffer, chunk, static_cast<uint32_t>(writtenTotal));
                const size_t written = file.write(buffer, chunk);
                if (written != chunk) {
                    ESP_LOGE(tag, "probe short write path=%s offset=%u wanted=%u got=%u", ownedPath.c_str(),
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
            File file = Board::Storage::filesystem().open(ownedPath.c_str(), FILE_READ);
            if (!file || file.isDirectory()) {
                if (file) {
                    file.close();
                }
                ESP_LOGE(tag, "probe reopen failed path=%s", ownedPath.c_str());
                removeProbeFile(path, tag);
                return false;
            }

            if (file.size() != bytes) {
                ESP_LOGE(tag, "probe size mismatch path=%s size=%u expected=%u", ownedPath.c_str(),
                         static_cast<unsigned int>(file.size()), static_cast<unsigned int>(bytes));
                file.close();
                removeProbeFile(path, tag);
                return false;
            }

            size_t readTotal = 0;
            while (readTotal < bytes) {
                const size_t chunk = std::min(kProbeChunkBytes, bytes - readTotal);
                const size_t read = file.read(buffer, chunk);
                if (read != chunk || !probeBufferMatches(buffer, chunk, static_cast<uint32_t>(readTotal))) {
                    ESP_LOGE(tag, "probe verify failed path=%s offset=%u wanted=%u got=%u", ownedPath.c_str(),
                             static_cast<unsigned int>(readTotal), static_cast<unsigned int>(chunk),
                             static_cast<unsigned int>(read));
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

    namespace {

        bool tryMountFrequency(bool& mounted, int frequencyKhz) {
            ESP_LOGD("sd-check", "trying mount at %d kHz", frequencyKhz);
            Board::Storage::end();
            mounted = Board::Storage::mount(StoragePaths::kMountPoint, frequencyKhz);
            if (!mounted) {
                return false;
            }
            if (!probe(StoragePaths::kSdFrequencyProbePath, kFrequencyProbeBytes, "sd-probe")) {
                ESP_LOGE("sd-check", "frequency %d kHz failed sustained probe", frequencyKhz);
                Board::Storage::end();
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
            Board::Storage::end();
            mounted = false;
            sMountedFrequencyKhz = 0;
        }

    } // namespace

    bool mount(bool& mounted, int* mountedFrequencyKhz) {
        if (mounted) {
            if (mountedFrequencyKhz != nullptr) {
                *mountedFrequencyKhz = sMountedFrequencyKhz;
            }
            return true;
        }

        if (!Board::Storage::supportsFrequencySelection()) {
            mounted = Board::Storage::mount(StoragePaths::kMountPoint, SDMMC_FREQ_DEFAULT);
            if (mounted) {
                recordMountedFrequency(SDMMC_FREQ_DEFAULT, mountedFrequencyKhz);
            }
            return mounted;
        }

        if (!Board::Storage::setSdMmcPins()) {
            ESP_LOGE("sd-check", "SD pin setup failed");
            return false;
        }

        FrequencyList frequencyOrder;
        const size_t frequencyCount = buildFrequencyProbeOrder(frequencyOrder);

        const FrequencyCache cache = readFrequencyCache();
        if (cache.valid) {
            ESP_LOGD("sd-check", "trying cached SD frequency %d kHz", cache.frequencyKhz);
            if (tryMountFrequency(mounted, cache.frequencyKhz)) {
                if (cacheMatchesMountedCard(cache)) {
                    recordMountedFrequency(cache.frequencyKhz, mountedFrequencyKhz);
                    ESP_LOGI("sd-check", "selected cached SD frequency %d kHz", cache.frequencyKhz);
                    return true;
                }

                ESP_LOGD("sd-check", "cached SD frequency belongs to a different card; rediscovering");
                unmountCard(mounted);
            }
        }

        for (size_t i = 0; i < frequencyCount; ++i) {
            const int frequencyKhz = frequencyOrder[i];
            if (tryMountFrequency(mounted, frequencyKhz)) {
                recordMountedFrequency(frequencyKhz, mountedFrequencyKhz);
                ESP_LOGI("sd-check", "selected SD frequency %d kHz", frequencyKhz);
                writeFrequencyCache(frequencyKhz);
                return true;
            }
        }
        sMountedFrequencyKhz = 0;
        return false;
    }

    int mountedFrequencyKhz() {
        return sMountedFrequencyKhz;
    }

} // namespace SdCard
