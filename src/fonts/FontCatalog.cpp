#include "fonts/FontCatalog.h"

#include <esp_log.h>
#if defined(BOARD_HAS_PSRAM)
#include <esp_heap_caps.h>
#endif

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <iterator>
#include <ranges>
#include <span>
#include <system_error>
#include <utility>

#include "board/BoardStorage.h"
#include "fonts/LiterataFallbackAlpha4.h"
#include "logging/Logger.h"
#include "storage/fs/StorageFiles.h"
#include "storage/fs/StoragePaths.h"
#include "text/AsciiText.h"

namespace {

    constexpr const char* kFallbackId = "literata";
    constexpr const char* kFallbackLabel = "Literata";
    constexpr auto& kFallbackFonts = ui::fonts::LiterataFallbackAlpha4_Sizes;
    static_assert(std::size(kFallbackFonts) == RFont4::kStrikeCount);

    std::string fontPath(std::string_view id) {
        return std::string{StoragePaths::kFontsPath} + "/" + std::string{id} + "/" + RFont4::kFilename;
    }

    template<typename T, size_t Extent>
    std::expected<void, std::string> readSection(File& file, uint32_t offset, std::span<T, Extent> out) {
        if (out.empty())
            return {};
        if (!file.seek(offset))
            return std::unexpected("Font section seek failed");
        if (file.read(reinterpret_cast<uint8_t*>(out.data()), out.size_bytes()) != out.size_bytes())
            return std::unexpected("Font section read failed");
        return {};
    }

#if defined(BOARD_HAS_PSRAM)
    std::expected<void, std::string> readPsramSection(File& file, uint32_t offset, std::span<uint8_t> out) {
        constexpr size_t kTransferBytes = 4096;
        auto* transfer = static_cast<uint8_t*>(heap_caps_malloc(kTransferBytes, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL));
        if (transfer == nullptr)
            return std::unexpected("Font transfer buffer unavailable");
        if (!file.seek(offset)) {
            heap_caps_free(transfer);
            return std::unexpected("Font section seek failed");
        }
        for (size_t copied = 0; copied < out.size();) {
            const size_t chunk = std::min(kTransferBytes, out.size() - copied);
            if (file.read(transfer, chunk) != chunk) {
                heap_caps_free(transfer);
                return std::unexpected("Font section read failed");
            }
            std::memcpy(out.data() + copied, transfer, chunk);
            copied += chunk;
        }
        heap_caps_free(transfer);
        return {};
    }
#endif

    std::expected<RFont4::Header, std::string> readHeader(File& file) {
        RFont4::Header header;
        if (!file.seek(0) || file.read(reinterpret_cast<uint8_t*>(&header), sizeof(header)) != sizeof(header))
            return std::unexpected("Font header read failed");
        return header;
    }

    std::expected<RFont4::Directory, std::string> readDirectory(File& file) {
        return readHeader(file).and_then([&](const RFont4::Header& header)
                                             -> std::expected<RFont4::Directory, std::string> {
            if (!RFont4::headerValid(header, file.size()))
                return std::unexpected("Unsupported or corrupt font format");
            RFont4::Directory directory;
            directory.header = header;
            return readSection(file, header.strikesOffset, std::span{directory.strikes}.first(header.strikeCount))
                .and_then([&] {
                    return readSection(file, header.layoutTablesOffset,
                                       std::span{directory.layoutTables}.first(header.layoutTableCount));
                })
                .and_then([&]() -> std::expected<RFont4::Directory, std::string> {
                    const auto tables = std::span{directory.layoutTables}.first(header.layoutTableCount);
                    if (!RFont4::layoutValid(header, directory.strikes, tables, file.size()))
                        return std::unexpected("Unsupported or corrupt font format");
                    return directory;
                });
        });
    }

    std::expected<std::string, std::string> readName(File& file, const RFont4::Header& header) {
        std::vector<uint8_t> bytes(header.nameSize);
        return readSection(file, header.nameOffset, std::span{bytes})
            .and_then([&]() -> std::expected<std::string, std::string> {
                if (bytes.back() != '\0')
                    return std::unexpected("Font name is invalid");
                return std::string{reinterpret_cast<const char*>(bytes.data()), bytes.size() - 1};
            });
    }

    std::expected<std::string, std::string> readLocales(File& file, const RFont4::Header& header) {
        if (header.localeSize == 0)
            return std::string{};
        std::string locales(header.localeSize, '\0');
        return readSection(file, header.localeOffset, std::span{locales})
            .and_then([&]() -> std::expected<std::string, std::string> {
                if (locales.back() != '\0')
                    return std::unexpected("Font locales are invalid");
                return locales;
            });
    }

} // namespace

FontCatalog::FontCatalog() {
    reset();
}

void FontCatalog::reset() {
    clearLoaded();
    families_ = {
        {.id = kFallbackId, .label = kFallbackLabel, .builtIn = true, .scriptMask = kFallbackFonts[0]->scriptMask}};
}

void FontCatalog::loadFromSd() {
    reset();
    if (auto created = StorageFiles::ensureDirectory(StoragePaths::kFontsPath); !created) {
        Logger::failure("font", "create directory", StoragePaths::kFontsPath, created.error());
        return;
    }

    File root = Board::Storage::filesystem().open(StoragePaths::kFontsPath);
    if (!root || !root.isDirectory()) {
        if (root)
            root.close();
        return;
    }

    size_t rejected = 0;
    std::vector<std::pair<Family, std::string>> candidates;
    while (File entry = root.openNextFile()) {
        if (!entry.isDirectory()) {
            entry.close();
            continue;
        }

        std::string directory = entry.path();
        const std::string path = directory + "/" + RFont4::kFilename;
        auto family = inspectFontFile(path);
        entry.close();
        if (family && family->id != kFallbackId) {
            candidates.emplace_back(std::move(*family), std::move(directory));
        } else if (!family) {
            ++rejected;
            ESP_LOGW("font", "rejected %s: %s", path.c_str(), family.error().c_str());
        }
    }
    root.close();

    for (auto& [family, directory]: candidates) {
        if (find(family.id))
            continue;
        const std::string canonicalDirectory = std::string{StoragePaths::kFontsPath} + "/" + family.id;
        if (directory != canonicalDirectory) {
            errno = 0;
            if (!Board::Storage::filesystem().rename(directory.c_str(), canonicalDirectory.c_str())) {
                const int error = errno == 0 ? EIO : errno;
                Logger::failure("font", "canonicalize directory", directory.c_str(), canonicalDirectory.c_str(),
                                std::error_code{error, std::generic_category()});
                ++rejected;
                continue;
            }
            ESP_LOGI("font", "canonicalized directory from=%s to=%s", directory.c_str(), canonicalDirectory.c_str());
        }
        families_.push_back(std::move(family));
    }

    std::ranges::sort(families_.begin() + 1, families_.end(), {}, &Family::label);
    ESP_LOGI("font", "catalog ready installed=%u rejected=%u", static_cast<unsigned>(families_.size() - 1),
             static_cast<unsigned>(rejected));
}

std::expected<std::reference_wrapper<const FontCatalog::Family>, std::string> FontCatalog::install(std::string_view
                                                                                                       stagedPath) {
    const std::string staged{stagedPath};
    return inspectFontFile(stagedPath)
        .and_then([this,
                   &staged](Family inspected) -> std::expected<std::reference_wrapper<const Family>, std::string> {
            if (inspected.id == kFallbackId || find(inspected.id))
                return std::unexpected("Font family already exists");

            const std::string directory = std::string{StoragePaths::kFontsPath} + "/" + inspected.id;
            const std::string finalPath = directory + "/" + RFont4::kFilename;
            return StorageFiles::ensureDirectory(StoragePaths::kFontsPath)
                .transform_error([](std::error_code) {
                    return std::string{"Fonts folder could not be created"};
                })
                .and_then([&] {
                    return StorageFiles::ensureDirectory(directory.c_str()).transform_error([](std::error_code) {
                        return std::string{"Font folder could not be created"};
                    });
                })
                .and_then([&]() -> std::expected<void, std::string> {
                    return StorageFiles::replaceFileAtomic(Board::Storage::filesystem(), finalPath.c_str(),
                                                           staged.c_str(), (finalPath + ".bak").c_str())
                        .transform_error([&](std::error_code) {
                            Board::Storage::filesystem().rmdir(directory.c_str());
                            return std::string{"Font file could not be installed"};
                        });
                })
                .transform([&, id = inspected.id] {
                    families_.push_back(std::move(inspected));
                    std::ranges::sort(families_.begin() + 1, families_.end(), {}, &Family::label);
                    return std::cref(*find(id));
                });
        });
}

std::expected<void, std::string> FontCatalog::remove(std::string_view id) {
    const auto family = find(id);
    if (!family)
        return std::unexpected("Font not found");
    if (family->builtIn)
        return std::unexpected("Built-in font cannot be removed");

    const std::string familyId = family->id;
    const std::string path = fontPath(familyId);
    clearLoaded();
    if (!Board::Storage::filesystem().remove(path.c_str()))
        return std::unexpected("Font file could not be removed");
    Board::Storage::filesystem().rmdir(StoragePaths::parentDirectoryForPath(path).c_str());
    std::erase_if(families_, [&familyId](const Family& candidate) {
        return candidate.id == familyId;
    });
    return {};
}

const FontCatalog::Family* FontCatalog::find(std::string_view id) const {
    const auto findId = [this](std::string_view value) {
        return std::ranges::find_if(families_, [value](const Family& family) {
            return family.id == value;
        });
    };
    auto found = findId(id);
    if (found == families_.end())
        found = findId(normalizeId(id));
    return found == families_.end() ? nullptr : &*found;
}

FontCatalog::Face FontCatalog::loadFaceFittingHeight(size_t familyIndex, std::span<const size_t> sizes,
                                                   int16_t height, bool vertical) {
    // Inspect metrics before loading: each runtime family retains only one raster strike.
    const size_t safeFamily = std::min(familyIndex, families_.size() - 1);
    const RFont4::Directory* directory = nullptr;
    if (!families_[safeFamily].builtIn) {
        if (auto family = loadRuntimeFamily(safeFamily))
            directory = &family->get().directory;
    }
    for (const size_t size : sizes) {
        const size_t actualSize = directory && size >= directory->header.strikeCount
                                    ? (size == RFont4::kPreviewStrikeIndex ? RFont4::kCompactStrikeIndex : 0)
                                    : size;
        const auto& fallback = *kFallbackFonts[actualSize];
        const int16_t pixelsPerEm = directory ? directory->strikes[actualSize].pixelsPerEm : fallback.pixelsPerEm;
        const int16_t inkHeight = directory
            ? directory->strikes[actualSize].wordInkBottom - directory->strikes[actualSize].wordInkTop + 1
            : fallback.wordInkBottom - fallback.wordInkTop + 1;
        const int16_t requiredHeight = vertical ? pixelsPerEm : std::max(pixelsPerEm, inkHeight);
        if (requiredHeight <= height || size == sizes.back())
            return loadFace(safeFamily, actualSize);
    }
    return loadFace(safeFamily, RFont4::kCompactStrikeIndex);
}

FontCatalog::Face FontCatalog::loadFace(size_t familyIndex, size_t sizeIndex) {
    const size_t safeFamily = std::min(familyIndex, families_.size() - 1);
    const size_t safeSize = std::min(sizeIndex, RFont4::kStrikeCount - 1);
    if (!families_[safeFamily].builtIn) {
        const std::string path = fontPath(families_[safeFamily].id);
        auto family = loadRuntimeFamily(safeFamily);
        if (family) {
            LoadedFamily& loaded = family->get();
            const size_t actualSize = safeSize < loaded.directory.header.strikeCount ? safeSize
                                      : safeSize == RFont4::kPreviewStrikeIndex ? RFont4::kCompactStrikeIndex : 0;
            auto strike = loadRuntimeStrike(loaded, actualSize);
            if (strike) {
                TextShaping::Shaper* shaper = nullptr;
                if (families_[safeFamily].shaping && !loaded.shapingFailed) {
                    if (!loaded.shaper.ready()) {
                        const auto tables =
                            std::span{loaded.directory.layoutTables}.first(loaded.directory.header.layoutTableCount);
                        if (auto opened = loaded.shaper.open(loaded.file, loaded.directory.header, tables); !opened) {
                            loaded.shapingFailed = true;
                            ESP_LOGE("font", "shaping failed %s: %s", path.c_str(), opened.error().c_str());
                        }
                    }
                    if (loaded.shaper.ready())
                        shaper = &loaded.shaper;
                }
                return {.raster = *strike, .shaper = shaper};
            }
            ESP_LOGE("font", "load failed %s: %s", path.c_str(), strike.error().c_str());
        } else {
            ESP_LOGE("font", "load failed %s: %s", path.c_str(), family.error().c_str());
        }
    }
    return {.raster = std::cref(*kFallbackFonts[safeSize])};
}

void FontCatalog::clearLoaded() {
    loadedFamilies_.clear();
    fileCache_.reset();
}

std::expected<std::reference_wrapper<FontCatalog::LoadedFamily>, std::string> FontCatalog::
    loadRuntimeFamily(size_t familyIndex) {
    const auto existing = std::ranges::find_if(loadedFamilies_, [familyIndex](const LoadedFamily& loaded) {
        return loaded.familyIndex == familyIndex;
    });
    if (existing != loadedFamilies_.end())
        return std::ref(*existing);

    if (!fileCache_)
        fileCache_ = std::make_unique<ui::fonts::RFontFileCache>();

    LoadedFamily& loaded = loadedFamilies_.emplace_back();
    loaded.familyIndex = familyIndex;
    const std::string path = fontPath(families_[familyIndex].id);
    loaded.file = Board::Storage::filesystem().open(path.c_str(), FILE_READ);
    const auto fail = [this](std::string error) -> std::expected<std::reference_wrapper<LoadedFamily>, std::string> {
        loadedFamilies_.pop_back();
        return std::unexpected(std::move(error));
    };
    if (!loaded.file || loaded.file.isDirectory())
        return fail("Font file unavailable");
    auto directory = readDirectory(loaded.file);
    if (!directory)
        return fail(directory.error());
    loaded.directory = std::move(*directory);

#if defined(BOARD_HAS_PSRAM)
    {
        constexpr uint32_t kPsramCapabilities = MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT;
        const RFont4::Header& header = loaded.directory.header;
        const uint32_t metadataEnd = header.verticalRulesOffset
                                   + header.verticalRuleCount * sizeof(RFont4::VerticalRule);
        const size_t leadingSize = header.pageMapOffset - header.supplementaryOffset;
        const size_t trailingSize = metadataEnd - header.pageTablesOffset;
        const size_t metadataSize = leadingSize + trailingSize;
        if (metadataSize <= heap_caps_get_largest_free_block(kPsramCapabilities))
            loaded.residentMetadata.reset(static_cast<uint8_t*>(heap_caps_malloc(metadataSize, kPsramCapabilities)));
        if (loaded.residentMetadata) {
            if (leadingSize != 0) {
                if (auto read = readPsramSection(loaded.file, header.supplementaryOffset,
                                                 {loaded.residentMetadata.get(), leadingSize});
                    !read)
                    return fail(read.error());
            }
            if (auto read = readPsramSection(loaded.file, header.pageTablesOffset,
                                             {loaded.residentMetadata.get() + leadingSize, trailingSize});
                !read)
                return fail(read.error());
            ESP_LOGI("font", "resident family metadata family=%s bytes=%u", families_[familyIndex].id.c_str(),
                     static_cast<unsigned>(metadataSize));
        }
    }
#endif
    if (auto read = readSection(loaded.file, loaded.directory.header.pageMapOffset, std::span{loaded.pageMap}); !read)
        return fail(read.error());
    return std::ref(loaded);
}

std::expected<std::reference_wrapper<const ui::fonts::AlphaFont>, std::string> FontCatalog::
    loadRuntimeStrike(LoadedFamily& family, size_t sizeIndex) {
    if (family.loadedStrike && family.loadedStrike->sizeIndex == sizeIndex)
        return std::cref(family.loadedStrike->font);

    LoadedStrike& loaded = family.loadedStrike.emplace();
    loaded.sizeIndex = sizeIndex;
    const RFont4::StrikeRecord& strike = family.directory.strikes[sizeIndex];
    const RFont4::Header& header = family.directory.header;
    loaded.font = {
        .name = families_[family.familyIndex].label,
        .glyphCount = header.glyphCount,
        .supplementaryCount = header.supplementaryCount,
        .verticalRuleCount = header.verticalRuleCount,
        .yAdvance = strike.yAdvance,
        .ascent = strike.ascent,
        .descent = strike.descent,
        .pageTableCount = header.pageTableCount,
        .kerningPairCount = strike.kerningPairCount,
        .wordInkTop = strike.wordInkTop,
        .wordInkBottom = strike.wordInkBottom,
        .glyphMapCount = header.sourceGlyphCount,
        .scriptMask = header.scriptMask,
        .pixelsPerEm = strike.pixelsPerEm,
        .file = &family.file,
        .fileCache = fileCache_.get(),
        .fileSize = header.totalSize,
        .generation = nextFontGeneration_++,
        .fileHeader = header,
        .fileStrike = strike,
        .bitsPerPixel = strike.bitsPerPixel,
        .bitmapEncoding = strike.bitmapEncoding,
    };

    loaded.font.pageMap = family.pageMap.data();
    if (family.residentMetadata) {
        const size_t leadingSize = header.pageMapOffset - header.supplementaryOffset;
        const auto section = [&](uint32_t offset) {
            return offset < header.pageMapOffset
                     ? family.residentMetadata.get() + (offset - header.supplementaryOffset)
                     : family.residentMetadata.get() + leadingSize + (offset - header.pageTablesOffset);
        };
        if (header.supplementaryCount != 0)
            loaded.font.supplementary =
                reinterpret_cast<const RFont4::SupplementaryRecord*>(section(header.supplementaryOffset));
        loaded.font.pageTableData = section(header.pageTablesOffset);
        if (header.layoutTableCount != 0) {
            loaded.font.glyphIds = section(header.glyphIdsOffset);
            loaded.font.glyphMap = section(header.glyphMapOffset);
        }
        if (header.verticalRuleCount != 0)
            loaded.font.verticalRules =
                reinterpret_cast<const RFont4::VerticalRule*>(section(header.verticalRulesOffset));
    }

#if defined(BOARD_HAS_PSRAM)
    {
        constexpr uint32_t kPsramCapabilities = MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT;
        const bool compact = sizeIndex == RFont4::kCompactStrikeIndex;
        const bool residentBitmap = compact;
        const uint32_t residentEnd = residentBitmap ? strike.bitmapOffset + strike.bitmapSize : strike.bitmapOffset;
        const size_t residentSize = residentEnd - strike.glyphsOffset;
        const size_t available = heap_caps_get_largest_free_block(kPsramCapabilities);
        if (residentSize <= available)
            loaded.residentData.reset(static_cast<uint8_t*>(heap_caps_malloc(residentSize, kPsramCapabilities)));
        if (loaded.residentData) {
            if (auto read =
                    readPsramSection(family.file, strike.glyphsOffset, {loaded.residentData.get(), residentSize});
                !read) {
                family.loadedStrike.reset();
                return std::unexpected(read.error());
            }

            const auto section = [&](uint32_t offset) {
                return loaded.residentData.get() + (offset - strike.glyphsOffset);
            };
            loaded.font.glyphs = reinterpret_cast<const RFont4::GlyphRecord*>(section(strike.glyphsOffset));
            loaded.font.kerningPairs = reinterpret_cast<const RFont4::KerningRecord*>(section(strike.kerningOffset));
            if (residentBitmap)
                loaded.font.bitmap = section(strike.bitmapOffset);
            ESP_LOGI("font", "resident %s family=%s bytes=%u", compact ? "compact strike" : "metrics",
                     families_[family.familyIndex].id.c_str(), static_cast<unsigned>(residentSize));
            return std::cref(loaded.font);
        }
        ESP_LOGW("font", "strike remains file-backed family=%s bytes=%u", families_[family.familyIndex].id.c_str(),
                 static_cast<unsigned>(residentSize));
    }
#endif
    return std::cref(loaded.font);
}

std::string FontCatalog::normalizeId(std::string_view value) {
    while (!value.empty() && AsciiText::isWhitespace(value.front()))
        value.remove_prefix(1);
    while (!value.empty() && AsciiText::isWhitespace(value.back()))
        value.remove_suffix(1);
    std::string out;
    out.reserve(value.size());
    bool previousDash = false;
    for (const char character: value) {
        if (AsciiText::isAlphaNumeric(character)) {
            out += AsciiText::toLower(character);
            previousDash = false;
        } else if (!previousDash && !out.empty()) {
            out.push_back('-');
            previousDash = true;
        }
    }
    if (!out.empty() && out.back() == '-')
        out.pop_back();
    return out;
}

std::expected<FontCatalog::Family, std::string> FontCatalog::inspectFontFile(std::string_view path) {
    const std::string ownedPath{path};
    File file = Board::Storage::filesystem().open(ownedPath.c_str(), FILE_READ);
    if (!file || file.isDirectory()) {
        if (file)
            file.close();
        return std::unexpected("Font file unavailable");
    }
    return readDirectory(file).and_then([&](const RFont4::Directory& directory) {
        return readName(file, directory.header).and_then([&](std::string label) -> std::expected<Family, std::string> {
            return readLocales(file, directory.header)
                .and_then([&](std::string locales) -> std::expected<Family, std::string> {
                    std::string id = normalizeId(label);
                    if (id.empty())
                        return std::unexpected("Font name is invalid");
                    return Family{.id = std::move(id),
                                  .label = std::move(label),
                                  .locales = std::move(locales),
                                  .shaping = directory.header.layoutTableCount != 0,
                                  .scriptMask = directory.header.scriptMask};
                });
        });
    });
}
