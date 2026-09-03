#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "reader/ReadingLoop.h"
#include "library/StorageManager.h"
#include "library/IndexedBookStore.h"
#include "library/ReadingProgress.h"
#include "settings/SettingsModel.h"
#include "ui/screens/Screens.h"

namespace screens {

    struct LibraryItem {
        const BookLibrary::Entry* book = nullptr;
        std::string chapter;
        uint8_t progress = 0;
    };

    // Library browser with two layouts selected by `settings::LibraryLayout`:
    // the bookshelf of spines on wide displays, or a scrollable list whose rows
    // show the full title, author, current chapter, and progress.
    class LibraryScreen {
    public:
        Action draw(ui::Context& ui, const std::vector<LibraryItem>& items, settings::LibraryLayout layout,
                    uint32_t nowMs, Screen& screen);
        void reset();
        void invalidate();
        const std::vector<LibraryItem>& items(StorageManager& storage, const IndexedBookStore& bookStore,
                                              const ReadingSession& session);
        size_t selectedIndex() const {
            return selectedIndex_;
        }

    private:
        Action drawShelf(ui::Context& ui, const std::vector<LibraryItem>& items, const ui::Rect& content);
        Action drawList(ui::Context& ui, const std::vector<LibraryItem>& items, const ui::Rect& content);

        int32_t centeredOffset(const std::vector<LibraryItem>& items, size_t index, int16_t viewportWidth) const;
        int32_t clampShelfOffset(const std::vector<LibraryItem>& items, int32_t offset, int16_t viewportWidth) const;
        size_t nearest(const std::vector<LibraryItem>& items, int32_t offset, int16_t x, int16_t viewportX) const;
        size_t spineAt(const std::vector<LibraryItem>& items, int32_t offset, const ui::Rect& viewport, uint16_t x,
                       uint16_t y) const;
        int16_t spineHeight(const LibraryItem& item, size_t index) const;
        uint32_t shelfSignature(const std::vector<LibraryItem>& items, size_t current, size_t first,
                                size_t pastLast) const;
        uint32_t listSignature(const std::vector<LibraryItem>& items, size_t first, size_t pastLast) const;

        bool dragging_ = false;
        bool moved_ = false;
        bool revealSelected_ = true;
        uint16_t startX_ = 0;
        uint16_t startY_ = 0;
        int32_t startOffset_ = 0;
        int32_t offset_ = 0;
        size_t selectedIndex_ = 0;
        size_t activeIndex_ = SIZE_MAX;
        std::vector<int16_t> rowHeights_;
        std::vector<int32_t> rowTops_;
        std::vector<LibraryItem> items_;
        bool itemsValid_ = false;
    };

} // namespace screens
