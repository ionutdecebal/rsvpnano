#include "storage/StorageManager.h"

#include <SD_MMC.h>
#include <cstdint>

#include "book/BookMetadata.h"
#include "storage/fs/SdDiagnostics.h"
#include "storage/fs/StorageFiles.h"
#include "storage/fs/StoragePaths.h"
#include "storage/index/IndexedBook.h"

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
    listedOnce_ = false;
    clearBookCache();

    statusCallback_(statusContext_, "SD", "Mounting card", "", 5);
    int mountedFrequencyKhz = 0;
    if (SdDiagnostics::mountCard(mounted_, &mountedFrequencyKhz)) {
        const uint64_t sizeMb = SD_MMC.cardSize() / kBytesPerMegabyte;
        Serial.printf("[storage] SD initialized (%llu MB, %d kHz)\n", sizeMb, mountedFrequencyKhz);
        statusCallback_(statusContext_, "SD", "Scanning books", "EPUB converts on open", 10);
        refreshBookPaths(false);
        return true;
    }

    Serial.println("[storage] SD init failed after retries");
    return false;
}

void StorageManager::end() {
    if (mounted_) {
        SD_MMC.end();
    }
    mounted_ = false;
    listedOnce_ = false;
    clearBookCache();
}

void StorageManager::listBooks() {
    if (!mounted_ || listedOnce_) {
        return;
    }
    listedOnce_ = true;

    if (!StorageFiles::directoryExists(StoragePaths::kBooksPath)) {
        Serial.println("[storage] /books directory not found");
        return;
    }

    if (library_.paths.empty()) {
        refreshBookPaths();
    }
    if (library_.paths.empty()) {
        Serial.println("[storage] No readable .rsvp, .txt, or .epub books found under /books");
        return;
    }

    BookLibrary::printListing(library_);
}

void StorageManager::refreshBooks(bool includeMetadata) {
    refreshBookPaths(includeMetadata);
}

size_t StorageManager::bookCount() const {
    return library_.paths.size();
}

String StorageManager::bookPath(size_t index) const {
    return BookLibrary::pathAt(library_, index);
}

bool StorageManager::bookIsArticle(size_t index) const {
    return BookLibrary::isArticle(library_, index);
}

String StorageManager::bookDisplayName(size_t index) const {
    return BookLibrary::displayName(library_, index);
}

String StorageManager::bookAuthorName(size_t index) const {
    return BookLibrary::authorName(library_, index);
}

bool StorageManager::loadIndexedBook(size_t index, IndexedBookStore& store, BookMetadata& metadata,
                                     const IndexedBookLoadOptions& options) {
    if (!mounted_) {
        Serial.println("[storage] SD not mounted, cannot load indexed book");
        statusCallback_(statusContext_, "Book open failed", "SD not mounted", "Check card", 100);
        return false;
    }

    IndexedBook::OpenRequest request;
    request.loadedPath = options.loadedPath;
    request.loadedIndex = options.loadedIndex;
    request.allowIndexBuild = options.allowIndexBuild;
    request.allowEpubConversion = options.allowEpubConversion;
    request.statusCallback = statusCallback_;
    request.statusContext = statusContext_;
    return IndexedBook::load(index, library_, store, metadata, request);
}

StorageManager::DiagnosticResult StorageManager::diagnoseSdCard() {
    DiagnosticResult result = SdDiagnostics::diagnoseCard(mounted_, statusCallback_, statusContext_);
    if (!result.booksDirectory || !result.bookFilesDirectory || !result.articleFilesDirectory
        || !result.configDirectory) {
        return result;
    }

    {
        // Refresh the library view used by both diagnostics and the app facade.
        statusCallback_(statusContext_, "SD check", "Scanning /books", "", 45);
        BookLibrary::refresh(library_, true, RSVP_ON_DEVICE_EPUB_CONVERSION);
        result.bookCount = library_.paths.size();
        result.unsupportedCount = BookLibrary::unsupportedFileCount();
    }

    {
        // Probe every required folder before reporting the card as writable.
        SdDiagnostics::probeWritableFolders(result, statusCallback_, statusContext_);
        if (!result.writable || !result.booksWritable || !result.articlesWritable || !result.configWritable) {
            return result;
        }
    }

    if (result.bookCount == 0) {
        result.summary = "No books found";
        if (result.unsupportedCount > 0) {
            result.detail = "Use .rsvp .txt .epub";
        } else {
            result.detail = "Upload to /books/books";
        }
        Serial.printf("[sd-check] no supported books; unsupported=%u\n",
                      static_cast<unsigned int>(result.unsupportedCount));
        return result;
    }

    result.summary = String(result.bookCount) + " books OK";
    result.detail = result.cardType + " " + String(static_cast<unsigned int>(result.sizeMb)) + " MB";
    Serial.printf("[sd-check] OK books=%u unsupported=%u writable=%u\n", static_cast<unsigned int>(result.bookCount),
                  static_cast<unsigned int>(result.unsupportedCount), result.writable ? 1 : 0);
    return result;
}

bool StorageManager::repairSdCardFolders() {
    return SdDiagnostics::repairFolderLayout(mounted_);
}

void StorageManager::refreshBookPaths(bool includeMetadata) {
    if (!mounted_) {
        clearBookCache();
        return;
    }

    statusCallback_(statusContext_, "SD", "Reading library", "", 96);
    BookLibrary::refresh(library_, includeMetadata, RSVP_ON_DEVICE_EPUB_CONVERSION);
}

void StorageManager::clearBookCache() {
    BookLibrary::clear(library_);
}
