#include "ui/screens/LibraryScreen.h"

#include <algorithm>

#include "logging/Logger.h"
#include "storage/fs/StoragePaths.h"
#include "library/IndexedBook.h"
#include "ui/screens/ScreenCommon.h"

namespace screens {
    void LibraryScreen::reset() {
        carouselGesture_ = {};
        dragging_ = false;
        moved_ = false;
        offset_ = 0;
    }

    void LibraryScreen::invalidate() {
        items_.clear();
        itemsValid_ = false;
    }

    const std::vector<LibraryItem>& LibraryScreen::items(StorageManager& storage, const IndexedBookStore& bookStore,
                                                         const ReadingSession& session) {
        const size_t bookCount = storage.books().size();
        if (itemsValid_ && items_.size() == bookCount) {
            const int activeIndex = storage.findBook(session.sourcePath());
            if (activeIndex >= 0 && static_cast<size_t>(activeIndex) < items_.size()) {
                LibraryItem& current = items_[static_cast<size_t>(activeIndex)];
                current.progress = ReadingProgress::percent(session.state.wordIndex, ReadingLoop::wordCount(session));
                if (const ChapterMarker* chapter = session.metadata.chapterAt(session.state.wordIndex)) {
                    current.chapter = chapter->title;
                }
            }
            return items_;
        }

        items_.clear();
        items_.reserve(bookCount);
        for (size_t index = 0; index < bookCount; ++index) {
            const BookLibrary::Entry& book = storage.books()[index];
            LibraryItem item{.book = &book};

            BookMetadata metadata;
            const auto identity = storage.readBookMetadata(index, metadata);
            uint32_t wordIndex = 0;
            bool hasPosition = false;

            if (session.sourcePath() == book.path) {
                wordIndex = static_cast<uint32_t>(session.state.wordIndex);
                item.progress = ReadingProgress::percent(wordIndex, ReadingLoop::wordCount(session));
                if (const ChapterMarker* chapter = session.metadata.chapterAt(wordIndex))
                    item.chapter = chapter->title;
            } else if (identity && identity->wordCount > 0) {
                const auto savedWordIndex = ReadingProgress::readBookStatePosition(book.path, *identity);
                if (savedWordIndex) {
                    hasPosition = true;
                    wordIndex = *savedWordIndex;
                } else if (savedWordIndex.error() != std::errc::no_such_file_or_directory
                           && savedWordIndex.error() != std::errc::state_not_recoverable) {
                    Logger::failure("library", "read progress", StoragePaths::bookStatePathFor(book.path).c_str(),
                                    savedWordIndex.error());
                }
                item.progress = hasPosition ? ReadingProgress::percent(wordIndex, identity->wordCount) : 0;
                if (const ChapterMarker* chapter = metadata.chapterAt(hasPosition ? wordIndex : 0)) {
                    item.chapter = chapter->title;
                }
            } else {
                item.progress = 0;
            }

            items_.push_back(std::move(item));
        }
        itemsValid_ = true;
        return items_;
    }

} // namespace screens
