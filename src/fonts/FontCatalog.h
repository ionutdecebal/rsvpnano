#pragma once

#include <Arduino.h>
#include <FS.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <expected>
#include <functional>
#include <list>
#include <memory>
#include <optional>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "fonts/AlphaFont.h"
#include "fonts/RFont4Format.h"
#include "text/TextShaping.h"

class FontCatalog {
public:
    struct Face {
        std::reference_wrapper<const ui::fonts::AlphaFont> raster;
        TextShaping::Shaper* shaper = nullptr;
    };

    struct Family {
        std::string id;
        std::string label;
        std::string locales;
        bool builtIn = false;
        bool shaping = false;
        uint32_t scriptMask = 0;

        bool supports(uint32_t requiredScripts) const {
            return (scriptMask & requiredScripts) == requiredScripts;
        }

        bool supportsAny(uint32_t requiredScripts) const {
            return (scriptMask & requiredScripts) != 0;
        }

        bool usableFor(std::string_view locale, uint32_t requiredScripts) const {
            return supports(requiredScripts) || (prefers(locale) && supportsAny(requiredScripts));
        }

        bool prefers(std::string_view locale) const {
            while (!locale.empty()) {
                size_t offset = 0;
                while (offset < locales.size()) {
                    const std::string_view entry{locales.data() + offset};
                    if (entry == locale)
                        return true;
                    offset += entry.size() + 1;
                }
                const size_t separator = locale.rfind('-');
                if (separator == std::string_view::npos)
                    break;
                locale = locale.substr(0, separator);
            }
            return false;
        }
    };

    FontCatalog();

    void loadFromSd();
    std::expected<std::reference_wrapper<const Family>, std::string> install(std::string_view stagedPath);
    std::expected<void, std::string> remove(std::string_view id);
    std::span<const Family> families() const {
        return families_;
    }
    const Family* find(std::string_view id) const;
    Face loadFace(size_t familyIndex, size_t sizeIndex);
    Face loadFaceFittingHeight(size_t familyIndex, std::span<const size_t> sizes, int16_t height,
                               bool vertical = false);
    void clearLoaded();
#if defined(RSVP_BENCHMARK_MODE)
    void resetFileCacheStats() {
        if (fileCache_)
            fileCache_->resetStats();
    }
    ui::fonts::RFontFileCache::Stats fileCacheStats() const {
        return fileCache_ ? fileCache_->stats() : ui::fonts::RFontFileCache::Stats{};
    }
#endif
    static size_t selectFamily(std::span<const Family> families, std::string_view requested, std::string_view locale,
                               uint32_t requiredScripts) {
        if (families.empty())
            return 0;
        auto selected = std::ranges::find_if(families, [&](const Family& family) {
            return family.id == requested && family.supports(requiredScripts);
        });
        if (selected == families.end()) {
            selected = std::ranges::find_if(families, [&](const Family& family) {
                return family.prefers(locale) && family.supportsAny(requiredScripts);
            });
        }
        if (selected == families.end()) {
            selected = std::ranges::find_if(families, [&](const Family& family) {
                return family.supports(requiredScripts);
            });
        }
        if (selected == families.end()) {
            const auto requestedFamily = std::ranges::find(families, requested, &Family::id);
            const uint32_t missing =
                requiredScripts & ~(requestedFamily == families.end() ? 0U : requestedFamily->scriptMask);
            selected = std::ranges::find_if(families, [&](const Family& family) {
                return family.supportsAny(missing);
            });
        }
        return selected == families.end() ? 0 : static_cast<size_t>(selected - families.begin());
    }

    static std::expected<Family, std::string> inspectFontFile(std::string_view path);

private:
    struct FreeResidentData {
        void operator()(uint8_t* data) const {
            std::free(data);
        }
    };

    struct LoadedStrike {
        size_t sizeIndex = 0;
        std::unique_ptr<uint8_t, FreeResidentData> residentData;
        ui::fonts::AlphaFont font;
    };

    struct LoadedFamily {
        size_t familyIndex = 0;
        File file;
        RFont4::Directory directory;
        std::array<uint8_t, RFont4::kPageMapBytes> pageMap{};
        std::unique_ptr<uint8_t, FreeResidentData> residentMetadata;
        std::optional<LoadedStrike> loadedStrike;
        TextShaping::Shaper shaper;
        bool shapingFailed = false;
    };

    void reset();
    std::expected<std::reference_wrapper<LoadedFamily>, std::string> loadRuntimeFamily(size_t familyIndex);
    std::expected<std::reference_wrapper<const ui::fonts::AlphaFont>, std::string> loadRuntimeStrike(LoadedFamily&
                                                                                                         family,
                                                                                                     size_t sizeIndex);
    static std::string normalizeId(std::string_view value);

    std::vector<Family> families_;
    std::list<LoadedFamily> loadedFamilies_;
    std::unique_ptr<ui::fonts::RFontFileCache> fileCache_;
    uint32_t nextFontGeneration_ = 1;
};
