#include "storage/fs/SdDiagnostics.h"

#include <algorithm>
#include <array>
#include <esp_log.h>
#include <string>

#include "board/BoardStorage.h"
#include "storage/fs/SdCard.h"
#include "storage/fs/StorageFiles.h"
#include "storage/fs/StoragePaths.h"

namespace SdDiagnostics {
    namespace {

        constexpr uint64_t kBytesPerMegabyte = 1024ULL * 1024ULL;
        constexpr uint64_t kSdxcMinSizeMb = 32ULL * 1024ULL;
        constexpr size_t kFolderProbeBytes = 64UL * 1024UL;
        constexpr std::array requiredFolders = {
            StoragePaths::kLibraryPath, StoragePaths::kBookFilesPath, StoragePaths::kArticleFilesPath,
            StoragePaths::kConfigPath,  StoragePaths::kThemesPath,   StoragePaths::kFontsPath,
            StoragePaths::kLocalesPath,
        };
        constexpr std::array configFiles = {
            StoragePaths::kSettingsConfigPath,
            StoragePaths::kRssConfigPath,
            StoragePaths::kFocusConfigPath,
        };

        const char* cardTypeLabel(Board::Storage::CardType cardType, uint64_t sizeMb) {
            switch (cardType) {
            case Board::Storage::CardType::Mmc:
                return "MMC";
            case Board::Storage::CardType::Sd:
                return "SDSC";
            case Board::Storage::CardType::Sdhc:
                return sizeMb > kSdxcMinSizeMb ? "SDXC" : "SDHC";
            case Board::Storage::CardType::None:
            default:
                return "Unknown";
            }
        }

    } // namespace

    Result run(bool mounted, Inventory inventory) {
        if (!mounted) {
            ESP_LOGE("sd-check", "card not mounted; check FAT32 MBR, seating, or card health");
            return {.summary = "Card not mounted", .detail = "Check FAT32 MBR/card"};
        }

        const uint64_t sizeMb = Board::Storage::cardSize() / kBytesPerMegabyte;
        const char* type = cardTypeLabel(Board::Storage::cardType(), sizeMb);
        const int frequencyKhz = SdCard::mountedFrequencyKhz();
        ESP_LOGI("sd-check", "mounted type=%s size=%llu MB freq=%d kHz", type, sizeMb, frequencyKhz);

        for (const char* folder: requiredFolders) {
            if (!StorageFiles::directoryExists(folder)) {
                ESP_LOGE("sd-check", "required folder missing: %s", folder);
                return {.summary = "Folder setup failed", .detail = folder};
            }
        }

        for (const char* folder: requiredFolders) {
            const std::string probePath = std::string{folder} + "/.sdcheck.tmp";
            if (!SdCard::probe(probePath, kFolderProbeBytes, "sd-check")) {
                ESP_LOGE("sd-check", "write/read probe failed: %s", folder);
                return {.summary = "Folder write failed", .detail = folder};
            }
        }

        const size_t configs = std::ranges::count_if(configFiles, StorageFiles::fileExists);
        return {
            .summary = std::string{"Storage OK | "} + type + " " + std::to_string(sizeMb) + " MB",
            .detail = "Items " + std::to_string(inventory.libraryItems) + " | Fonts " + std::to_string(inventory.fonts)
                    + " | Themes " + std::to_string(inventory.themes) + " | Config " + std::to_string(configs) + "/"
                    + std::to_string(configFiles.size()),
        };
    }

} // namespace SdDiagnostics
