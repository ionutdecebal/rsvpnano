#pragma once

#include "library/BookMetadata.h"
#include "reader/ReadingState.h"
#include <string>

class StorageManager;

// A Library selection is independent of the reader's open book and position.
class BookSettingsEditor {
public:
    bool load(StorageManager& storage, size_t index);
    bool save() const;
    void clear() { *this = BookSettingsEditor{}; }
    bool ready() const { return !path.empty(); }

    BookMetadata metadata;
    settings::ReadingOverrides overrides;
    std::string path;
    std::string title;
private:
    reading::BookIdentity identity_;
};
