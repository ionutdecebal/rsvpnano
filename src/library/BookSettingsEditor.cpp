#include "library/BookSettingsEditor.h"
#include "library/StorageManager.h"
#include "library/ReadingProgress.h"

bool BookSettingsEditor::load(StorageManager& storage, size_t index) {
    clear();
    if (!storage.mounted() || !storage.book(index)) return false;
    title = BookLibrary::displayName(*storage.book(index));
    auto identity = storage.readBookMetadata(index, metadata);
    std::string selectedPath;
    if (identity) {
        selectedPath = storage.book(index)->path;
    } else {
        // An unopened import may need indexing. Never replace the live reader's
        // store/session merely to inspect this book's settings.
        IndexedBookStore temporary;
        if (!storage.loadIndexedBook(index, temporary, metadata)) {
            clear();
            return false;
        }
        identity = temporary.identity();
        selectedPath = temporary.sourcePath();
        temporary.close();
    }
    if (selectedPath.empty() || !identity->sourceSize || !identity->wordCount) {
        clear();
        return false;
    }
    auto saved = ReadingProgress::readBookState(selectedPath, *identity);
    if (saved) {
        overrides = std::move(saved->overrides);
    } else {
        const auto error = saved.error();
        if (error != std::errc::no_such_file_or_directory && error != std::errc::invalid_argument
            && error != std::errc::value_too_large && error != std::errc::state_not_recoverable) {
            clear();
            return false;
        }
    }
    identity_ = *identity;
    path = std::move(selectedPath);
    return true;
}

bool BookSettingsEditor::save() const {
    return ready() && ReadingProgress::writeBookOverrides(path, identity_, overrides).has_value();
}
