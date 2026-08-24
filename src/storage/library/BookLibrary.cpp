#include "storage/library/BookLibrary.h"
#include <esp_log.h>

#include <Arduino.h>
#include <algorithm>
#include <array>
#include <charconv>
#include <system_error>
#include <vector>
#include "board/BoardStorage.h"

#include "hash/Fnv1a.h"
#include "storage/fs/StoragePaths.h"
#include "storage/library/EpubCache.h"
#include "text/AsciiText.h"
#include "text/RsvpDirectives.h"
#include "text/TextNormalizer.h"

namespace BookLibrary {
    namespace {

        using RsvpText::readRsvpDirectiveValues;
        using RsvpText::RsvpDirectiveValues;
        using namespace StoragePaths;

        struct DirectoryEntryInfo {
            std::string path;
            size_t bytes = 0;
            bool readable = false;
        };

        std::string_view fileName(std::string_view path) {
            const size_t separator = path.find_last_of('/');
            return separator == std::string_view::npos ? path : path.substr(separator + 1);
        }

        bool equalsIgnoreCase(std::string_view left, std::string_view right) {
            return std::ranges::equal(left, right, {}, AsciiText::toLower, AsciiText::toLower);
        }

        std::vector<DirectoryEntryInfo> scanLibraryDirectories() {
            std::vector<DirectoryEntryInfo> entries;
            auto makeEntryInfo = [](std::string_view directoryPath, std::string_view name, size_t bytes) {
                DirectoryEntryInfo info;
                info.path.reserve(directoryPath.size() + name.size() + 1);
                info.path.append(directoryPath).append("/").append(name);
                info.bytes = bytes;
                return info;
            };
            auto appendDirectoryEntries = [&](const char* directoryPath) {
                File dir = Board::Storage::filesystem().open(directoryPath);
                if (!dir || !dir.isDirectory()) {
                    if (dir) {
                        dir.close();
                    }
                    return;
                }

                for (File entry = dir.openNextFile(); entry; entry = dir.openNextFile()) {
                    if (!entry.isDirectory()) {
                        const std::string_view name = fileName(entry.name());
                        if (!name.empty())
                            entries.push_back(makeEntryInfo(directoryPath, name, static_cast<size_t>(entry.size())));
                    }

                    entry.close();
                }

                dir.close();
            };

            appendDirectoryEntries(StoragePaths::kLibraryPath);
            appendDirectoryEntries(StoragePaths::kBookFilesPath);
            appendDirectoryEntries(StoragePaths::kArticleFilesPath);
            return entries;
        }

        bool inventoryHasFileWithBytes(const std::vector<DirectoryEntryInfo>& entries, std::string_view path) {
            return std::ranges::any_of(entries, [&](const DirectoryEntryInfo& candidate) {
                return candidate.bytes > 0 && equalsIgnoreCase(candidate.path, path);
            });
        }

        std::vector<DirectoryEntryInfo> collectBooks(bool onDeviceEpubConversionEnabled) {
            const uint32_t startedMs = millis();
            std::vector<DirectoryEntryInfo> entries = scanLibraryDirectories();
            const size_t fileCount = entries.size();
            size_t cacheProbeCount = 0;

            auto hasStaleGeneratedRsvp = [&](std::string_view path) {
                if (!StoragePaths::hasRsvpExtension(path)
                    || !inventoryHasFileWithBytes(entries, StoragePaths::epubSiblingPathForRsvp(path))) {
                    return false;
                }
                ++cacheProbeCount;
                return !EpubCache::rsvpIsCurrent(path);
            };

            auto isReadableText = [&](std::string_view path) {
                return StoragePaths::hasTextExtension(path)
                    && !inventoryHasFileWithBytes(entries,
                                                  StoragePaths::siblingPathWithExtension(path,
                                                                                         StoragePaths::kRsvpExtension));
            };

            auto isPendingEpub = [&](std::string_view path) {
                if (!onDeviceEpubConversionEnabled || !StoragePaths::hasEpubExtension(path)) {
                    return false;
                }

                const std::string rsvpPath = StoragePaths::rsvpCachePathForEpub(path);
                if (!inventoryHasFileWithBytes(entries, rsvpPath)) {
                    return true;
                }

                ++cacheProbeCount;
                return !EpubCache::hasCurrentCache(path);
            };

            for (DirectoryEntryInfo& entry: entries) {
                const std::string& path = entry.path;
                if (StoragePaths::isHiddenOrSidecarPath(path))
                    continue;
                entry.readable = (!hasStaleGeneratedRsvp(path) && StoragePaths::hasRsvpExtension(path))
                              || isReadableText(path) || isPendingEpub(path);
            }
            std::erase_if(entries, [](const DirectoryEntryInfo& entry) {
                return !entry.readable;
            });

            std::ranges::sort(entries, [](const DirectoryEntryInfo& left, const DirectoryEntryInfo& right) {
                return std::ranges::lexicographical_compare(fileName(left.path), fileName(right.path), {},
                                                            AsciiText::toLower, AsciiText::toLower);
            });

            ESP_LOGD("storage", "Directory inventory: %u files, %u books, %u cache probes in %lu ms",
                     static_cast<unsigned int>(fileCount), static_cast<unsigned int>(entries.size()),
                     static_cast<unsigned int>(cacheProbeCount), static_cast<unsigned long>(millis() - startedMs));

            return entries;
        }

    } // namespace

    using namespace StoragePaths;

    void refresh(Listing& listing, bool includeMetadata, bool onDeviceEpubConversionEnabled) {
        auto books = collectBooks(onDeviceEpubConversionEnabled);
        listing.clear();
        listing.reserve(books.size());
        for (DirectoryEntryInfo& book: books) {
            Entry entry{.path = std::move(book.path), .bytes = book.bytes};
            entry.title = RsvpText::normalizeDisplayText(displayNameWithoutExtension(entry.path));
            listing.push_back(std::move(entry));
        }

        const std::array counts{
            std::ranges::count_if(listing,
                                  [](const Entry& book) {
                                      return hasRsvpExtension(book.path);
                                  }),
            std::ranges::count_if(listing,
                                  [](const Entry& book) {
                                      return hasTextExtension(book.path);
                                  }),
            std::ranges::count_if(listing,
                                  [](const Entry& book) {
                                      return hasEpubExtension(book.path);
                                  }),
        };

        auto rebuildMetadata = [&]() {
            const uint32_t startedMs = millis();
            size_t rsvpMetadataCount = 0;
            for (Entry& book: listing) {
                refreshMetadata(book);
                if (hasRsvpExtension(book.path))
                    ++rsvpMetadataCount;
            }

            ESP_LOGD("storage", "Metadata cache: %u entries (%u rsvp) in %lu ms",
                     static_cast<unsigned int>(listing.size()), static_cast<unsigned int>(rsvpMetadataCount),
                     static_cast<unsigned long>(millis() - startedMs));
        };

        // Metadata is optional for fast startup scans, but counts are always logged.
        if (includeMetadata) {
            rebuildMetadata();
        } else {
            ESP_LOGW("storage", "Metadata cache skipped for %u entries", static_cast<unsigned int>(listing.size()));
        }

        ESP_LOGD("storage", "Library scan: %u books (%u rsvp, %u txt, %u pending epub)",
                 static_cast<unsigned int>(listing.size()), static_cast<unsigned int>(counts[0]),
                 static_cast<unsigned int>(counts[1]), static_cast<unsigned int>(counts[2]));
    }

    void refreshMetadata(Entry& book) {
        if (book.metadataLoaded)
            return;
        if (hasRsvpExtension(book.path)) {
            RsvpDirectiveValues values = readRsvpDirectiveValues(book.path);
            if (!values.title.empty())
                book.title = RsvpText::normalizeDisplayText(values.title);
            book.author = RsvpText::normalizeDisplayText(values.author);
        } else if (hasEpubExtension(book.path)) {
            book.author = RsvpText::normalizeDisplayText(EpubCache::libraryLabel(book.path));
        }
        book.metadataLoaded = true;
    }

    const Entry* at(const Listing& listing, size_t index) {
        return index < listing.size() ? &listing[index] : nullptr;
    }

    bool isArticle(const Entry& book) {
        return std::string_view{book.path}.starts_with(kArticleFilesPrefix);
    }

    std::string_view displayName(const Entry& book) {
        return book.title;
    }

    std::string_view relativeName(const Entry& book) {
        const std::string prefix = std::string{kLibraryPath} + "/";
        const std::string_view path = book.path;
        if (path.starts_with(prefix))
            return path.substr(prefix.length());
        const size_t separator = path.find_last_of('/');
        return separator == std::string_view::npos ? path : path.substr(separator + 1);
    }

    std::string id(const Entry& book) {
        const uint32_t hash = Fnv1a::hash(book.path);
        std::array<char, 8> digits{};
        const auto [end, error] = std::to_chars(digits.data(), digits.data() + digits.size(), hash, 16);
        if (error != std::errc{})
            return "b00000000";

        std::string result{"b"};
        const size_t digitCount = static_cast<size_t>(end - digits.data());
        result.append(digits.size() - digitCount, '0');
        result.append(digits.data(), digitCount);
        return result;
    }

    int indexOfPath(const Listing& listing, std::string_view target) {
        const auto item = std::ranges::find(listing, target, &Entry::path);
        if (item == listing.end()) {
            return -1;
        }
        return static_cast<int>(std::distance(listing.begin(), item));
    }

} // namespace BookLibrary
