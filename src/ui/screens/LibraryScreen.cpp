#include "ui/screens/LibraryScreen.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <climits>
#include <cstdint>
#include <cstdlib>

#include "logging/Logger.h"
#include "storage/fs/StoragePaths.h"
#include "library/IndexedBook.h"
#include "text/AsciiText.h"
#include "text/Utf8Text.h"
#include "ui/screens/LibraryList.h"
#include "ui/screens/ScreenCommon.h"

namespace screens {
    namespace {

        // Shelf layout.
        constexpr int16_t kDetailHeight = 43;
        constexpr int16_t kDetailGap = 2;
        constexpr int16_t kGap = 5;
        constexpr int16_t kSpineBaseWidth = 29;
        constexpr int16_t kSpineWidthStep = 2;
        constexpr size_t kSpineWidthPeriod = 4;
        constexpr int32_t kDragThreshold = ui::TouchTiming{}.tapMoveTolerancePx;
        constexpr int32_t kSpineCycleWidth =
            kSpineWidthPeriod * (kSpineBaseWidth + kGap)
            + kSpineWidthStep * kSpineWidthPeriod * (kSpineWidthPeriod - 1) / 2;

        // List layout.
        constexpr int16_t kScrollbarWidth = 4;
        constexpr int16_t kScrollbarGap = 6;
        constexpr int16_t kRowInset = 12;
        constexpr int16_t kAccentBarWidth = 3;
        constexpr int16_t kTitleTop = 5;
        constexpr int16_t kDetailLineHeight = 11;
        constexpr int16_t kProgressWidth = 52;
        constexpr int16_t kProgressBarHeight = 2;
        constexpr uint8_t kTitleTextSize = 2;

        constexpr int16_t spineWidth(size_t index) {
            return static_cast<int16_t>(kSpineBaseWidth + kSpineWidthStep * (index % kSpineWidthPeriod));
        }

        constexpr int32_t spineLeft(size_t index) {
            const int32_t cycles = static_cast<int32_t>(index / kSpineWidthPeriod);
            const int32_t remainder = static_cast<int32_t>(index % kSpineWidthPeriod);
            return cycles * kSpineCycleWidth + remainder * (kSpineBaseWidth + kGap)
                 + kSpineWidthStep * remainder * (remainder - 1) / 2;
        }

        static_assert(spineLeft(4) == kSpineCycleWidth);
        static_assert(spineLeft(5) - spineLeft(4) == spineWidth(4) + kGap);

        size_t spineIndexAt(int32_t contentX, size_t count) {
            if (count == 0 || contentX <= 0)
                return 0;
            const size_t cycle = static_cast<size_t>(contentX / kSpineCycleWidth);
            const int32_t withinCycle = contentX % kSpineCycleWidth;
            const size_t within = withinCycle >= spineLeft(3) ? 3
                                : withinCycle >= spineLeft(2) ? 2
                                : withinCycle >= spineLeft(1) ? 1
                                                              : 0;
            return std::min(count - 1, cycle * kSpineWidthPeriod + within);
        }

        size_t firstVisibleSpine(int32_t contentX, size_t count) {
            if (count == 0)
                return 0;
            const size_t index = spineIndexAt(contentX, count);
            return spineLeft(index) + spineWidth(index) <= contentX ? std::min(index + 1, count) : index;
        }

        size_t pastLastVisibleSpine(int32_t contentX, size_t count) {
            if (count == 0)
                return 0;
            const size_t index = spineIndexAt(contentX, count);
            return spineLeft(index) < contentX ? std::min(index + 1, count) : index;
        }

        uint16_t spineColor(size_t index, bool article) {
            constexpr uint16_t books[] = {0x99E3, 0x1AF5, 0x0B6A, 0x7B98, 0x4490, 0xB4CD, 0x9A49, 0x32FA};
            constexpr uint16_t articles[] = {0x8B88, 0x63CF, 0x82A9, 0x536A, 0x6ACF};
            return article ? articles[index % 5] : books[index % 8];
        }

        std::string_view title(const LibraryItem& item) {
            return item.book == nullptr ? std::string_view{} : BookLibrary::displayName(*item.book);
        }

        std::string_view author(const LibraryItem& item) {
            return item.book == nullptr ? std::string_view{} : std::string_view{item.book->author};
        }

        constexpr bool inside(ui::Rect rect, ui::Rect clip) {
            return rect.x >= clip.x && rect.y >= clip.y && rect.x + rect.w <= clip.x + clip.w
                && rect.y + rect.h <= clip.y + clip.h;
        }

        void fillClipped(Arduino_GFX& gfx, ui::Rect rect, ui::Rect clip, uint16_t color) {
            rect = ui::intersection(rect, clip);
            if (rect.w > 0 && rect.h > 0)
                gfx.fillRect(rect.x, rect.y, rect.w, rect.h, color);
        }

        void drawRectClipped(Arduino_GFX& gfx, ui::Rect rect, ui::Rect clip, uint16_t color) {
            fillClipped(gfx, {rect.x, rect.y, rect.w, 1}, clip, color);
            fillClipped(gfx, {rect.x, static_cast<int16_t>(rect.y + rect.h - 1), rect.w, 1}, clip, color);
            fillClipped(gfx, {rect.x, rect.y, 1, rect.h}, clip, color);
            fillClipped(gfx, {static_cast<int16_t>(rect.x + rect.w - 1), rect.y, 1, rect.h}, clip, color);
        }

        void drawSpineTitle(Arduino_GFX& gfx, std::string_view value, int16_t x, int16_t y, int16_t bottom,
                            ui::Rect clip) {
            const auto startsWith = [value](std::string_view prefix) {
                return value.size() >= prefix.size()
                    && std::ranges::equal(value.substr(0, prefix.size()), prefix, {}, AsciiText::toLower,
                                          AsciiText::toLower);
            };
            if (startsWith("the "))
                value.remove_prefix(4);
            else if (startsWith("an "))
                value.remove_prefix(3);
            else if (startsWith("a "))
                value.remove_prefix(2);

            size_t written = 0;
            for (char character: value) {
                if (written == 7 || y + 8 >= bottom)
                    break;
                if (character >= 'a' && character <= 'z')
                    character = static_cast<char>(character - 'a' + 'A');
                if ((character < 'A' || character > 'Z') && (character < '0' || character > '9'))
                    continue;
                if (x >= clip.x && x + 6 <= clip.x + clip.w && y >= clip.y && y + 9 <= clip.y + clip.h) {
                    gfx.setCursor(x, y);
                    gfx.write(static_cast<uint8_t>(character));
                }
                y = static_cast<int16_t>(y + 11);
                ++written;
            }
            if (written == 0) {
                for (char character: std::string_view{"BOOK"}) {
                    if (y + 8 >= bottom)
                        break;
                    if (x >= clip.x && x + 6 <= clip.x + clip.w && y >= clip.y && y + 9 <= clip.y + clip.h) {
                        gfx.setCursor(x, y);
                        gfx.write(static_cast<uint8_t>(character));
                    }
                    y = static_cast<int16_t>(y + 11);
                }
            }
        }

        std::string_view percentText(std::array<char, 5>& buffer, uint8_t percent) {
            const auto [end, error] = std::to_chars(buffer.data(), buffer.data() + buffer.size() - 1, percent);
            const size_t digits = error == std::errc{} ? static_cast<size_t>(end - buffer.data()) : 1;
            buffer[digits] = '%';
            return {buffer.data(), digits + 1};
        }

        // Codepoints per title line at the list's title size.
        size_t titleCapacity(int16_t width) {
            const int16_t cell = std::max<int16_t>(1, ui::Context::textWidth("M", kTitleTextSize));
            return static_cast<size_t>(std::max<int16_t>(0, width) / cell);
        }

        void drawRow(ui::Context& ui, const LibraryItem& item, ui::Rect row, ui::Rect clip, bool active,
                     size_t capacity) {
            Arduino_GFX& gfx = ui.gfx();
            const uint16_t surface =
                ui.color(active ? ui::themes::ColorRole::SurfaceActive : ui::themes::ColorRole::SurfaceMuted);
            fillClipped(gfx, row, clip, surface);
            drawRectClipped(gfx, row, clip, ui.color(ui::themes::ColorRole::Outline));
            if (active)
                fillClipped(gfx,
                            {static_cast<int16_t>(row.x + 3), static_cast<int16_t>(row.y + 4), kAccentBarWidth,
                             static_cast<int16_t>(row.h - 8)},
                            clip, ui.color(ui::themes::ColorRole::Accent));

            const int16_t left = static_cast<int16_t>(row.x + kRowInset);
            const int16_t innerWidth = static_cast<int16_t>(row.w - kRowInset * 2);
            std::vector<std::string_view> lines;
            librarylist::wrapLines(title(item), capacity, &lines);
            const int16_t titleHeight = static_cast<int16_t>(librarylist::kTitleLineHeight * lines.size());
            const ui::Rect titleRect{left, static_cast<int16_t>(row.y + kTitleTop),
                                     static_cast<int16_t>(std::max<int16_t>(0, innerWidth - kProgressWidth - 8)),
                                     titleHeight};
            const ui::Rect progressRect{static_cast<int16_t>(left + innerWidth - kProgressWidth),
                                        static_cast<int16_t>(row.y + kTitleTop), kProgressWidth,
                                        librarylist::kTitleLineHeight};
            const int16_t detailTop = static_cast<int16_t>(titleRect.y + titleHeight + 4);
            const int16_t detailWidth = static_cast<int16_t>(std::max<int16_t>(0, (innerWidth - 8) / 2));
            const ui::Rect authorRect{left, detailTop, detailWidth, kDetailLineHeight};
            const ui::Rect chapterRect{static_cast<int16_t>(left + innerWidth - detailWidth), detailTop, detailWidth,
                                       kDetailLineHeight};
            const ui::Rect barRect{left, static_cast<int16_t>(row.y + row.h - kProgressBarHeight - 3), innerWidth,
                                   kProgressBarHeight};

            // Text has no clipping, so rows sliding in at the viewport edges draw
            // each text block only once it fits completely.
            for (size_t line = 0; line < lines.size(); ++line) {
                const ui::Rect lineRect{titleRect.x,
                                        static_cast<int16_t>(titleRect.y + librarylist::kTitleLineHeight * line),
                                        titleRect.w, librarylist::kTitleLineHeight};
                if (inside(lineRect, clip))
                    ui.drawText(lineRect, lines[line], kTitleTextSize, ui.color(ui::themes::ColorRole::Foreground));
            }
            if (inside(progressRect, clip)) {
                std::array<char, 5> buffer{};
                ui.drawText(progressRect, percentText(buffer, item.progress), 2,
                            ui.color(ui::themes::ColorRole::Accent), ui::TextAlign::Right);
            }
            if (inside(authorRect, clip)) {
                const std::string_view name = author(item);
                ui.drawText(authorRect, name.empty() ? ui.text(UiText::Unknown) : name, 1,
                            ui.color(ui::themes::ColorRole::Muted));
            }
            if (inside(chapterRect, clip) && !item.chapter.empty())
                ui.drawText(chapterRect, item.chapter, 1, ui.color(ui::themes::ColorRole::Muted),
                            ui::TextAlign::Right);
            fillClipped(gfx, barRect, clip, ui.color(ui::themes::ColorRole::ProgressTrack));
            if (item.progress > 0)
                fillClipped(gfx,
                            {barRect.x, barRect.y,
                             static_cast<int16_t>(std::max<int32_t>(2, barRect.w * item.progress / 100)), barRect.h},
                            clip, ui.color(ui::themes::ColorRole::Accent));
        }

    } // namespace

    Action LibraryScreen::draw(ui::Context& ui, const std::vector<LibraryItem>& items, settings::LibraryLayout layout,
                               uint32_t nowMs, Screen& screen) {
        (void)nowMs;
        Action result = Action::None;
        if (const Action action = detail::navigation(ui, Screen::Library, screen); action != Action::None) {
            result = action;
        }
        selectedIndex_ = items.empty() ? 0 : std::min(selectedIndex_, items.size() - 1);
        if (revealSelected_ && activeIndex_ < items.size())
            selectedIndex_ = activeIndex_;

        const ui::Rect content = detail::tabContent(ui);
        // The shelf needs the wide strip display; every other board gets the list.
        const bool wide = ui.width() >= 620 && ui.height() >= 150 && ui.height() <= 240;
        const Action action = layout == settings::LibraryLayout::shelf && wide ? drawShelf(ui, items, content)
                                                                                : drawList(ui, items, content);
        revealSelected_ = false;
        return action != Action::None ? action : result;
    }

    Action LibraryScreen::drawShelf(ui::Context& ui, const std::vector<LibraryItem>& items, const ui::Rect& content) {
        Action result = Action::None;
        const ui::Touch* touch = ui.touch();
        const int16_t detailY = static_cast<int16_t>(content.y + content.h - kDetailHeight);
        const ui::Rect viewport{content.x, content.y, content.w,
                                std::max<int16_t>(0, static_cast<int16_t>(detailY - kDetailGap - content.y))};
        const ui::Rect shelfRect{viewport.x, 0, viewport.w,
                                 static_cast<int16_t>(viewport.y + viewport.h + 2)};
        const ui::Rect detailRect{content.x, detailY, content.w, kDetailHeight};
        const int16_t marker = static_cast<int16_t>(viewport.x + viewport.w / 2);
        if (!dragging_)
            offset_ = centeredOffset(items, selectedIndex_, viewport.w);

        if (touch != nullptr && ui::hasTouch(*touch, ui::TouchStart) && ui::contains(viewport, touch->x, touch->y)) {
            dragging_ = true;
            moved_ = false;
            startX_ = touch->x;
            startY_ = touch->y;
            startOffset_ = offset_;
        }
        if (dragging_ && touch != nullptr && ui::hasTouch(*touch, ui::TouchMove)) {
            const int32_t dx = static_cast<int32_t>(touch->x) - startX_;
            const int32_t dy = static_cast<int32_t>(touch->y) - startY_;
            moved_ = moved_ || std::abs(dx) > kDragThreshold || std::abs(dy) > kDragThreshold;
            if (moved_) {
                offset_ = clampShelfOffset(items, startOffset_ + dx, viewport.w);
                selectedIndex_ = nearest(items, offset_, marker, viewport.x);
            }
        }
        if (dragging_ && touch != nullptr && ui::hasTouch(*touch, ui::TouchRelease)) {
            const int32_t dx = static_cast<int32_t>(touch->x) - startX_;
            const int32_t dy = static_cast<int32_t>(touch->y) - startY_;
            moved_ = moved_ || std::abs(dx) > kDragThreshold || std::abs(dy) > kDragThreshold;
            if (moved_) {
                offset_ = clampShelfOffset(items, startOffset_ + dx, viewport.w);
                selectedIndex_ = nearest(items, offset_, marker, viewport.x);
                offset_ = centeredOffset(items, selectedIndex_, viewport.w);
            } else if (!items.empty() && ui::hasTouch(*touch, ui::TouchTap)
                       && ui::contains(viewport, touch->x, touch->y)) {
                const size_t tapped = spineAt(items, offset_, viewport, touch->x, touch->y);
                if (tapped == items.size()) {
                    result = Action::OpenBook;
                } else {
                    result = Action::OpenBook;
                    selectedIndex_ = tapped;
                    offset_ = centeredOffset(items, tapped, viewport.w);
                }
            }
            dragging_ = false;
        }
        if (touch != nullptr && ui::hasTouch(*touch, ui::TouchTap) && ui::contains(detailRect, touch->x, touch->y)) {
            result = Action::OpenBook;
        }

        if (items.empty()) {
            ui.label(viewport, ui.text(UiText::NoLibraryItems), 2, ui::themes::ColorRole::Muted, ui::TextAlign::Center);
            return result;
        }

        const int32_t contentLeft = -offset_;
        const size_t first = firstVisibleSpine(contentLeft, items.size());
        const size_t pastLast = pastLastVisibleSpine(contentLeft + viewport.w, items.size());
        const bool redrawShelf = ui.redraw(shelfRect, shelfSignature(items, selectedIndex_, first, pastLast));
        if (redrawShelf) {
            Arduino_GFX& gfx = ui.gfx();
            const uint16_t foreground = ui.color(ui::themes::ColorRole::Foreground);
            const uint16_t accent = ui.color(ui::themes::ColorRole::Accent);
            const uint16_t outline = ui.color(ui::themes::ColorRole::Outline);
            gfx.drawFastVLine(marker, viewport.y, viewport.h, ui.color(ui::themes::ColorRole::ProgressTrack));

            int32_t left = spineLeft(first);
            for (size_t index = first; index < pastLast; ++index) {
                const int16_t width = spineWidth(index);
                const int16_t height = spineHeight(items[index], index);
                const int32_t screenX = viewport.x + left + offset_;
                left += width + kGap;
                const int16_t x = static_cast<int16_t>(screenX);
                const bool active = index == selectedIndex_;
                const int16_t y = static_cast<int16_t>(viewport.y + viewport.h - height - (active ? 8 : 0));
                const bool article = items[index].book != nullptr && BookLibrary::isArticle(*items[index].book);
                const uint16_t fill = spineColor(index, article);
                fillClipped(gfx, {x, y, width, height}, shelfRect, fill);
                drawRectClipped(gfx, {x, y, width, height}, shelfRect, foreground);
                if (active)
                    fillClipped(gfx, {x, static_cast<int16_t>(y - 2), width, 2}, shelfRect, accent);
                if (items[index].progress > 0) {
                    const int16_t ribbonX = static_cast<int16_t>(x + width - 9);
                    const int16_t ribbonHeight =
                        std::max<int16_t>(8, static_cast<int16_t>(height * items[index].progress / 100));
                    fillClipped(gfx, {ribbonX, y, 5, ribbonHeight}, shelfRect, 0xDACA);
                    for (int16_t row = 0; row < 3; ++row)
                        fillClipped(gfx,
                                    {static_cast<int16_t>(ribbonX + 2 - row),
                                     static_cast<int16_t>(y + ribbonHeight - 3 + row),
                                     static_cast<int16_t>(row * 2 + 1), 1},
                                    shelfRect, fill);
                }
                gfx.setTextSize(1);
                gfx.setTextColor(0xFF9C);
                drawSpineTitle(gfx, title(items[index]), static_cast<int16_t>(x + width / 2 - 3),
                               static_cast<int16_t>(y + 6), static_cast<int16_t>(y + height), shelfRect);
            }
            gfx.drawFastHLine(viewport.x, static_cast<int16_t>(viewport.y + viewport.h), viewport.w, outline);
            gfx.drawFastHLine(viewport.x, static_cast<int16_t>(viewport.y + viewport.h + 1), viewport.w, outline);
        }

        constexpr int16_t progressWidth = 76;
        constexpr int16_t detailGap = 12;
        const int16_t textWidth = static_cast<int16_t>(detailRect.w - progressWidth - detailGap);
        const LibraryItem& item = items[selectedIndex_];
        const std::string_view itemTitle = title(item);
        const std::string_view itemAuthor = author(item).empty() ? ui.text(UiText::Unknown) : author(item);
        std::array<char, 5> progress{};
        const std::string_view progressLabel = percentText(progress, item.progress);
        uint32_t detailState = ui::Context::signature(itemTitle);
        detailState = ui::Context::signature(itemAuthor, detailState);
        detailState = ui::Context::signature(item.chapter, detailState);
        detailState = ui::Context::combine(detailState, item.progress);
        if (ui.redraw(detailRect, detailState)) {
            ui.drawText({detailRect.x, detailRect.y, textWidth, 18}, itemTitle, 2,
                        ui.color(ui::themes::ColorRole::Foreground));
            ui.drawText({detailRect.x, static_cast<int16_t>(detailRect.y + 20), textWidth, 10}, itemAuthor, 1,
                        ui.color(ui::themes::ColorRole::Muted));
            ui.drawText({detailRect.x, static_cast<int16_t>(detailRect.y + 33), textWidth, 10}, item.chapter, 1,
                        ui.color(ui::themes::ColorRole::Muted));
            ui.drawText({static_cast<int16_t>(detailRect.x + detailRect.w - progressWidth), detailRect.y,
                         progressWidth, detailRect.h},
                        progressLabel, 3, ui.color(ui::themes::ColorRole::Accent), ui::TextAlign::Right);
        }
        return result;
    }

    Action LibraryScreen::drawList(ui::Context& ui, const std::vector<LibraryItem>& items, const ui::Rect& content) {
        using namespace librarylist;
        Action result = Action::None;
        const ui::Touch* touch = ui.touch();
        const ui::Rect viewport{content.x, content.y,
                                static_cast<int16_t>(std::max<int16_t>(0, content.w - kScrollbarWidth - kScrollbarGap)),
                                content.h};
        const ui::Rect track{static_cast<int16_t>(content.x + content.w - kScrollbarWidth), content.y, kScrollbarWidth,
                             content.h};
        const size_t count = items.size();
        const size_t capacity = titleCapacity(static_cast<int16_t>(viewport.w - kRowInset * 2 - kProgressWidth - 8));
        rowHeights_.resize(count);
        for (size_t index = 0; index < count; ++index)
            rowHeights_[index] = rowHeight(wrapLines(title(items[index]), capacity, nullptr));
        layout(rowHeights_, rowTops_);
        const Rows rows{rowTops_, rowHeights_};

        if (revealSelected_)
            offset_ = offsetToReveal(rows, selectedIndex_, offset_, viewport.h);
        offset_ = clampOffset(rows, offset_, viewport.h);

        if (touch != nullptr && ui::hasTouch(*touch, ui::TouchStart) && ui::contains(content, touch->x, touch->y)) {
            dragging_ = true;
            moved_ = false;
            startX_ = touch->x;
            startY_ = touch->y;
            startOffset_ = offset_;
        }
        if (dragging_ && touch != nullptr && ui::hasTouch(*touch, ui::TouchMove)) {
            const int32_t dx = static_cast<int32_t>(touch->x) - startX_;
            const int32_t dy = static_cast<int32_t>(touch->y) - startY_;
            moved_ = moved_ || std::abs(dx) > kDragThreshold || std::abs(dy) > kDragThreshold;
            if (moved_)
                offset_ = clampOffset(rows, startOffset_ - dy, viewport.h);
        }
        if (dragging_ && touch != nullptr && ui::hasTouch(*touch, ui::TouchRelease)) {
            const int32_t dx = static_cast<int32_t>(touch->x) - startX_;
            const int32_t dy = static_cast<int32_t>(touch->y) - startY_;
            moved_ = moved_ || std::abs(dx) > kDragThreshold || std::abs(dy) > kDragThreshold;
            if (moved_) {
                offset_ = clampOffset(rows, startOffset_ - dy, viewport.h);
            } else if (ui::hasTouch(*touch, ui::TouchTap) && ui::contains(viewport, touch->x, touch->y)) {
                const size_t tapped = rowAt(rows, offset_, static_cast<int32_t>(touch->y) - viewport.y);
                if (tapped < count) {
                    selectedIndex_ = tapped;
                    result = Action::OpenBook;
                }
            }
            dragging_ = false;
        }

        if (count == 0) {
            ui.label(content, ui.text(UiText::NoLibraryItems), 2, ui::themes::ColorRole::Muted,
                     ui::TextAlign::Center);
            return result;
        }

        const size_t first = firstVisible(rows, offset_);
        const size_t pastLast = pastLastVisible(rows, offset_, viewport.h);
        if (ui.redraw(content, listSignature(items, first, pastLast))) {
            Arduino_GFX& gfx = ui.gfx();
            for (size_t index = first; index < pastLast; ++index) {
                const ui::Rect row{viewport.x, static_cast<int16_t>(viewport.y + rows.tops[index] - offset_),
                                   viewport.w, rows.heights[index]};
                drawRow(ui, items[index], row, viewport, index == selectedIndex_, capacity);
            }
            if (contentHeight(rows) > viewport.h) {
                const Thumb thumb = scrollThumb(rows, offset_, viewport.h, track.h);
                gfx.fillRect(track.x, track.y, track.w, track.h, ui.color(ui::themes::ColorRole::ProgressTrack));
                gfx.fillRect(track.x, static_cast<int16_t>(track.y + thumb.y), track.w, thumb.h,
                             ui.color(ui::themes::ColorRole::Muted));
            }
        }
        return result;
    }

    void LibraryScreen::reset() {
        dragging_ = false;
        moved_ = false;
        offset_ = 0;
        revealSelected_ = true;
    }

    void LibraryScreen::invalidate() {
        items_.clear();
        itemsValid_ = false;
        activeIndex_ = SIZE_MAX;
    }

    const std::vector<LibraryItem>& LibraryScreen::items(StorageManager& storage, const IndexedBookStore& bookStore,
                                                         const ReadingSession& session) {
        (void)bookStore;
        const size_t bookCount = storage.books().size();
        if (itemsValid_ && items_.size() == bookCount) {
            const int activeIndex = storage.findBook(session.sourcePath());
            if (activeIndex >= 0 && static_cast<size_t>(activeIndex) < items_.size()) {
                activeIndex_ = static_cast<size_t>(activeIndex);
                LibraryItem& current = items_[activeIndex_];
                current.progress = ReadingProgress::percent(session.state.wordIndex, ReadingLoop::wordCount(session));
                if (const ChapterMarker* chapter = session.metadata.chapterAt(session.state.wordIndex)) {
                    current.chapter = chapter->title;
                }
            }
            return items_;
        }

        items_.clear();
        items_.reserve(bookCount);
        activeIndex_ = SIZE_MAX;
        for (size_t index = 0; index < bookCount; ++index) {
            const BookLibrary::Entry& book = storage.books()[index];
            LibraryItem item{.book = &book};

            BookMetadata metadata;
            const auto identity = storage.readBookMetadata(index, metadata);
            uint32_t wordIndex = 0;
            bool hasPosition = false;

            if (session.sourcePath() == book.path) {
                activeIndex_ = index;
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

    int32_t LibraryScreen::centeredOffset(const std::vector<LibraryItem>& items, size_t index,
                                          int16_t viewportWidth) const {
        if (items.empty())
            return 0;
        index = std::min(index, items.size() - 1);
        return clampShelfOffset(items, viewportWidth / 2 - spineLeft(index) - spineWidth(index) / 2, viewportWidth);
    }

    int32_t LibraryScreen::clampShelfOffset(const std::vector<LibraryItem>& items, int32_t offset,
                                            int16_t viewportWidth) const {
        if (items.empty())
            return 0;
        const size_t last = items.size() - 1;
        const int32_t lastCenter = spineLeft(last) + spineWidth(last) / 2;
        const int32_t firstCenter = spineWidth(0) / 2;
        return std::clamp(offset, viewportWidth / 2 - lastCenter, viewportWidth / 2 - firstCenter);
    }

    size_t LibraryScreen::nearest(const std::vector<LibraryItem>& items, int32_t offset, int16_t x,
                                  int16_t viewportX) const {
        if (items.empty())
            return 0;
        const int32_t contentX = static_cast<int32_t>(x) - viewportX - offset;
        const size_t candidate = spineIndexAt(contentX, items.size());
        const size_t first = candidate == 0 ? 0 : candidate - 1;
        const size_t last = std::min(items.size() - 1, candidate + 1);
        size_t result = first;
        int distance = INT_MAX;
        for (size_t index = first; index <= last; ++index) {
            const int center = viewportX + spineLeft(index) + spineWidth(index) / 2 + offset;
            if (std::abs(center - x) < distance) {
                distance = std::abs(center - x);
                result = index;
            }
        }
        return result;
    }

    size_t LibraryScreen::spineAt(const std::vector<LibraryItem>& items, int32_t offset, const ui::Rect& viewport,
                                  uint16_t x, uint16_t y) const {
        if (items.empty())
            return 0;
        const int32_t contentX = static_cast<int32_t>(x) - viewport.x - offset;
        const size_t index = spineIndexAt(contentX, items.size());
        const int16_t width = spineWidth(index);
        const int16_t height = spineHeight(items[index], index);
        const int32_t screenX = viewport.x + spineLeft(index) + offset;
        const int16_t spineY =
            static_cast<int16_t>(viewport.y + viewport.h - height - (index == selectedIndex_ ? 8 : 0));
        if (x >= screenX && x < screenX + width && y >= spineY && y < spineY + height)
            return index;
        return items.size();
    }

    int16_t LibraryScreen::spineHeight(const LibraryItem& item, size_t index) const {
        const bool article = item.book != nullptr && BookLibrary::isArticle(*item.book);
        return std::min<int16_t>(110, static_cast<int16_t>((article ? 78 : 84)
                                                           + std::min<size_t>(title(item).length(), 24) / 2
                                                           + (index * 5) % 17));
    }

    uint32_t LibraryScreen::shelfSignature(const std::vector<LibraryItem>& items, size_t current, size_t first,
                                           size_t pastLast) const {
        uint32_t value = ui::Context::combine(Fnv1a::kOffsetBasis, static_cast<uint32_t>(offset_));
        value = ui::Context::combine(value, static_cast<uint32_t>(current));
        value = ui::Context::combine(value, items.size());
        for (size_t index = first; index < pastLast; ++index) {
            const LibraryItem& item = items[index];
            value = ui::Context::signature(title(item), value);
            value = ui::Context::combine(value, item.progress);
        }
        return value;
    }

    uint32_t LibraryScreen::listSignature(const std::vector<LibraryItem>& items, size_t first, size_t pastLast) const {
        uint32_t value = ui::Context::combine(Fnv1a::kOffsetBasis, static_cast<uint32_t>(offset_));
        value = ui::Context::combine(value, static_cast<uint32_t>(selectedIndex_));
        value = ui::Context::combine(value, items.size());
        for (size_t index = first; index < pastLast; ++index) {
            const LibraryItem& item = items[index];
            value = ui::Context::signature(title(item), value);
            value = ui::Context::signature(author(item), value);
            value = ui::Context::signature(item.chapter, value);
            value = ui::Context::combine(value, item.progress);
        }
        return value;
    }

} // namespace screens
