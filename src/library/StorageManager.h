#pragma once

#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <system_error>

#include "library/BookMetadata.h"
#include "library/IndexedBook.h"
#include "library/ReadingProgress.h"
#include "library/BookLibrary.h"

class IndexedBookStore;

class StorageManager {
public:
    using StatusCallback = IndexedBook::StatusCallback;
    void setStatusCallback(StatusCallback callback, void* context);
    bool begin();
    // Retry a failed startup mount without running migration or a book scan.
    bool mountForCheck();
    void end();
    void refreshBooks(bool includeMetadata = true);
    std::expected<void, std::error_code> installBook(std::string_view stagedPath, std::string_view destinationPath);
    std::expected<void, std::error_code> removeBook(std::string_view path);
    bool mounted() const {
        return mounted_;
    }
    bool loadIndexedBook(size_t index, IndexedBookStore& store, BookMetadata& metadata,
                         IndexedBook::OpenRequest request = {});
    std::span<const BookLibrary::Entry> books() const {
        return library_;
    }
    uint32_t bookRevision() const { return bookRevision_; }
    const BookLibrary::Entry* book(size_t index) const;
    int findBook(std::string_view path) const;
    std::optional<reading::BookIdentity> readBookMetadata(size_t index, BookMetadata& metadata);

private:
    static void ignoreStatus(void* context, const char* title, const char* line1, const char* line2,
                             int progressPercent);

    void refreshBookPaths(bool includeMetadata = true);
    void clearBookCache();

    bool mounted_ = false;
    StatusCallback statusCallback_ = &StorageManager::ignoreStatus;
    void* statusContext_ = nullptr;
    BookLibrary::Listing library_;
    uint32_t bookRevision_ = 0;
};
