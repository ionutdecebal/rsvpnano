#include "storage/StorageManager.h"
#include <esp_log.h>

#include <Arduino.h>
#include <cstdint>
#include <limits>
#include "board/BoardStorage.h"

#include "storage/fs/SdCard.h"
#include "storage/fs/StorageFiles.h"
#include "storage/fs/StoragePaths.h"
#include "storage/index/IndexedBook.h"
#include "storage/migration/Migration.h"

#ifndef RSVP_ON_DEVICE_EPUB_CONVERSION
#define RSVP_ON_DEVICE_EPUB_CONVERSION 0
#endif

namespace {

    constexpr uint64_t kBytesPerMegabyte = 1024ULL * 1024ULL;

} // namespace

void StorageManager::ignoreStatus(void* context, const char* title, const char* line1, const char* line2,
                                  int progressPercent) {
    (void) context;
    (void) title;
    (void) line1;
    (void) line2;
    (void) progressPercent;
}

void StorageManager::setStatusCallback(StatusCallback callback, void* context) {
    statusCallback_ = callback == nullptr ? &StorageManager::ignoreStatus : callback;
    statusContext_ = callback == nullptr ? nullptr : context;
}

bool StorageManager::begin() {
    mounted_ = false;
    clearBookCache();

    statusCallback_(statusContext_, "SD", "Mounting card", "", 5);
    int mountedFrequencyKhz = 0;
    if (SdCard::mount(mounted_, &mountedFrequencyKhz)) {
        const uint64_t sizeMb = Board::Storage::cardSize() / kBytesPerMegabyte;
        ESP_LOGI("storage", "SD initialized (%llu MB, %d kHz)", sizeMb, mountedFrequencyKhz);
        if (!StorageMigration::prepareLayout()) {
            statusCallback_(statusContext_, "SD", "Folder setup failed", "Run storage check", 10);
        }
        statusCallback_(statusContext_, "SD", "Scanning books", "EPUB converts on open", 10);
        refreshBookPaths(false);
        return true;
    }

    ESP_LOGE("storage", "SD init failed after retries");
    return false;
}

void StorageManager::end() {
    if (mounted_) {
        Board::Storage::end();
    }
    mounted_ = false;
    clearBookCache();
}

void StorageManager::refreshBooks(bool includeMetadata) {
    refreshBookPaths(includeMetadata);
}

std::expected<void, std::error_code> StorageManager::installBook(std::string_view stagedPath,
                                                                 std::string_view destinationPath) {
    if (!mounted_)
        return std::unexpected(std::make_error_code(std::errc::no_such_device));
    const std::string parent = StoragePaths::parentDirectoryForPath(destinationPath);
    const bool supported = StoragePaths::hasRsvpExtension(destinationPath)
                        || StoragePaths::hasTextExtension(destinationPath)
                        || StoragePaths::hasEpubExtension(destinationPath);
    if ((parent != StoragePaths::kBookFilesPath && parent != StoragePaths::kArticleFilesPath) || !supported)
        return std::unexpected(std::make_error_code(std::errc::invalid_argument));

    const std::string staged{stagedPath};
    const std::string destination{destinationPath};
    const std::string backup = destination + ".bak";
    return StorageFiles::replaceFileAtomic(Board::Storage::filesystem(), destination.c_str(), staged.c_str(),
                                           backup.c_str())
        .transform([this] {
            refreshBookPaths(false);
        });
}

std::expected<void, std::error_code> StorageManager::removeBook(std::string_view path) {
    if (!mounted_)
        return std::unexpected(std::make_error_code(std::errc::no_such_device));
    if (findBook(path) < 0)
        return std::unexpected(std::make_error_code(std::errc::no_such_file_or_directory));

    const std::string ownedPath{path};
    if (!Board::Storage::filesystem().remove(ownedPath.c_str()))
        return std::unexpected(std::make_error_code(std::errc::io_error));
    Board::Storage::filesystem().remove(StoragePaths::indexedIndexPathFor(path).c_str());
    Board::Storage::filesystem().remove(StoragePaths::indexedDataPathFor(path).c_str());
    Board::Storage::filesystem().remove(StoragePaths::bookStatePathFor(path).c_str());
    refreshBookPaths(false);
    return {};
}

int StorageManager::findBook(std::string_view path) const {
    return BookLibrary::indexOfPath(library_, path);
}

const BookLibrary::Entry* StorageManager::book(size_t index) const {
    return BookLibrary::at(library_, index);
}

std::optional<reading::BookIdentity> StorageManager::readBookMetadata(size_t index, BookMetadata& metadata) {
    if (index >= library_.size()) {
        metadata.clear();
        return std::nullopt;
    }
    BookLibrary::Entry& entry = library_[index];

    if (entry.bytes > std::numeric_limits<uint32_t>::max()) {
        ESP_LOGW("storage", "unsupported library item: %s size=%llu", entry.path.c_str(),
                 static_cast<unsigned long long>(entry.bytes));
        metadata.clear();
        return std::nullopt;
    }

    metadata.clear();
    IndexedBookStore::Header header;
    const bool loaded = IndexedBook::readMetadata(entry.path, metadata, &header);
    BookLibrary::refreshMetadata(entry);
    if (!loaded)
        return std::nullopt;
    return header.identity;
}

bool StorageManager::loadIndexedBook(size_t index, IndexedBookStore& store, BookMetadata& metadata,
                                     IndexedBook::OpenRequest request) {
    if (!mounted_) {
        ESP_LOGE("storage", "SD not mounted, cannot load indexed book");
        statusCallback_(statusContext_, "Book open failed", "SD not mounted", "Check card", 100);
        return false;
    }

    request.statusCallback = statusCallback_;
    request.statusContext = statusContext_;
    if (!IndexedBook::load(index, library_, store, metadata, request))
        return false;
    return true;
}

void StorageManager::refreshBookPaths(bool includeMetadata) {
    if (!mounted_) {
        clearBookCache();
        return;
    }

    ESP_LOGI("storage", "library scan begin metadata=%u", includeMetadata ? 1U : 0U);
    statusCallback_(statusContext_, "SD", "Reading library", includeMetadata ? "Reading metadata" : "Finding books",
                    96);
    BookLibrary::refresh(library_, includeMetadata, RSVP_ON_DEVICE_EPUB_CONVERSION);
    ESP_LOGI("storage", "library scan ready books=%u metadata=%u", static_cast<unsigned>(library_.size()),
             includeMetadata ? 1U : 0U);
}

void StorageManager::clearBookCache() {
    library_.clear();
}
