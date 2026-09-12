#include "ui/screens/ReaderScreen.h"
#include <esp_log.h>

#include <algorithm>
#include <array>
#include <cstdio>

#include "board/BoardPower.h"
#include "library/StorageManager.h"
#include "settings/SettingsRules.h"
#include "storage/migration/Migration.h"
#include "text/UnicodeText.h"
#include "text/Utf8Text.h"
#include "ui/screens/ReaderLayout.h"
#include "ui/screens/Screens.h"

namespace screens {
    namespace {

        using namespace readerLayout;

        constexpr uint16_t kTapSlop = 26;
        constexpr uint16_t kDoubleTapSlop = 92;
        constexpr uint16_t kSwipeThreshold = 40;
        constexpr uint16_t kAxisBias = 12;
        constexpr uint16_t kScrubStep = 22;
        constexpr uint32_t kDoubleTapWindowMs = 520;
        constexpr uint32_t kWpmFeedbackMs = 900;
        constexpr int kMaxScrubSteps = 96;
        constexpr int32_t kParagraphScrollScale = 1'000'000;
        constexpr int32_t kMaximumParagraphRate = 5'000;
        constexpr size_t kPhantomBeforeTargets[] = {64, 96, 144};
        constexpr size_t kPhantomAfterTargets[] = {96, 144, 208};
#if defined(BOARD_HAS_PSRAM)
        constexpr size_t kFontReadAheadWords = ui::fonts::RFontFileCache::kBlockCount;
        constexpr size_t kFontReadAheadTargetBlocks = 12;
        constexpr uint32_t kFontReadAheadMinimumSlackMs = 10;
#endif

        int focusOrdinal(int length) {
            if (length <= 1)
                return 0;
            if (length <= 5)
                return 1;
            if (length <= 9)
                return 2;
            if (length <= 13)
                return 3;
            return 4;
        }

        uint32_t codepointAt(std::string_view text, size_t offset) {
            if (offset >= text.size())
                return 0;
            text.remove_prefix(offset);
            uint32_t codepoint = 0;
            Utf8Text::next(text, codepoint);
            return codepoint;
        }

    } // namespace

    ReaderScreen::ReaderScreen(Arduino_GFX& gfx, settings::ReadingSettings& settings) :
            width_(gfx.width()),
            height_(gfx.height()),
            text_(gfx),
            settings_(settings),
            face_(fonts.loadFace(0, 0)) {}

    void ReaderScreen::begin(const ui::themes::Theme& theme) {
        text_.begin();
        applyTheme(theme);
        refreshTypography();
    }

    void ReaderScreen::applyTheme(const ui::themes::Theme& theme) {
        background_ = theme.definition.colors.background;
        renderedWordIndex_ = SIZE_MAX;
        ++typographyRevision_;
    }

    void ReaderScreen::releaseRuntimeCaches() {
        fonts.clearLoaded();
        store.releaseCache();
        face_ = fonts.loadFace(0, typography_.fontSizeIndex);
        activateFace(face_);

        pageState_ = {};
        rsvpParagraph_ = {};
        rsvpBidi_.clear();
        phantomBidi_.clear();
        BidiText::Line{}.swap(rsvpLine_);
        BidiText::Line{}.swap(phantomLine_);
        std::vector<BidiText::Codepoint>{}.swap(rsvpVisual_);
        std::vector<ui::fonts::PositionedGlyph>{}.swap(rsvpGlyphs_);
        phantoms_ = {};

        loadedWordIndex_ = SIZE_MAX;
        loadedFamilyIndex_ = SIZE_MAX;
        renderedWordIndex_ = SIZE_MAX;
        prefetchedWordIndex_ = SIZE_MAX;
        readAheadWordIndex_ = SIZE_MAX;
        readAheadFamilyIndex_ = SIZE_MAX;
        readAheadBlockCount_ = 0;
    }

    void ReaderScreen::refreshTypography() {
        refreshTypography(settings_, session.state.overrides);
    }

    void ReaderScreen::refreshTypography(const settings::ReadingSettings& settings,
                                         const settings::ReadingOverrides& overrides) {
        if (settings.mode == settings::ReadingMode::page)
            pagePreview_ = false;
        typography_ = settings.typography;
        releaseRuntimeCaches();
        ++typographyRevision_;

        const auto families = fonts.families();
        if (families.size() <= 1 || ReadingLoop::wordCount(session) == 0)
            return;
        std::vector<uint8_t> prepared(families.size());
        std::vector<size_t> firstWords(families.size(), SIZE_MAX);
        const size_t sizeIndex = settings.mode == settings::ReadingMode::page
                                   ? readerLayout::pageStrikeIndex()
                                   : static_cast<size_t>(typography_.fontSizeIndex);
        const auto prepare = [&](size_t wordIndex, std::string_view locale, uint32_t scripts) {
            for (const UnicodeText::ScriptTag& script: UnicodeText::SupportedScripts) {
                if ((scripts & script.mask) == 0)
                    continue;
                const std::string_view requested =
                    settings::fontForText(overrides, locale, script.mask, typography_.fontId);
                const size_t family = FontCatalog::selectFamily(families, requested, locale, script.mask);
                if (family != 0 && !prepared[family]) {
                    fonts.loadFace(family, sizeIndex);
                    prepared[family] = true;
                    firstWords[family] = wordIndex;
                }
            }
        };
        prepare(0, session.metadata.localeAt(0), session.metadata.scriptMaskAt(0));
        for (const BookTextRun& run: session.metadata.textRuns) {
            const std::string_view locale =
                run.locale.empty() ? std::string_view{session.metadata.locale} : std::string_view{run.locale};
            prepare(run.wordIndex, locale, run.scriptMask);
        }

        if (settings.mode != settings::ReadingMode::rsvp)
            return;
        for (size_t family = 1; family < firstWords.size(); ++family) {
            const size_t wordIndex = firstWords[family];
            if (wordIndex == SIZE_MAX || wordIndex >= ReadingLoop::wordCount(session)
                || fontChoice(wordIndex, settings, overrides) != family)
                continue;
            const FontCatalog::Face face = fonts.loadFace(family, sizeIndex);
            activateFace(face);
            const std::string_view word = ReadingLoop::wordAt(session, wordIndex);
            if (!face.shaper) {
                text_.prepare(word);
                const size_t next = wordIndex + 1;
                if (next < ReadingLoop::wordCount(session) && fontChoice(next, settings, overrides) == family)
                    text_.prepare(ReadingLoop::wordAt(session, next));
                continue;
            }
            const ReadingLoop::TextParagraph paragraph = ReadingLoop::paragraphAt(session, wordIndex);
            const size_t localWord = wordIndex - paragraph.firstWord;
            if (localWord >= paragraph.wordOffsets.size())
                continue;
            rsvpGlyphs_.clear();
            const size_t offset = paragraph.wordOffsets[localWord];
            const bool rightToLeft = session.metadata.directionAt(wordIndex) == TextDirection::rtl;
            if (face.shaper->shape(paragraph.text, offset, word.size(), rightToLeft,
                                   session.metadata.localeAt(wordIndex), text_, rsvpGlyphs_))
                text_.prepare(std::span<const ui::fonts::PositionedGlyph>{rsvpGlyphs_});
        }
        rsvpGlyphs_.clear();

#if defined(BOARD_HAS_PSRAM)
        if (session.metadata.writingMode == WritingMode::verticalRl) {
            const size_t current = session.state.wordIndex;
            const size_t family = fontChoice(current, settings, overrides);
            const FontCatalog::Face currentFace = fonts.loadFace(family, sizeIndex);
            const auto bounds = ReadingLoop::paragraphBoundsAt(session, current);
            size_t loadedBlocks = 0;
            for (size_t index = current; index < bounds.second && loadedBlocks < ui::fonts::RFontFileCache::kBlockCount;
                 ++index) {
                if (fontChoice(index, settings, overrides) != family)
                    break;
                loadedBlocks +=
                    ui::fonts::prefetchGlyphBitmaps(currentFace.raster.get(), ReadingLoop::wordAt(session, index),
                                                    ui::fonts::RFontFileCache::kBlockCount - loadedBlocks, true);
            }
            activateFace(currentFace);
        }
#endif
    }

    size_t ReaderScreen::fontChoice(size_t wordIndex) const {
        return fontChoice(wordIndex, settings_, session.state.overrides);
    }

    size_t ReaderScreen::fontChoice(size_t wordIndex, const settings::ReadingSettings& settings,
                                    const settings::ReadingOverrides& overrides) const {
        const std::string_view word = ReadingLoop::wordAt(session, wordIndex);
        const std::string_view locale = session.metadata.localeAt(wordIndex);
        const uint32_t requiredScripts = UnicodeText::scriptsIn(word);
        const std::string_view requested =
            settings::fontForText(overrides, locale, requiredScripts, settings.typography.fontId);
        const auto families = fonts.families();
        if (families.empty())
            return 0;
        return FontCatalog::selectFamily(families, requested, locale, requiredScripts);
    }

    void ReaderScreen::activateFace(const FontCatalog::Face& face) {
        text_.setFont(face.raster.get());
    }

    void ReaderScreen::refreshTypeface() {
        if (loadedWordIndex_ == session.state.wordIndex && loadedFontSizeIndex_ == typography_.fontSizeIndex) {
            activateFace(face_);
            return;
        }
        loadedWordIndex_ = session.state.wordIndex;
        const size_t family = fontChoice(session.state.wordIndex);
        if (loadedFamilyIndex_ != family || loadedFontSizeIndex_ != typography_.fontSizeIndex) {
            loadedFamilyIndex_ = family;
            loadedFontSizeIndex_ = typography_.fontSizeIndex;
            ++fontRevision_;
        }
        face_ = fonts.loadFace(family, typography_.fontSizeIndex);
        activateFace(face_);
    }

    void ReaderScreen::prefetchUpcomingFont(uint32_t nowMs) {
#if defined(BOARD_HAS_PSRAM)
        if (!session.playing || readAheadWordIndex_ == session.state.wordIndex)
            return;
        readAheadWordIndex_ = session.state.wordIndex;

        const size_t currentFamily = fontChoice(session.state.wordIndex);
        if (readAheadFamilyIndex_ == currentFamily) {
            readAheadFamilyIndex_ = SIZE_MAX;
            readAheadBlockCount_ = 0;
        }

        const uint32_t baseWordDurationMs = 60'000UL / settings_.wpm;
        const auto hasSlack = [&](uint32_t currentMs) {
            return currentMs - session.lastAdvanceMs + kFontReadAheadMinimumSlackMs < baseWordDurationMs;
        };
        if (!hasSlack(nowMs))
            return;

        const size_t wordCount = ReadingLoop::wordCount(session);
        const size_t end = std::min<size_t>(wordCount, session.state.wordIndex + 1 + kFontReadAheadWords);
        for (size_t first = session.state.wordIndex + 1; first < end; ++first) {
            const size_t family = fontChoice(first);
            if (family == currentFamily)
                continue;
            const FontCatalog::Face upcoming = fonts.loadFace(family, typography_.fontSizeIndex);
            if (upcoming.shaper)
                return;
            if (upcoming.raster.get().bitmap != nullptr)
                continue;

            if (readAheadFamilyIndex_ != family) {
                readAheadFamilyIndex_ = family;
                readAheadBlockCount_ = 0;
            }
            for (size_t index = first;
                 index < end && readAheadBlockCount_ < kFontReadAheadTargetBlocks && hasSlack(millis()); ++index) {
                if (fontChoice(index) != family)
                    break;
                readAheadBlockCount_ +=
                    ui::fonts::prefetchGlyphBitmaps(upcoming.raster.get(), ReadingLoop::wordAt(session, index), 1,
                                                    session.metadata.writingMode == WritingMode::verticalRl);
            }
            return;
        }
#endif
    }

    void ReaderScreen::prefetchNextWord(uint32_t nowMs) {
        if (settings_.mode == settings::ReadingMode::page || pagePreview_
            || renderedWordIndex_ != session.state.wordIndex)
            return;
        prefetchUpcomingFont(nowMs);
        const size_t next = session.state.wordIndex + 1;
        if (next == prefetchedWordIndex_ || next >= ReadingLoop::wordCount(session))
            return;
        prefetchedWordIndex_ = next;
        const size_t nextFamily = fontChoice(next);
        const bool changingFamily = nextFamily != loadedFamilyIndex_;
#if defined(BOARD_HAS_PSRAM)
        const std::string_view nextWord = ReadingLoop::wordAt(session, next);
        if (changingFamily && UnicodeText::isCjkText(nextWord)) {
            const FontCatalog::Face nextFace = fonts.loadFace(nextFamily, typography_.fontSizeIndex);
            constexpr size_t blockBudget = kFontReadAheadTargetBlocks / 8;
            size_t loadedBlocks = 0;
            const auto bounds = ReadingLoop::paragraphBoundsAt(session, next);
            for (size_t index = next; index < bounds.second && loadedBlocks < blockBudget; ++index) {
                if (fontChoice(index) != nextFamily)
                    break;
                loadedBlocks +=
                    ui::fonts::prefetchGlyphBitmaps(nextFace.raster.get(), ReadingLoop::wordAt(session, index),
                                                    blockBudget - loadedBlocks,
                                                    session.metadata.writingMode == WritingMode::verticalRl);
            }
            return;
        }

        TextShaping::Shaper* nextShaper = nullptr;
        if (changingFamily) {
            const FontCatalog::Face nextFace = fonts.loadFace(nextFamily, typography_.fontSizeIndex);
            nextShaper = nextFace.shaper;
            if (nextShaper)
                activateFace(nextFace);
        }
        if (nextShaper) {
            const ReadingLoop::TextParagraph paragraph = ReadingLoop::paragraphAt(session, next);
            const size_t localWord = next - paragraph.firstWord;
            if (localWord < paragraph.wordOffsets.size()) {
                rsvpGlyphs_.clear();
                const size_t offset = paragraph.wordOffsets[localWord];
                const bool rightToLeft = session.metadata.directionAt(next) == TextDirection::rtl;
                if (nextShaper->shape(paragraph.text, offset, nextWord.size(), rightToLeft,
                                      session.metadata.localeAt(next), text_, rsvpGlyphs_))
                    text_.prepare(std::span<const ui::fonts::PositionedGlyph>{rsvpGlyphs_});
                rsvpGlyphs_.clear();

                const size_t after = next + 1;
                const size_t localAfter = localWord + 1;
                if (settings_.phantomWords && after < paragraph.lastWord && localAfter < paragraph.wordOffsets.size()
                    && fontChoice(after) == nextFamily
                    && session.metadata.directionAt(after) == session.metadata.directionAt(next)) {
                    const std::string_view afterWord = ReadingLoop::wordAt(session, after);
                    const size_t afterOffset = paragraph.wordOffsets[localAfter];
                    if (nextShaper->shape(paragraph.text, afterOffset, afterWord.size(), rightToLeft,
                                          session.metadata.localeAt(after), text_, rsvpGlyphs_))
                        text_.prepare(std::span<const ui::fonts::PositionedGlyph>{rsvpGlyphs_});
                    rsvpGlyphs_.clear();
                }
            }
            return;
        }
#endif
        if (changingFamily || face_.shaper || face_.raster.get().bitmap != nullptr)
            return;
        activateFace(face_);
        if (session.metadata.writingMode == WritingMode::verticalRl) {
#if defined(BOARD_HAS_PSRAM)
            constexpr size_t blockBudget = 4;
            size_t loadedBlocks = 0;
            const auto bounds = ReadingLoop::paragraphBoundsAt(session, next);
            for (size_t index = next; index < bounds.second && loadedBlocks < blockBudget; ++index) {
                if (fontChoice(index) != nextFamily)
                    break;
                loadedBlocks += ui::fonts::prefetchGlyphBitmaps(face_.raster.get(), ReadingLoop::wordAt(session, index),
                                                                blockBudget - loadedBlocks, true);
            }
#endif
            text_.prepareVertical(ReadingLoop::wordAt(session, next));
        } else {
            text_.prepare(ReadingLoop::wordAt(session, next));
        }
    }

    FontCatalog::Face ReaderScreen::pageTypeface(size_t wordIndex) {
        return fonts.loadFace(fontChoice(wordIndex), readerLayout::pageStrikeIndex());
    }

    bool ReaderScreen::openBook(ui::Context& ui, StorageManager& storage, Preferences& preferences, size_t index,
                                uint32_t nowMs) {
        if (!storage.mounted() || index >= storage.books().size())
            return false;
        const BookLibrary::Entry& book = storage.books()[index];
        status(ui, ui.text(UiText::OpeningBook), BookLibrary::displayName(book), {}, 5);
        prepareBookOpen(preferences, nowMs);
        if (!storage.loadIndexedBook(index, store, session.metadata)) {
            status(ui, ui.text(UiText::BookFailed), BookLibrary::displayName(book), ui.text(UiText::CheckSdCard));
            return false;
        }
        finishBookOpen(preferences, nowMs);
        refreshTypography();
        return true;
    }

    void ReaderScreen::closeBook() {
        ReadingProgress::mirror(session, store);
        store.close();
        session = {};
        pagePreview_ = false;
        refreshTypography();
    }

    void ReaderScreen::prepareBookOpen(Preferences& preferences, uint32_t nowMs) {
        ReadingProgress::save(session, preferences, true, nowMs);
        ReadingProgress::mirror(session, store);
        store.close();
        session.metadata.clear();
    }

    void ReaderScreen::finishBookOpen(Preferences& preferences, uint32_t nowMs) {
        session.state = {};
        pagePreview_ = false;
        session.lastSavedWordIndex = static_cast<size_t>(-1);
        ReadingLoop::setBookStore(session, store, nowMs);

        const uint32_t savedWord = ReadingProgress::restore(session, store);
        if (savedWord != ReadingProgress::kNoSavedWordIndex) {
            ReadingLoop::seekTo(session, savedWord);
            ReadingProgress::cache(session, preferences, static_cast<uint32_t>(session.state.wordIndex));
        } else {
            preferences.putString("book", session.sourcePath().data());
        }
    }

    void ReaderScreen::loadInitialBook(ui::Context& ui, StorageManager& storage, Preferences& preferences,
                                       uint32_t nowMs) {
        storage.refreshBooks();
        const std::string savedPath = StorageMigration::currentPath(preferences.getString("book").c_str());
        if (!savedPath.empty()) {
            const int savedBook = storage.findBook(savedPath);
            if (savedBook >= 0 && openBook(ui, storage, preferences, static_cast<size_t>(savedBook), nowMs))
                return;
            ESP_LOGE("reader", "saved book not found: %s", savedPath.c_str());
        }
        if (!storage.books().empty() && openBook(ui, storage, preferences, 0, nowMs))
            return;
        session.metadata.clear();
        session.state = {};
        pagePreview_ = false;
        refreshTypography();
        ReadingLoop::begin(session, nowMs);
    }

    void ReaderScreen::draw(ui::Context& ui, const StorageManager& storage, const Board::Power::BatteryState& battery,
                            uint32_t nowMs) {
        width_ = ui.width();
        height_ = ui.height();
        if (touchIntent_ == TouchIntent::Scrub || touchIntent_ == TouchIntent::Paragraph) {
            const auto typeface = [this](size_t index) {
                return pageTypeface(index);
            };
            const bool vertical = session.metadata.writingMode == WritingMode::verticalRl;
            PageReader::draw(pageState_, ui, text_, typeface, typography_, typographyRevision_, session,
                             readerLayout::readingArea(ui.width(), ui.height(), vertical), {});
            return;
        }
        const std::string_view bookTitle = ReadingProgress::title(session, storage);
        const bool reading = session.playing;
        const settings::ReadingSettings& settings = settings_;
        const bool pageView = settings.mode == settings::ReadingMode::page || pagePreview_;
        const bool vertical = session.metadata.writingMode == WritingMode::verticalRl;
        if (!pageView)
            refreshTypeface();
        const auto nextChapter =
            std::ranges::upper_bound(session.metadata.chapters, session.state.wordIndex, {}, &ChapterMarker::wordIndex);
        const ChapterMarker* chapter =
            nextChapter == session.metadata.chapters.begin() ? nullptr : &*std::prev(nextChapter);
        if (chapter != nullptr && session.state.wordIndex == chapter->wordIndex + 1
            && nextChapter != session.metadata.chapters.end())
            ui.prepareTextFont(nextChapter->title, session.metadata.localeAt(nextChapter->wordIndex));
        const std::string_view chapterLabel = chapter != nullptr && !chapter->title.empty()
                                                ? std::string_view{chapter->title}
                                                : std::string_view{bookTitle};
        const uint8_t progress = ReadingProgress::percent(session.state.wordIndex, ReadingLoop::wordCount(session));
        std::string footer;
        if (reading || settings.footerMetric == settings::FooterMetric::percentage) {
            footer = readerLayout::progressText(ui, settings::FooterMetric::percentage, progress, 0);
        } else {
            size_t remainingWords = ReadingLoop::wordCount(session) > session.state.wordIndex
                                      ? ReadingLoop::wordCount(session) - session.state.wordIndex
                                      : 0;
            if (settings.footerMetric == settings::FooterMetric::chapterTime) {
                const auto next = std::ranges::upper_bound(session.metadata.chapters, session.state.wordIndex, {},
                                                           &ChapterMarker::wordIndex);
                if (next != session.metadata.chapters.end())
                    remainingWords = next->wordIndex - session.state.wordIndex;
            }
            const uint32_t minutes = static_cast<uint32_t>((remainingWords + settings.wpm - 1) / settings.wpm);
            footer = readerLayout::progressText(ui, settings.footerMetric, progress, minutes);
        }
        const bool cjkPacing = ReadingLoop::pacingMode(session) == settings::ReadingPacing::cjkPhrase;
        const bool overlayVisible = wpmFeedbackUntilMs_ > nowMs;

        const ui::Rect readingArea = ui.paintBounds(readerLayout::readingArea(ui.width(), ui.height(), vertical));
        if (pageView) {
            const std::string overlay =
                vertical || !overlayVisible ? "" : std::to_string(settings.wpm) + (cjkPacing ? " CPM" : " WPM");
            const auto typeface = [this](size_t wordIndex) -> FontCatalog::Face {
                return pageTypeface(wordIndex);
            };
            PageReader::draw(pageState_, ui, text_, typeface, typography_, typographyRevision_, session, readingArea,
                             overlay);
        } else if (ui.redraw(readingArea, frameSignature(session.currentWord, overlayVisible, cjkPacing, settings),
                             true)) {
            const std::string overlay =
                overlayVisible ? std::to_string(settings.wpm) + (cjkPacing ? " CPM" : " WPM") : "";
            background_ = ui.color(ui::themes::ColorRole::Background);
            activateFace(face_);
            text_.setTextColor(ui.color(ui::themes::ColorRole::Foreground),
                               ui.color(ui::themes::ColorRole::Background));

            const std::string& word = session.currentWord;
            const std::string before = settings.phantomWords ? phantomBefore(session, typography_.fontSizeIndex) : "";
            const std::string after = settings.phantomWords ? phantomAfter(session, typography_.fontSizeIndex) : "";
            const bool bidi = session.metadata.requiresBidi(session.state.wordIndex, session.state.wordIndex + 1);
            const bool shaping = face_.shaper != nullptr;
            size_t wordOffset = 0;
            bool shaped = false;
            int32_t shapedWidth = 0;
            if (bidi || shaping) {
                if (session.state.wordIndex < rsvpParagraph_.firstWord
                    || session.state.wordIndex >= rsvpParagraph_.lastWord) {
                    rsvpParagraph_ = ReadingLoop::paragraphAt(session, session.state.wordIndex);
                    rsvpBidi_.clear();
                    if (bidi) {
                        const auto analyzed = rsvpBidi_.reset(rsvpParagraph_.text,
                                                              session.metadata.directionAt(rsvpParagraph_.firstWord));
                        if (!analyzed)
                            ESP_LOGW("reader", "bidi analysis failed: %s", analyzed.error().c_str());
                    }
                }
                const size_t localWord = session.state.wordIndex - rsvpParagraph_.firstWord;
                wordOffset = rsvpParagraph_.wordOffsets[localWord];

                rsvpLine_.clear();
                if (bidi) {
                    if (const auto direction = rsvpBidi_.uniformRightToLeft(wordOffset, word.size())) {
                        rsvpLine_.push_back({wordOffset, word.size(), *direction});
                    } else {
                        if (auto resolved = rsvpBidi_.resolve({wordOffset, word.size()}, rsvpLine_); !resolved) {
                            ESP_LOGW("reader", "bidi layout failed: %s", resolved.error().c_str());
                            rsvpLine_.push_back({wordOffset, word.size(), false});
                        }
                    }
                } else {
                    rsvpLine_.push_back({wordOffset, word.size(), false});
                }

                rsvpGlyphs_.clear();
                if (shaping) {
                    const std::string_view locale = session.metadata.localeAt(session.state.wordIndex);
                    if (!bidi) {
                        const auto result = face_.shaper->shape(rsvpParagraph_.text, wordOffset, word.size(), false,
                                                                locale, text_, rsvpGlyphs_);
                        shaped = result.has_value();
                        if (result)
                            shapedWidth = *result;
                        if (!result)
                            ESP_LOGW("reader", "shaping failed: %s", result.error().c_str());
                    } else {
                        shaped = true;
                        for (const BidiText::Run& run: rsvpLine_) {
                            const auto result = face_.shaper->shape(rsvpParagraph_.text, run.offset, run.length,
                                                                    run.rightToLeft, locale, text_, rsvpGlyphs_);
                            if (!result) {
                                ESP_LOGW("reader", "shaping failed: %s", result.error().c_str());
                                shaped = false;
                                break;
                            }
                            shapedWidth += *result;
                        }
                    }
                    if (!shaped)
                        rsvpGlyphs_.clear();
                }
                if (!shaped)
                    BidiText::visualCodepoints(rsvpParagraph_.text, rsvpLine_, rsvpVisual_);
            }
            const int focus = focusOffset(word);
            uint32_t shapedFocusCluster = UINT32_MAX;
            const int16_t wordWidth = shaped ? static_cast<int16_t>(std::clamp<int32_t>(shapedWidth, 0, INT16_MAX))
                                    : bidi   ? wordAdvance(rsvpVisual_)
                                             : text_.textAdvance(word, typography_.tracking);
            int16_t focusCenter = wordWidth / 2;
            if (focus >= 0) {
                int16_t cursor = 0;
                if (shaped) {
                    const uint32_t target = static_cast<uint32_t>(wordOffset + focus);
                    for (const auto& glyph: rsvpGlyphs_) {
                        if (glyph.cluster <= target
                            && (shapedFocusCluster == UINT32_MAX || glyph.cluster > shapedFocusCluster))
                            shapedFocusCluster = glyph.cluster;
                    }
                    for (const auto& glyph: rsvpGlyphs_) {
                        if (glyph.cluster == shapedFocusCluster) {
                            focusCenter = static_cast<int16_t>(cursor + glyph.xOffset + glyph.xAdvance / 2);
                            break;
                        }
                        cursor = static_cast<int16_t>(cursor + glyph.xAdvance);
                    }
                } else if (bidi) {
                    uint32_t previous = 0;
                    bool previousValid = false;
                    for (const BidiText::Codepoint& codepoint: rsvpVisual_) {
                        if (previousValid && !codepoint.rightToLeft)
                            cursor = static_cast<int16_t>(cursor + text_.kerningAdjust(previous, codepoint.value));
                        const int16_t advance = text_.glyphAdvance(codepoint.value);
                        if (codepoint.offset == wordOffset + static_cast<size_t>(focus)) {
                            focusCenter = static_cast<int16_t>(cursor + advance / 2);
                            break;
                        }
                        cursor = static_cast<int16_t>(cursor + advance + typography_.tracking);
                        previous = codepoint.value;
                        previousValid = true;
                    }
                } else {
                    size_t offset = 0;
                    std::string_view remaining = word;
                    while (!remaining.empty()) {
                        const size_t bytes = remaining.size();
                        uint32_t codepoint = 0;
                        Utf8Text::next(remaining, codepoint);
                        const int16_t advance = text_.glyphAdvance(codepoint);
                        if (offset == static_cast<size_t>(focus)) {
                            focusCenter = static_cast<int16_t>(cursor + advance / 2);
                            break;
                        }
                        cursor = static_cast<int16_t>(cursor + advance + typography_.tracking);
                        offset += bytes - remaining.size();
                    }
                }
            }
            const int16_t anchor = static_cast<int16_t>((ui.width() * typography_.anchor) / 100);
            const int16_t x = static_cast<int16_t>(anchor - focusCenter);
            const int16_t inkTop = face_.raster.get().wordInkTop;
            const int16_t inkBottom = face_.raster.get().wordInkBottom;
            const int16_t baseline = static_cast<int16_t>(((ui.height() - (inkBottom - inkTop + 1)) / 2) - inkTop);
            if (vertical) {
                if (shaped)
                    text_.prepare(std::span<const ui::fonts::PositionedGlyph>{rsvpGlyphs_});
                text_.prepareVertical(word);
            } else if (!shaped) {
                text_.prepare(word);
            }
            const bool rightToLeft = bidi && rsvpBidi_.rightToLeft();
            preparePhantom(phantoms_[0], before, rightToLeft);
            preparePhantom(phantoms_[1], after, rightToLeft);
            const auto arrows = vertical ? ui::TextLayout{} : readerLayout::prepareArrows(ui, settings, reading);
            ui.paint(readingArea, [&](Arduino_GFX& gfx, ui::Rect translated) {
                Arduino_GFX& previousOutput = text_.setOutput(gfx);
                const int16_t dx = static_cast<int16_t>(translated.x - readingArea.x);
                const int16_t dy = static_cast<int16_t>(translated.y - readingArea.y);
                const int16_t wordX = static_cast<int16_t>(x + dx);
                const int16_t wordBaseline = static_cast<int16_t>(baseline + dy);
                const int16_t centerY = static_cast<int16_t>(ui.height() / 2 + dy);
                drawGuides(ui, gfx, static_cast<int16_t>(anchor + dx), wordBaseline);
                if (shaped) {
                    int16_t cursor = wordX;
                    size_t first = 0;
                    while (first < rsvpGlyphs_.size()) {
                        const bool highlighted =
                            typography_.focusHighlight && rsvpGlyphs_[first].cluster == shapedFocusCluster;
                        size_t last = first + 1;
                        while (last < rsvpGlyphs_.size()
                               && (typography_.focusHighlight && rsvpGlyphs_[last].cluster == shapedFocusCluster)
                                      == highlighted)
                            ++last;
                        text_.setTextColor(ui.color(highlighted ? ui::themes::ColorRole::Accent
                                                                : ui::themes::ColorRole::Foreground),
                                           background_);
                        if (vertical) {
                            for (size_t index = first; index < last; ++index) {
                                const auto& glyph = rsvpGlyphs_[index];
                                cursor = static_cast<int16_t>(cursor
                                                              + text_.drawVerticalGlyph(glyph,
                                                                                        codepointAt(rsvpParagraph_.text,
                                                                                                    glyph.cluster),
                                                                                        cursor, centerY));
                            }
                        } else {
                            cursor =
                                static_cast<int16_t>(cursor
                                                     + text_.drawGlyphs(std::span{rsvpGlyphs_}.subspan(first,
                                                                                                       last - first),
                                                                        cursor, wordBaseline));
                        }
                        first = last;
                    }
                } else if (bidi)
                    drawWord(rsvpVisual_, wordX, wordBaseline, wordOffset, centerY, focus, vertical, ui);
                else
                    drawWord(word, wordX, wordBaseline, centerY, focus, vertical, ui);
                if (!before.empty())
                    drawPhantom(phantoms_[0], before,
                                rightToLeft ? static_cast<int16_t>(wordX + wordWidth + 24)
                                            : static_cast<int16_t>(wordX - 24),
                                !rightToLeft, wordBaseline, centerY, vertical, ui);
                if (!after.empty())
                    drawPhantom(phantoms_[1], after,
                                rightToLeft ? static_cast<int16_t>(wordX - 24)
                                            : static_cast<int16_t>(wordX + wordWidth + 24),
                                rightToLeft, wordBaseline, centerY, vertical, ui);

                if (!vertical) {
                    readerLayout::drawArrows(ui, gfx, settings, reading, arrows, inkBottom - inkTop + 13, dx, dy);
                    gfx.setFont(static_cast<const GFXfont*>(nullptr));
                    gfx.setTextWrap(false);
                    gfx.setTextSize(2);
                    gfx.setTextColor(ui.color(ui::themes::ColorRole::Muted));
                    if (!overlay.empty()) {
                        gfx.setTextColor(ui.color(ui::themes::ColorRole::Accent));
                        const int16_t overlayY =
                            std::min<int16_t>(ui.height() - 56, readingArea.y + readingArea.h - 16);
                        gfx.setCursor(static_cast<int16_t>((ui.width() - overlay.size() * 12) / 2 + dx),
                                      static_cast<int16_t>(overlayY + dy));
                        gfx.print(overlay.c_str());
                    }
                }
                text_.setOutput(previousOutput);
            });
        }

        renderedWordIndex_ = pageView ? SIZE_MAX : session.state.wordIndex;

        const std::string batteryLabel = readerLayout::batteryText(settings.batteryLabel, battery);
        const std::string overlay = overlayVisible ? std::to_string(settings.wpm) + (cjkPacing ? " CPM" : " WPM") : "";
        readerLayout::chrome(ui,
                             {
                                 .vertical = vertical,
                                 .chapter = chapterLabel.empty() ? ui.text(UiText::Start) : chapterLabel,
                                 .locale = chapter == nullptr ? session.metadata.locale
                                                              : session.metadata.localeAt(chapter->wordIndex),
                                 .footer = footer,
                                 .batteryLabel = batteryLabel,
                                 .overlay = overlay,
                                 .reading = reading,

                                 .topState = pageView ? ui::Context::signature(overlay)
                                                      : frameSignature(session.currentWord, overlayVisible, cjkPacing,
                                                                       settings),
                                 .bottomState = pageView ? ui::Context::combine(0, settings.leftHanded)
                                                         : frameSignature(session.currentWord, overlayVisible,
                                                                          cjkPacing, settings),
                             },
                             settings, battery);
    }

    bool ReaderScreen::batteryTouched(const ui::Touch& touch) const {
        const ui::Rect rect = session.metadata.writingMode == WritingMode::verticalRl
                                ? ui::rotateClockwise(portraitBatteryRect(), height_)
                                : batteryRect(width_, height_);
        return ui::contains(rect, touch.x, touch.y);
    }

    bool ReaderScreen::batteryTapped(const ui::Touch& touch) const {
        return ui::hasTouch(touch, ui::TouchTap) && batteryTouched(touch);
    }

    bool ReaderScreen::batteryLongPressed(const ui::Touch& touch) const {
        return ui::hasTouch(touch, ui::TouchHold) && batteryTouched(touch);
    }

    bool ReaderScreen::previousSentenceTapped(uint16_t x, uint16_t y) const {
        if (session.metadata.writingMode == WritingMode::verticalRl) {
            const ui::Rect previous =
                ui::rotateClockwise(portraitPreviousRect(height_, width_, settings_.leftHanded), height_);
            return ui::contains(previous, x, y);
        }
        if (ui::contains(batteryRect(width_, height_), x, y))
            return false;
        return ui::contains(readerLayout::previousSentenceRect(width_, height_, settings_.leftHanded,
                                                               settings_.mode == settings::ReadingMode::page
                                                                   || pagePreview_),
                            x, y);
    }

    void ReaderScreen::handleTouch(ui::Context& ui, uint32_t nowMs, Preferences& preferences,
                                   settings::SettingsStore& settingsStore) {
        width_ = ui.width();
        height_ = ui.height();
        const ui::Touch* event = ui.touch();
        if (event == nullptr)
            return;
        const ui::Touch& touch = *event;
        const bool ended = ui::hasTouch(touch, ui::TouchRelease);
        const bool held = ui::hasTouch(touch, ui::TouchHold);

        if (ended && touchIntent_ == TouchIntent::PlayHold) {
            resetTouch();
            requestPause(preferences, nowMs);
            return;
        }
        if (ui::hasTouch(touch, ui::TouchStart)) {
            touching_ = true;
            touchStartX_ = touch.x;
            touchStartY_ = touch.y;
            touchStartWord_ = session.state.wordIndex;
            scrubSteps_ = 0;
            touchIntent_ = TouchIntent::None;
            return;
        }
        if (!touching_)
            return;

        const int deltaX = static_cast<int>(touch.x) - touchStartX_;
        const int deltaY = static_cast<int>(touch.y) - touchStartY_;
        const int absX = std::abs(deltaX);
        const int absY = std::abs(deltaY);
        const bool tapLike = absX <= kTapSlop && absY <= kTapSlop;

        if (touchIntent_ == TouchIntent::None && tapLike && batteryLongPressed(touch)) {
            settings::toggleVisibility(settings_.batteryIconVisibility, session.playing);
            settingsStore.acceptChanges();
            lastTapValid_ = false;
            resetTouch();
            return;
        }
        if (touchIntent_ == TouchIntent::None && ended && tapLike && batteryTapped({ui::TouchTap, touch.x, touch.y})) {
            settings_.batteryLabel = settings::cycleEnum(settings_.batteryLabel);
            settingsStore.acceptChanges();
            lastTapValid_ = false;
            resetTouch();
            return;
        }

        if (session.playing) {
            if (held && tapLike && !playLocked_) {
                resetTouch();
                requestPause(preferences, nowMs);
                return;
            }
            if (!ended)
                return;
            resetTouch();
            if (!tapLike) {
                lastTapValid_ = false;
                return;
            }
            if (previousSentenceTapped(touch.x, touch.y)) {
                lastTapValid_ = false;
                ReadingLoop::rewindSentence(session);
                ReadingLoop::pause(session);
                pauseAtSentenceEndRequested_ = false;
                playLocked_ = false;
                ReadingProgress::save(session, preferences, true, nowMs);
                return;
            }
            if (playLocked_ || pauseAtSentenceEndRequested_) {
                lastTapValid_ = false;
                requestPause(preferences, nowMs);
                return;
            }
            if (doubleTap(touch.x, touch.y, nowMs))
                requestPause(preferences, nowMs);
            return;
        }

        if (touchIntent_ == TouchIntent::None && !ended && held && tapLike && !pagePreview_) {
            lastTapValid_ = false;
            touchIntent_ = TouchIntent::PlayHold;
            start(nowMs, false);
            return;
        }
        if (touchIntent_ == TouchIntent::None) {
            if (absX >= kSwipeThreshold && absX > absY + kAxisBias) {
                lastTapValid_ = false;
                touchIntent_ = TouchIntent::Scrub;
                ui.invalidate();
                pagePreview_ = settings_.mode != settings::ReadingMode::page;
                if (pagePreview_)
                    pageState_.pageStart = SIZE_MAX;
            } else if (absY > absX + kAxisBias && (pagePreview_ || absY >= kSwipeThreshold)) {
                lastTapValid_ = false;
                touchIntent_ = pagePreview_ ? TouchIntent::Paragraph : TouchIntent::Wpm;
                if (touchIntent_ == TouchIntent::Paragraph) {
                    ui.invalidate();
                    paragraphTickMs_ = nowMs;
                    paragraphRemainder_ = 0;
                }
            }
        }
        if (touchIntent_ == TouchIntent::Scrub) {
            const int steps = scrubSteps(deltaX);
            const bool changed = steps != scrubSteps_;
            scrubSteps_ = steps;
            if (changed)
                ReadingLoop::seekRelative(session, touchStartWord_, steps);
            if (ended) {
                resetTouch();
                ui.invalidate();
                ReadingProgress::save(session, preferences, true, nowMs);
            }
            return;
        }
        if (touchIntent_ == TouchIntent::Wpm) {
            if (!ended)
                return;
            resetTouch();
            ReadingLoop::adjustWpm(settings_, deltaY < 0 ? 1 : -1);
            settingsStore.acceptChanges();
            wpmFeedbackUntilMs_ = nowMs + kWpmFeedbackMs;
            return;
        }
        if (touchIntent_ == TouchIntent::Paragraph) {
            if (!ended)
                browseParagraphs(touch.y, nowMs);
            else {
                resetTouch();
                ui.invalidate();
                ReadingProgress::save(session, preferences, true, nowMs);
            }
            return;
        }
        if (!ended)
            return;

        resetTouch();
        if (!tapLike) {
            lastTapValid_ = false;
            return;
        }
        if (pagePreview_) {
            lastTapValid_ = false;
            pagePreview_ = false;
            return;
        }
        const bool footerTapped =
            session.metadata.writingMode == WritingMode::verticalRl
                ? ui::contains(ui::rotateClockwise(portraitFooterRect(height_), height_), touch.x, touch.y)
                : [&] {
                      const uint16_t width = std::min<uint16_t>(220, static_cast<uint16_t>(width_ / 2));
                      return touch.y >= static_cast<uint16_t>(std::max<int16_t>(0, height_ - 40))
                          && (settings_.leftHanded ? touch.x <= width
                                                   : touch.x >= static_cast<uint16_t>(width_ - width));
                  }();
        if (footerTapped) {
            settings_.footerMetric = settings::cycleEnum(settings_.footerMetric);
            settingsStore.acceptChanges();
            lastTapValid_ = false;
            return;
        }
        if (previousSentenceTapped(touch.x, touch.y)) {
            lastTapValid_ = false;
            ReadingLoop::rewindSentence(session);
            pauseAtSentenceEndRequested_ = false;
            playLocked_ = false;
            ReadingProgress::save(session, preferences, true, nowMs);
            return;
        }
        if (doubleTap(touch.x, touch.y, nowMs))
            start(nowMs, true);
    }

    void ReaderScreen::toggle(Preferences& preferences, uint32_t nowMs) {
        if (session.playing)
            requestPause(preferences, nowMs);
        else
            start(nowMs, true);
    }

    void ReaderScreen::update(Preferences& preferences, uint32_t nowMs) {
        if (wpmFeedbackUntilMs_ > 0 && nowMs >= wpmFeedbackUntilMs_) {
            wpmFeedbackUntilMs_ = 0;
        }
        if (shouldFinishPause(nowMs)) {
            finishPause(preferences, nowMs);
            return;
        }
        prefetchNextWord(nowMs);
        nowMs = millis();
        if (!session.playing)
            return;
#if defined(RSVP_BENCHMARK_MODE)
        // Reading benchmarks load indexed fixtures but intentionally exclude persistence I/O.
        ReadingLoop::update(session, settings_, nowMs);
#else
        const size_t previousIndex = session.state.wordIndex;
        if (ReadingLoop::update(session, settings_, nowMs)) {
            ReadingProgress::saveChapterTransition(session, preferences, store, previousIndex, session.state.wordIndex,
                                                   nowMs);
        }
#endif
    }

    void ReaderScreen::start(uint32_t nowMs, bool locked) {
        pagePreview_ = false;
        playLocked_ = locked;
        pauseAtSentenceEndRequested_ = false;
        wpmFeedbackUntilMs_ = 0;
        ReadingLoop::start(session, nowMs);
    }

    void ReaderScreen::requestPause(Preferences& preferences, uint32_t nowMs) {
        if (!session.playing)
            return;
        playLocked_ = false;
        if (settings_.pauseMode == settings::PauseMode::instant) {
            finishPause(preferences, nowMs);
            return;
        }
        pauseAtSentenceEndRequested_ = true;
        if (shouldFinishPause(nowMs)) {
            finishPause(preferences, nowMs);
        }
    }

    bool ReaderScreen::shouldFinishPause(uint32_t nowMs) const {
        if (!session.playing || !pauseAtSentenceEndRequested_)
            return false;
        const uint32_t durationMs = ReadingLoop::currentWordDurationMs(session, settings_);
        return durationMs > 0 && ReadingLoop::elapsedInCurrentWordMs(session, nowMs) >= durationMs
            && (ReadingLoop::currentWordEndsSentence(session) || ReadingLoop::atEnd(session));
    }

    void ReaderScreen::finishPause(Preferences& preferences, uint32_t nowMs) {
        ReadingLoop::pause(session);
        pauseAtSentenceEndRequested_ = false;
        playLocked_ = false;
        ReadingProgress::save(session, preferences, true, nowMs);
    }

    bool ReaderScreen::doubleTap(uint16_t x, uint16_t y, uint32_t nowMs) {
        const bool matched = lastTapValid_ && nowMs - lastTapMs_ <= kDoubleTapWindowMs
                          && std::abs(static_cast<int>(x) - lastTapX_) <= kDoubleTapSlop
                          && std::abs(static_cast<int>(y) - lastTapY_) <= kDoubleTapSlop;
        if (matched) {
            lastTapValid_ = false;
            return true;
        }
        lastTapValid_ = true;
        lastTapMs_ = nowMs;
        lastTapX_ = x;
        lastTapY_ = y;
        return false;
    }

    void ReaderScreen::resetTouch() {
        touching_ = false;
        touchIntent_ = TouchIntent::None;
        scrubSteps_ = 0;
        paragraphTickMs_ = 0;
        paragraphRemainder_ = 0;
    }

    void ReaderScreen::browseParagraphs(uint16_t y, uint32_t nowMs) {
        const uint32_t elapsed = std::min<uint32_t>(nowMs - paragraphTickMs_, 100);
        paragraphTickMs_ = nowMs;
        const int32_t rate = ui::centeredDragRate(y, 0, height_, 28, kMaximumParagraphRate);
        if (rate == 0) {
            paragraphRemainder_ = 0;
            return;
        }
        paragraphRemainder_ += rate * static_cast<int32_t>(elapsed);
        const int steps = paragraphRemainder_ / kParagraphScrollScale;
        paragraphRemainder_ %= kParagraphScrollScale;
        if (steps != 0)
            ReadingLoop::seekParagraph(session, steps);
    }

    int ReaderScreen::scrubSteps(int deltaX) const {
        const int distance = std::abs(deltaX);
        if (distance < kSwipeThreshold)
            return 0;
        const int steps = std::min(1 + (distance - kSwipeThreshold) / kScrubStep, kMaxScrubSteps);
        return deltaX > 0 ? steps : -steps;
    }

    std::string ReaderScreen::phantomBefore(const ReadingSession& reader, uint8_t sizeIndex) const {
        if (ReadingLoop::wordCount(reader) == 0)
            return "";
        const auto bounds = ReadingLoop::paragraphBoundsAt(reader, reader.state.wordIndex);
        const TextDirection direction = reader.metadata.directionAt(reader.state.wordIndex);
        const size_t target = kPhantomBeforeTargets[std::min<size_t>(sizeIndex, 2)];
        size_t start = reader.state.wordIndex;
        size_t characters = 0;
        while (start > bounds.first && characters < target) {
            if (reader.metadata.directionAt(start - 1) != direction || fontChoice(start - 1) != loadedFamilyIndex_)
                break;
            --start;
            characters += ReadingLoop::wordAt(reader, start).length() + (start + 1 < reader.state.wordIndex);
        }
        std::string result;
        result.reserve(characters);
        for (size_t index = start; index < reader.state.wordIndex; ++index) {
            if (!result.empty())
                result += ' ';
            result += ReadingLoop::wordAt(reader, index);
        }
        return result;
    }

    std::string ReaderScreen::phantomAfter(const ReadingSession& reader, uint8_t sizeIndex) const {
        if (reader.state.wordIndex + 1 >= ReadingLoop::wordCount(reader))
            return "";
        const auto bounds = ReadingLoop::paragraphBoundsAt(reader, reader.state.wordIndex);
        const TextDirection direction = reader.metadata.directionAt(reader.state.wordIndex);
        const size_t target = kPhantomAfterTargets[std::min<size_t>(sizeIndex, 2)];
        size_t end = reader.state.wordIndex + 1;
        size_t characters = 0;
        while (end < bounds.second && characters < target) {
            if (reader.metadata.directionAt(end) != direction || fontChoice(end) != loadedFamilyIndex_)
                break;
            characters += ReadingLoop::wordAt(reader, end).length() + (end > reader.state.wordIndex + 1);
            ++end;
        }
        std::string result;
        result.reserve(characters);
        for (size_t index = reader.state.wordIndex + 1; index < end; ++index) {
            if (!result.empty())
                result += ' ';
            result += ReadingLoop::wordAt(reader, index);
        }
        return result;
    }

    int ReaderScreen::focusOffset(std::string_view word) const {
        std::array<size_t, 5> offsets{};
        int characters = 0;
        size_t offset = 0;
        std::string_view remaining = word;
        while (!remaining.empty()) {
            const size_t bytes = remaining.size();
            uint32_t codepoint = 0;
            Utf8Text::next(remaining, codepoint);
            if (UnicodeText::isWordCharacter(codepoint)) {
                if (characters < static_cast<int>(offsets.size()))
                    offsets[characters] = offset;
                ++characters;
            }
            offset += bytes - remaining.size();
        }
        if (characters == 0)
            return word.empty() ? -1 : 0;
        const int target = std::min(focusOrdinal(characters), characters - 1);
        return static_cast<int>(offsets[target]);
    }

    int16_t ReaderScreen::wordAdvance(std::span<const BidiText::Codepoint> word) const {
        int16_t advance = 0;
        uint32_t previous = 0;
        bool previousValid = false;
        for (size_t index = 0; index < word.size(); ++index) {
            const BidiText::Codepoint& codepoint = word[index];
            if (previousValid && !codepoint.rightToLeft)
                advance = static_cast<int16_t>(advance + text_.kerningAdjust(previous, codepoint.value));
            advance = static_cast<int16_t>(advance + text_.glyphAdvance(codepoint.value)
                                           + (index + 1 < word.size() ? typography_.tracking : 0));
            previous = codepoint.value;
            previousValid = true;
        }
        return advance;
    }

    void ReaderScreen::preparePhantom(Phantom& phantom, std::string_view value, bool rightToLeft) {
        phantom.visual.clear();
        phantom.glyphs.clear();
        phantom.width = 0;
        phantom.shaped = false;
        phantom.bidi = false;
        if (value.empty())
            return;

        phantomLine_.clear();
        phantom.bidi = rightToLeft
                    || (UnicodeText::scriptsIn(value) & (UnicodeText::ScriptHebrew | UnicodeText::ScriptArabic)) != 0;
        if (phantom.bidi) {
            const TextDirection direction = rightToLeft ? TextDirection::rtl : TextDirection::ltr;
            if (!phantomBidi_.reset(value, direction) || !phantomBidi_.resolve({0, value.size()}, phantomLine_))
                phantomLine_.assign(1, {0, value.size(), rightToLeft});
        } else {
            phantomLine_.assign(1, {0, value.size(), false});
        }

        phantom.shaped = face_.shaper != nullptr;
        int32_t width = 0;
        if (phantom.shaped) {
            const std::string_view locale = session.metadata.localeAt(session.state.wordIndex);
            for (const BidiText::Run& run: phantomLine_) {
                const auto result =
                    face_.shaper->shape(value, run.offset, run.length, run.rightToLeft, locale, text_, phantom.glyphs);
                if (!result) {
                    phantom.shaped = false;
                    phantom.glyphs.clear();
                    break;
                }
                width += *result;
            }
        }
        if (!phantom.shaped) {
            if (phantom.bidi) {
                BidiText::visualCodepoints(value, phantomLine_, phantom.visual);
                width = wordAdvance(phantom.visual);
            } else {
                width = text_.textAdvance(value, typography_.tracking);
            }
        }

        phantom.width = static_cast<int16_t>(std::clamp<int32_t>(width, 0, INT16_MAX));
    }

    void ReaderScreen::drawPhantom(const Phantom& phantom, std::string_view value, int16_t edge, bool extendsLeft,
                                   int16_t baseline, int16_t centerY, bool vertical, ui::Context& ui) {
        int16_t x = edge;
        if (extendsLeft)
            x = static_cast<int16_t>(x - phantom.width);
        text_.setTextColor(ui.blend(ui::themes::ColorRole::Foreground, 62), background_);
        if (phantom.shaped) {
            if (vertical) {
                for (const auto& glyph: phantom.glyphs)
                    x = static_cast<int16_t>(x
                                             + text_.drawVerticalGlyph(glyph, codepointAt(value, glyph.cluster), x,
                                                                       centerY));
            } else
                text_.drawGlyphs(phantom.glyphs, x, baseline);
        } else if (phantom.bidi) {
            uint32_t previous = 0;
            bool previousValid = false;
            for (const BidiText::Codepoint& codepoint: phantom.visual) {
                if (previousValid && !codepoint.rightToLeft)
                    x = static_cast<int16_t>(x + text_.kerningAdjust(previous, codepoint.value));
                x = static_cast<int16_t>(x
                                         + (vertical ? text_.drawVerticalCodepoint(codepoint.value, x, centerY)
                                                     : text_.drawCodepoint(codepoint.value, x, baseline))
                                         + typography_.tracking);
                previous = codepoint.value;
                previousValid = true;
            }
        } else if (!vertical) {
            text_.drawString(value, x, baseline, typography_.tracking);
        } else {
            text_.drawVerticalString(value, x, centerY, typography_.tracking);
        }
    }

    void ReaderScreen::drawWord(std::string_view word, int16_t x, int16_t baseline, int16_t centerY, int focus,
                                bool vertical, ui::Context& ui) {
        size_t offset = 0;
        while (!word.empty()) {
            const size_t bytes = word.size();
            uint32_t codepoint = 0;
            Utf8Text::next(word, codepoint);
            text_.setTextColor(typography_.focusHighlight && focus >= 0 && offset == static_cast<size_t>(focus)
                                   ? ui.color(ui::themes::ColorRole::Accent)
                                   : ui.color(ui::themes::ColorRole::Foreground),
                               ui.color(ui::themes::ColorRole::Background));
            x = static_cast<int16_t>(x
                                     + (vertical ? text_.drawVerticalCodepoint(codepoint, x, centerY)
                                                 : text_.drawCodepoint(codepoint, x, baseline))
                                     + typography_.tracking);
            offset += bytes - word.size();
        }
    }

    void ReaderScreen::drawWord(std::span<const BidiText::Codepoint> word, int16_t x, int16_t baseline,
                                size_t wordOffset, int16_t centerY, int focus, bool vertical, ui::Context& ui) {
        uint32_t previous = 0;
        bool previousValid = false;
        for (size_t index = 0; index < word.size(); ++index) {
            const BidiText::Codepoint& codepoint = word[index];
            if (previousValid && !codepoint.rightToLeft)
                x = static_cast<int16_t>(x + text_.kerningAdjust(previous, codepoint.value));
            text_.setTextColor(typography_.focusHighlight && focus >= 0
                                       && codepoint.offset == wordOffset + static_cast<size_t>(focus)
                                   ? ui.color(ui::themes::ColorRole::Accent)
                                   : ui.color(ui::themes::ColorRole::Foreground),
                               ui.color(ui::themes::ColorRole::Background));
            x = static_cast<int16_t>(x
                                     + (vertical ? text_.drawVerticalCodepoint(codepoint.value, x, centerY)
                                                 : text_.drawCodepoint(codepoint.value, x, baseline))
                                     + (index + 1 < word.size() ? typography_.tracking : 0));
            previous = codepoint.value;
            previousValid = true;
        }
    }

    void ReaderScreen::drawGuides(ui::Context& ui, Arduino_GFX& gfx, int16_t anchor, int16_t baseline) {
        const int16_t inkTop = face_.raster.get().wordInkTop;
        const int16_t inkBottom = face_.raster.get().wordInkBottom;
        const int16_t guideTop = static_cast<int16_t>(baseline + inkTop - 6);
        const int16_t guideBottom = static_cast<int16_t>(baseline + inkBottom + 6);
        const uint16_t guide = ui.blend(ui::themes::ColorRole::Foreground, 96);
        gfx.drawFastHLine(static_cast<int16_t>(anchor - typography_.guideWidth), guideTop,
                          static_cast<int16_t>(typography_.guideWidth - typography_.guideGap), guide);
        gfx.drawFastHLine(static_cast<int16_t>(anchor + typography_.guideGap), guideTop,
                          static_cast<int16_t>(typography_.guideWidth - typography_.guideGap), guide);
        gfx.drawFastHLine(static_cast<int16_t>(anchor - typography_.guideWidth), guideBottom,
                          static_cast<int16_t>(typography_.guideWidth - typography_.guideGap), guide);
        gfx.drawFastHLine(static_cast<int16_t>(anchor + typography_.guideGap), guideBottom,
                          static_cast<int16_t>(typography_.guideWidth - typography_.guideGap), guide);
        const uint16_t marker = typography_.focusHighlight ? ui.color(ui::themes::ColorRole::Accent) : guide;
        gfx.drawFastVLine(anchor, guideTop, 5, marker);
        gfx.drawFastVLine(anchor, static_cast<int16_t>(guideBottom - 4), 5, marker);
    }

    uint32_t ReaderScreen::frameSignature(std::string_view word, bool overlayVisible, bool cjkPacing,
                                          const settings::ReadingSettings& settings) const {
        uint32_t value = ui::Context::signature(word);
        value = ui::Context::combine(value, static_cast<uint32_t>(session.state.wordIndex));
        value = ui::Context::combine(value, overlayVisible);
        value = ui::Context::combine(value, cjkPacing);
        value = ui::Context::combine(value, settings.wpm);
        value = ui::Context::combine(value, settings.phantomWords);
        value = ui::Context::combine(value, static_cast<uint8_t>(typography_.tracking));
        value = ui::Context::combine(value, typography_.anchor);
        value = ui::Context::combine(value, typography_.guideWidth);
        value = ui::Context::combine(value, typography_.guideGap);
        value = ui::Context::combine(value, typography_.focusHighlight);
        value = ui::Context::combine(value, settings.leftHanded);
        value = ui::Context::combine(value, settings::visible(settings.arrowsVisibility, session.playing));
        value = ui::Context::combine(value, static_cast<uint8_t>(session.metadata.writingMode));
        value = ui::Context::combine(value, fontRevision_);
        return value;
    }

} // namespace screens
