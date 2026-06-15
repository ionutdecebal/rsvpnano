#pragma once

#include <Arduino.h>

#include "book/BookMetadata.h"
#include "storage/index/IndexedBookStore.h"
#include "storage/library/BookLibrary.h"

namespace IndexedBook {

    using StatusCallback =
        void (*)(void* context, const char* title, const char* line1, const char* line2, int progressPercent);

    struct OpenRequest {
        String* loadedPath = nullptr;
        size_t* loadedIndex = nullptr;
        bool allowIndexBuild = true;
        bool allowEpubConversion = true;
        StatusCallback statusCallback = nullptr;
        void* statusContext = nullptr;
    };

    bool load(size_t index,
              BookLibrary::Listing& library,
              IndexedBookStore& store,
              BookMetadata& metadata,
              const OpenRequest& request);

} // namespace IndexedBook
