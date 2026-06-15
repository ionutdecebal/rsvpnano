#pragma once

#include <Arduino.h>

#include "storage/fs/SdDiagnostics.h"
#include "storage/library/BookLibrary.h"

struct BookMetadata;
class IndexedBookStore;

class StorageManager {
public:
    using StatusCallback = void (*)(void* context, const char* title, const char* line1, const char* line2,
                                    int progressPercent);
    using DiagnosticResult = SdDiagnostics::DiagnosticResult;

    struct IndexedBookLoadOptions {
        IndexedBookLoadOptions() :
                loadedPath(nullptr),
                loadedIndex(nullptr),
                allowIndexBuild(true),
                allowEpubConversion(true) {}

        String* loadedPath;
        size_t* loadedIndex;
        bool allowIndexBuild;
        bool allowEpubConversion;
    };

    void setStatusCallback(StatusCallback callback, void* context);
    bool begin();
    void end();
    void listBooks();
    void refreshBooks(bool includeMetadata = true);
    bool loadIndexedBook(size_t index, IndexedBookStore& store, BookMetadata& metadata,
                         const IndexedBookLoadOptions& options = IndexedBookLoadOptions());
    size_t bookCount() const;
    String bookPath(size_t index) const;
    bool bookIsArticle(size_t index) const;
    String bookDisplayName(size_t index) const;
    String bookAuthorName(size_t index) const;
    DiagnosticResult diagnoseSdCard();
    bool repairSdCardFolders();

private:
    static void ignoreStatus(void* context, const char* title, const char* line1, const char* line2,
                             int progressPercent);

    void refreshBookPaths(bool includeMetadata = true);
    void clearBookCache();

    bool mounted_ = false;
    bool listedOnce_ = false;
    StatusCallback statusCallback_ = &StorageManager::ignoreStatus;
    void* statusContext_ = nullptr;
    BookLibrary::Listing library_;
};
