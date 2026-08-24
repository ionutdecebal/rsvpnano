#include "storage/migration/Migration.h"

#include <Arduino.h>
#include <esp_log.h>

#include <array>
#include <cerrno>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include "board/BoardStorage.h"
#include "fonts/FontCatalog.h"
#include "locales/LocalePack.h"
#include "logging/Logger.h"
#include "rss/RssConfig.h"
#include "settings/SettingsCodec.h"
#include "storage/fs/StorageFiles.h"
#include "storage/fs/StoragePaths.h"
#include "storage/index/IndexedBook.h"
#include "timer/FocusTimers.h"
#include "ui/Theme.h"

namespace StorageMigration {
    namespace {

        constexpr const char* kLegacyLibraryPath = "/books";
        constexpr const char* kLegacyBookFilesPath = "/books/books";
        constexpr const char* kLegacyArticleFilesPath = "/books/articles";
        constexpr size_t kMaximumReportedEntries = 32;
        constexpr std::array requiredFolders = {
            StoragePaths::kLibraryPath, StoragePaths::kBookFilesPath, StoragePaths::kArticleFilesPath,
            StoragePaths::kConfigPath,  StoragePaths::kThemesPath,    StoragePaths::kFontsPath,
            StoragePaths::kLocalesPath,
        };

        std::error_code filesystemError() {
            return errno == 0 ? std::make_error_code(std::errc::io_error)
                              : std::error_code{errno, std::generic_category()};
        }

        std::string_view fileName(std::string_view path) {
            const size_t separator = path.find_last_of('/');
            return separator == std::string_view::npos ? path : path.substr(separator + 1);
        }

        void issue(Report* report, std::string message) {
            if (report == nullptr)
                return;
            report->healthy = false;
            if (report->issues.size() < kMaximumReportedEntries)
                report->issues.push_back(std::move(message));
        }

        void action(Report* report, std::string message) {
            if (report != nullptr && report->actions.size() < kMaximumReportedEntries)
                report->actions.push_back(std::move(message));
        }

        std::string uniqueDestination(std::string_view directory, std::string_view name) {
            std::string candidate = std::string{directory} + "/" + std::string{name};
            if (!Board::Storage::filesystem().exists(candidate.c_str()))
                return candidate;

            const size_t dot = name.find_last_of('.');
            const std::string_view stem = dot == std::string_view::npos ? name : name.substr(0, dot);
            const std::string_view extension = dot == std::string_view::npos ? std::string_view{} : name.substr(dot);
            for (unsigned number = 2;; ++number) {
                candidate = std::string{directory} + "/" + std::string{stem} + " (migrated " + std::to_string(number)
                          + ")" + std::string{extension};
                if (!Board::Storage::filesystem().exists(candidate.c_str()))
                    return candidate;
            }
        }

        bool moveFiles(std::string_view sourceDirectory, std::string_view targetDirectory, Report* report) {
            const std::string sourceDirectoryString{sourceDirectory};
            File directory = Board::Storage::filesystem().open(sourceDirectoryString.c_str(), FILE_READ);
            if (!directory)
                return true;
            if (!directory.isDirectory()) {
                directory.close();
                issue(report, sourceDirectoryString + " is not a folder");
                return false;
            }

            std::vector<std::string> names;
            for (File entry = directory.openNextFile(); entry; entry = directory.openNextFile()) {
                if (!entry.isDirectory())
                    names.emplace_back(fileName(entry.name()));
                entry.close();
            }
            directory.close();

            bool complete = true;
            for (const std::string& name: names) {
                const std::string source = sourceDirectoryString + "/" + name;
                const std::string target = uniqueDestination(targetDirectory, name);
                errno = 0;
                if (!Board::Storage::filesystem().rename(source.c_str(), target.c_str())) {
                    Logger::failure("storage", "migrate file", source.c_str(), target.c_str(), filesystemError());
                    issue(report, "Could not move " + source);
                    complete = false;
                    continue;
                }
                if (report != nullptr)
                    ++report->moved;
                action(report, "Moved " + source + " to " + target);
            }
            return complete;
        }

        bool removeEmptyDirectory(const char* path) {
            File directory = Board::Storage::filesystem().open(path, FILE_READ);
            if (!directory)
                return true;
            File entry = directory.openNextFile();
            const bool empty = !entry;
            if (entry)
                entry.close();
            directory.close();
            return empty && Board::Storage::filesystem().rmdir(path);
        }

        bool renameLegacyRoot(Report* report) {
            if (!StorageFiles::directoryExists(kLegacyLibraryPath)
                || StorageFiles::directoryExists(StoragePaths::kLibraryPath)) {
                return true;
            }
            errno = 0;
            if (Board::Storage::filesystem().rename(kLegacyLibraryPath, StoragePaths::kLibraryPath)) {
                action(report, "Moved /books to /library");
                return true;
            }
            Logger::failure("storage", "rename library", kLegacyLibraryPath, StoragePaths::kLibraryPath,
                            filesystemError());
            return true;
        }

        bool mergeLegacyLibrary(Report* report) {
            if (!StorageFiles::directoryExists(kLegacyLibraryPath))
                return true;

            bool complete = true;
            complete &= moveFiles(kLegacyBookFilesPath, StoragePaths::kBookFilesPath, report);
            complete &= moveFiles(kLegacyArticleFilesPath, StoragePaths::kArticleFilesPath, report);
            removeEmptyDirectory(kLegacyBookFilesPath);
            removeEmptyDirectory(kLegacyArticleFilesPath);
            complete &= moveFiles(kLegacyLibraryPath, StoragePaths::kBookFilesPath, report);
            if (!removeEmptyDirectory(kLegacyLibraryPath)) {
                issue(report, "The old /books folder still contains unsupported nested folders");
                complete = false;
            }
            return complete;
        }

        bool ensureFolders(Report* report) {
            bool complete = true;
            for (const char* path: requiredFolders) {
                if (auto directory = StorageFiles::ensureDirectory(path); !directory) {
                    Logger::failure("storage", "create directory", path, directory.error());
                    issue(report, "Could not create " + std::string{path});
                    complete = false;
                }
            }
            return complete;
        }

        bool normalizeLayout(Report* report) {
            const bool renamed = renameLegacyRoot(report);
            const bool foldersReady = ensureFolders(report);
            const bool merged = mergeLegacyLibrary(report);
            const bool flatFilesMoved = moveFiles(StoragePaths::kLibraryPath, StoragePaths::kBookFilesPath, report);
            return renamed && foldersReady && merged && flatFilesMoved;
        }

        bool readPrefix(File& file, std::span<uint8_t> output) {
            return file.seek(0) && file.read(output.data(), output.size()) == output.size();
        }

        void repairIndex(std::string_view sourcePath, Report& report) {
            const std::string indexPath = StoragePaths::indexedIndexPathFor(sourcePath);
            const std::string dataPath = StoragePaths::indexedDataPathFor(sourcePath);
            if (!StorageFiles::fileExists(indexPath.c_str()) && !StorageFiles::fileExists(dataPath.c_str()))
                return;

            BookMetadata metadata;
            if (IndexedBook::readMetadata(sourcePath, metadata))
                return;

            size_t removed = 0;
            removed += Board::Storage::filesystem().remove(indexPath.c_str()) ? 1 : 0;
            removed += Board::Storage::filesystem().remove(dataPath.c_str()) ? 1 : 0;
            report.removed += removed;
            if (removed > 0)
                action(&report, "Removed an outdated reading index for " + std::string{sourcePath});
            if (StorageFiles::fileExists(indexPath.c_str()) || StorageFiles::fileExists(dataPath.c_str()))
                issue(&report, "Could not remove an invalid reading index for " + std::string{sourcePath});
        }

        void inspectLibraryDirectory(const char* path, Report& report) {
            File directory = Board::Storage::filesystem().open(path, FILE_READ);
            if (!directory || !directory.isDirectory()) {
                if (directory)
                    directory.close();
                issue(&report, std::string{"Could not inspect "} + path);
                return;
            }

            for (File entry = directory.openNextFile(); entry; entry = directory.openNextFile()) {
                const std::string entryPath = std::string{path} + "/" + std::string{fileName(entry.name())};
                const bool directoryEntry = entry.isDirectory();
                const size_t bytes = entry.size();
                ++report.checked;
                if (directoryEntry) {
                    issue(&report, "Unexpected nested folder: " + entryPath);
                    entry.close();
                    continue;
                }

                if (entryPath.ends_with(StoragePaths::kTempExtension)
                    || entryPath.ends_with(StoragePaths::kConvertingExtension)) {
                    entry.close();
                    if (Board::Storage::filesystem().remove(entryPath.c_str())) {
                        ++report.removed;
                        action(&report, "Removed interrupted temporary file " + entryPath);
                    } else {
                        issue(&report, "Could not remove temporary file " + entryPath);
                    }
                    continue;
                }

                const bool sourceFile = StoragePaths::hasTextExtension(entryPath)
                                     || StoragePaths::hasRsvpExtension(entryPath)
                                     || StoragePaths::hasEpubExtension(entryPath);
                if (StoragePaths::hasTextExtension(entryPath) || StoragePaths::hasRsvpExtension(entryPath)) {
                    if (bytes == 0)
                        issue(&report, "Empty library file: " + entryPath);
                } else if (StoragePaths::hasEpubExtension(entryPath)) {
                    std::array<uint8_t, 2> magic{};
                    constexpr std::array<uint8_t, 2> zipMagic{'P', 'K'};
                    if (bytes < magic.size() || !readPrefix(entry, magic) || magic != zipMagic)
                        issue(&report, "Invalid EPUB file: " + entryPath);
                } else if (!StoragePaths::isHiddenOrSidecarPath(entryPath)
                           && !entryPath.ends_with(StoragePaths::kFailedExtension)) {
                    issue(&report, "Unsupported file in the library: " + entryPath);
                }
                entry.close();
                if (sourceFile)
                    repairIndex(entryPath, report);
            }
            directory.close();
        }

        void inspectThemes(Report& report) {
            File directory = Board::Storage::filesystem().open(StoragePaths::kThemesPath, FILE_READ);
            if (!directory || !directory.isDirectory()) {
                if (directory)
                    directory.close();
                return;
            }
            for (File entry = directory.openNextFile(); entry; entry = directory.openNextFile()) {
                const std::string path =
                    std::string{StoragePaths::kThemesPath} + "/" + std::string{fileName(entry.name())};
                const bool inspect = !entry.isDirectory() && ui::themes::hasThemeExtension(path);
                entry.close();
                if (!inspect)
                    continue;
                ++report.checked;
                auto text =
                    StorageFiles::readTextFile(Board::Storage::filesystem(), path.c_str(), settings::kMaxSettingsBytes);
                if (!text || !ui::themes::decodeToml(*text, ui::themes::themeIdFromPath(path)))
                    issue(&report, "Invalid theme file: " + path);
            }
            directory.close();
        }

        void inspectFonts(Report& report) {
            File directory = Board::Storage::filesystem().open(StoragePaths::kFontsPath, FILE_READ);
            if (!directory || !directory.isDirectory()) {
                if (directory)
                    directory.close();
                return;
            }
            for (File entry = directory.openNextFile(); entry; entry = directory.openNextFile()) {
                const std::string path = std::string{entry.path()} + "/" + RFont4::kFilename;
                const bool inspect = entry.isDirectory();
                entry.close();
                if (!inspect)
                    continue;
                ++report.checked;
                if (!FontCatalog::inspectFontFile(path))
                    issue(&report, "Invalid or unsupported font: " + path);
            }
            directory.close();
        }

        void inspectLocales(Report& report) {
            File directory = Board::Storage::filesystem().open(StoragePaths::kLocalesPath, FILE_READ);
            if (!directory || !directory.isDirectory()) {
                if (directory)
                    directory.close();
                return;
            }
            for (File entry = directory.openNextFile(); entry; entry = directory.openNextFile()) {
                const std::string id{fileName(entry.name())};
                const bool inspect = entry.isDirectory() && !id.starts_with('.');
                entry.close();
                if (!inspect)
                    continue;
                ++report.checked;
                const std::string manifest = std::string{StoragePaths::kLocalesPath} + "/" + id + "/manifest.toml";
                auto text = StorageFiles::readTextFile(Board::Storage::filesystem(), manifest.c_str(),
                                                       locales::kMaximumManifestBytes);
                if (!text || !locales::decodeManifest(*text, id))
                    issue(&report, "Invalid or unsupported language pack: " + id);
            }
            directory.close();
        }

        void inspectConfig(Report& report) {
            if (StorageFiles::fileExists(StoragePaths::kSettingsConfigPath)) {
                ++report.checked;
                auto text = StorageFiles::readTextFile(Board::Storage::filesystem(), StoragePaths::kSettingsConfigPath,
                                                       settings::kMaxSettingsBytes);
                if (!text || !settings::codec::decodeToml(*text, settings::SettingsSource::Sd))
                    issue(&report, "Invalid reader settings file");
            }
            if (StorageFiles::fileExists(StoragePaths::kFocusConfigPath)) {
                ++report.checked;
                auto text = StorageFiles::readTextFile(Board::Storage::filesystem(), StoragePaths::kFocusConfigPath,
                                                       focus::kMaxConfigBytes);
                if (!text || !focus::decodeToml(*text))
                    issue(&report, "Invalid focus timer file");
            }
            if (StorageFiles::fileExists(StoragePaths::kRssConfigPath)) {
                ++report.checked;
                auto text = StorageFiles::readTextFile(Board::Storage::filesystem(), StoragePaths::kRssConfigPath,
                                                       rss::kMaxConfigBytes);
                if (!text || !rss::decodeToml(*text))
                    issue(&report, "Invalid RSS feed file");
            }
        }

    } // namespace

    bool prepareLayout() {
        return normalizeLayout(nullptr);
    }

    Report repair(bool mounted, SdDiagnostics::Inventory inventory) {
        Report report;
        if (!mounted) {
            issue(&report, "The SD card is not mounted");
        } else {
            normalizeLayout(&report);
            inspectLibraryDirectory(StoragePaths::kBookFilesPath, report);
            inspectLibraryDirectory(StoragePaths::kArticleFilesPath, report);
            inspectThemes(report);
            inspectFonts(report);
            inspectLocales(report);
            inspectConfig(report);
        }

        const SdDiagnostics::Result diagnostic = SdDiagnostics::run(mounted, inventory);
        report.diagnosticSummary = diagnostic.summary;
        report.diagnosticDetail = diagnostic.detail;
        if (!diagnostic.summary.starts_with("Storage OK"))
            report.healthy = false;
        return report;
    }

    std::string currentPath(std::string_view path) {
        constexpr std::string_view legacyRoot = kLegacyLibraryPath;
        if (path == legacyRoot
            || (path.starts_with(legacyRoot) && path.size() > legacyRoot.size() && path[legacyRoot.size()] == '/')) {
            return std::string{StoragePaths::kLibraryPath} + std::string{path.substr(legacyRoot.size())};
        }
        return std::string{path};
    }

} // namespace StorageMigration
