#include "ui/screens/PageReaderScreen.h"
#include "ui/ClippedGfx.h"

#include <algorithm>
#include <iterator>
#include <cstring>

#if defined(ESP32) && defined(BOARD_HAS_PSRAM)
#include <esp_heap_caps.h>
#endif

#include "reader/ReadingLoop.h"
#include "text/BidiText.h"
#include "text/UnicodeText.h"

namespace screens::PageReader {
    namespace {

        constexpr size_t kInvalidIndex = std::numeric_limits<size_t>::max();
        constexpr size_t kAnchorLeadWords = 12;
        constexpr size_t kParagraphSnapWords = 12;
        constexpr int16_t kMarginX = 14;
        constexpr int16_t kMarginY = 4;
        constexpr uint8_t kOverlayTextSize = 2;
        const int16_t kOverlayTextHeight = ui::Context::textHeight(kOverlayTextSize);
        constexpr int16_t kLineGap = 2;

        void clearPage(State& state) {
            state.lineCount = 0;
            state.faces.clear();
            state.words.clear();
            state.glyphs.clear();
            state.characters.clear();
            state.highlighted = kInvalidIndex;
            state.bidi = false;
        }

        void invalidate(State& state) {
            clearPage(state);
            state.pageStart = kInvalidIndex;
            state.pageEnd = 0;
        }

        bool paragraphStart(const ReadingSession& session, size_t index) {
            return index == 0 || std::ranges::binary_search(session.metadata.paragraphStarts, index);
        }

        size_t anchorIndex(const ReadingSession& session, size_t currentIndex) {
            const size_t anchor = currentIndex > kAnchorLeadWords ? currentIndex - kAnchorLeadWords : 0;
            const auto& starts = session.metadata.paragraphStarts;
            const auto next = std::ranges::upper_bound(starts, anchor);
            if (next == starts.begin())
                return anchor;
            const size_t paragraph = *std::prev(next);
            return anchor - paragraph <= kParagraphSnapWords ? paragraph : anchor;
        }

        void activateFace(ui::fonts::AlphaTextRenderer<640>& text, const FontCatalog::Face& face) {
            text.setFont(face.raster.get());
        }

        bool sameFace(const FontCatalog::Face& left, const FontCatalog::Face& right) {
            return &left.raster.get() == &right.raster.get() && left.shaper == right.shaper;
        }

        uint8_t rememberFace(State& state, const FontCatalog::Face& face) {
            const auto found = std::ranges::find_if(state.faces, [&](const FontCatalog::Face& candidate) {
                return sameFace(candidate, face);
            });
            if (found != state.faces.end())
                return static_cast<uint8_t>(found - state.faces.begin());
            state.faces.push_back(face);
            return static_cast<uint8_t>(state.faces.size() - 1);
        }

        const FontCatalog::Face& faceAt(const State& state, size_t wordIndex) {
            return state.faces[state.words[wordIndex - state.pageStart].faceIndex];
        }

        State::Word& prepareWord(State& state, size_t index, uint8_t faceIndex, ui::fonts::AlphaTextRenderer<640>& text,
                                 const FontCatalog::Face& face, const settings::TypographySettings& typography,
                                 const ReadingSession& session, ReadingLoop::TextParagraph& paragraph,
                                 BidiText::Analysis& bidiAnalysis, bool paragraphBidi, bool bidiReady,
                                 BidiText::Line& bidiLine) {
            const size_t localIndex = index - state.pageStart;
            if (localIndex < state.words.size())
                return state.words[localIndex];

            const std::string_view word = ReadingLoop::wordAt(session, index);
            State::Word prepared{.faceIndex = faceIndex, .cjk = UnicodeText::isCjkText(word)};
            if (face.shaper) {
                const size_t paragraphWord = index - paragraph.firstWord;
                const size_t offset = paragraph.wordOffsets[paragraphWord];
                const std::string_view locale = session.localeAt(index);
                const size_t glyphStart = state.glyphs.size();
                if (glyphStart > UINT16_MAX) {
                    prepared.width = text.textAdvance(word, typography.tracking);
                    state.words.push_back(prepared);
                    return state.words.back();
                }
                prepared.glyphStart = static_cast<uint16_t>(glyphStart);
                bool shaped = false;
                int32_t width = 0;
                if (paragraphBidi && bidiReady) {
                    if (const auto direction = bidiAnalysis.uniformRightToLeft(offset, word.size())) {
                        const auto result = face.shaper->shape(paragraph.text, offset, word.size(), *direction, locale,
                                                               text, state.glyphs);
                        shaped = result.has_value();
                        if (result)
                            width = *result;
                    } else if (bidiAnalysis.resolve({offset, word.size()}, bidiLine)) {
                        shaped = true;
                        for (const BidiText::Run& run: bidiLine) {
                            const auto result = face.shaper->shape(paragraph.text, run.offset, run.length,
                                                                   run.rightToLeft, locale, text, state.glyphs);
                            if (!result) {
                                shaped = false;
                                break;
                            }
                            width += *result;
                        }
                    }
                } else {
                    const auto result =
                        face.shaper->shape(paragraph.text, offset, word.size(), false, locale, text, state.glyphs);
                    shaped = result.has_value();
                    if (result)
                        width = *result;
                }
                const size_t glyphCount = state.glyphs.size() - glyphStart;
                if (shaped && glyphCount > 0 && state.glyphs.size() <= UINT16_MAX) {
                    prepared.glyphCount = static_cast<uint16_t>(glyphCount);
                    prepared.width = static_cast<int16_t>(std::clamp<int32_t>(width, 0, INT16_MAX));
                    prepared.shaped = true;
                } else
                    state.glyphs.resize(glyphStart);
            }
            if (!prepared.shaped)
                prepared.width = text.textAdvance(word, typography.tracking);
            state.words.push_back(prepared);
            return state.words.back();
        }

        const State::Word& wordAt(const State& state, size_t index) {
            return state.words[index - state.pageStart];
        }

        ui::Rect inkRect(const ui::fonts::AlphaTextRenderer<640>::Bounds& bounds) {
            return bounds.w > 0 && bounds.h > 0
                     ? ui::Rect{bounds.x1, bounds.y1, static_cast<int16_t>(bounds.w), static_cast<int16_t>(bounds.h)}
                     : ui::Rect{};
        }

        ui::Rect unionInk(ui::Rect left, ui::Rect right) {
            if (left.w <= 0 || left.h <= 0)
                return right;
            if (right.w <= 0 || right.h <= 0)
                return left;
            const int16_t x = std::min(left.x, right.x), y = std::min(left.y, right.y);
            return {x, y, static_cast<int16_t>(std::max(left.x + left.w, right.x + right.w) - x),
                    static_cast<int16_t>(std::max(left.y + left.h, right.y + right.h) - y)};
        }

        bool visibleInk(ui::Rect ink, bool known, ui::Rect viewport) {
            if (!known)
                return true;
            const auto visible = ui::intersection(ink, viewport);
            return visible.w > 0 && visible.h > 0;
        }

        int16_t lineAdvance(State& state, const State::Line& line, ui::fonts::AlphaTextRenderer<640>& text,
                            const settings::TypographySettings& typography);
        void measurePageInk(State& state, ui::fonts::AlphaTextRenderer<640>& text,
                            const settings::TypographySettings& typography, const ReadingSession& session,
                            ui::Rect area);

        void appendBidiParagraph(State& state, const ReadingSession& session,
                                 const ReadingLoop::TextParagraph& paragraph, BidiText::Analysis& analysis,
                                 bool bidiReady, size_t firstLine, size_t lastLine, BidiText::Line& bidiLine,
                                 std::vector<BidiText::Codepoint>& visual) {
            if (firstLine == lastLine)
                return;
            state.bidi = true;
            for (size_t lineIndex = firstLine; lineIndex < lastLine; ++lineIndex) {
                State::Line& line = state.lines[lineIndex];
                const size_t localStart = line.start - paragraph.firstWord;
                const size_t localEnd = line.end - paragraph.firstWord;
                const size_t offset = paragraph.wordOffsets[localStart];
                const size_t end =
                    paragraph.wordOffsets[localEnd - 1] + ReadingLoop::wordAt(session, line.end - 1).size();
                const BidiText::LineRange range{offset, end - offset};
                line.characterStart = state.characters.size();
                line.bidi = true;
                const bool resolved = bidiReady && analysis.resolve(range, bidiLine).has_value();
                line.rightToLeft = resolved && analysis.rightToLeft();
                if (!resolved)
                    bidiLine.assign(1, {range.offset, range.length, false});
                BidiText::visualCodepoints(paragraph.text, bidiLine, visual);
                for (const BidiText::Codepoint& codepoint: visual) {
                    const auto next = std::ranges::upper_bound(paragraph.wordOffsets, codepoint.offset);
                    const size_t localWord = static_cast<size_t>(next - paragraph.wordOffsets.begin() - 1);
                    const size_t logicalWord = paragraph.firstWord + localWord;
                    const std::string_view word = ReadingLoop::wordAt(session, logicalWord);
                    const bool belongsToWord = codepoint.offset < paragraph.wordOffsets[localWord] + word.size();
                    state.characters.push_back({
                        .codepoint = codepoint.value,
                        .wordOffset = static_cast<uint16_t>((!belongsToWord && logicalWord + 1 < paragraph.lastWord
                                                                 ? logicalWord + 1
                                                                 : logicalWord)
                                                            - state.pageStart),
                        .belongsToWord = belongsToWord,
                        .rightToLeft = codepoint.rightToLeft,
                    });
                }
                line.characterEnd = state.characters.size();
            }
        }

        void layout(State& state, ui::fonts::AlphaTextRenderer<640>& text, const Typeface& typeface,
                    const settings::TypographySettings& typography, const ReadingSession& session, ui::Rect area,
                    size_t start, bool measureInk = true) {
            const size_t wordCount = ReadingLoop::wordCount(session);
            size_t index = std::min(start, wordCount);
            clearPage(state);
            state.vertical = false;
            state.pageStart = index;
            const int16_t maximumWidth = std::max<int16_t>(1, static_cast<int16_t>(area.w - kMarginX * 2));
            const int16_t bottom = static_cast<int16_t>(area.y + area.h - kMarginY);
            int16_t top = static_cast<int16_t>(area.y + kMarginY);
            int16_t previousLineHeight = 0;
            std::pair<size_t, size_t> paragraphRange{};
            ReadingLoop::TextParagraph shapingParagraph;
            BidiText::Analysis shapingBidi;
            bool paragraphBidi = false;
            bool shapingBidiReady = true;
            BidiText::Line shapingBidiLine;
            std::vector<BidiText::Codepoint> visual;
            size_t bidiFirstLine = 0;
            state.characters.clear();

            while (index < wordCount && state.lineCount < state.lines.size()) {
                if (index < paragraphRange.first || index >= paragraphRange.second) {
                    if (paragraphBidi && shapingParagraph.lastWord > shapingParagraph.firstWord)
                        appendBidiParagraph(state, session, shapingParagraph, shapingBidi, shapingBidiReady,
                                            bidiFirstLine, state.lineCount, shapingBidiLine, visual);
                    paragraphRange = ReadingLoop::paragraphBoundsAt(session, index);
                    bidiFirstLine = state.lineCount;
                    paragraphBidi = session.metadata.requiresBidi(paragraphRange.first, paragraphRange.second);
                    shapingParagraph = {};
                    if (paragraphBidi) {
                        shapingParagraph = ReadingLoop::paragraphAt(session, index);
                        shapingBidiReady =
                            shapingBidi
                                .reset(shapingParagraph.text, session.metadata.directionAt(shapingParagraph.firstWord))
                                .has_value();
                    }
                }
                const bool startsParagraph = paragraphStart(session, index);
                if (state.lineCount > 0 && startsParagraph)
                    top = static_cast<int16_t>(top + std::max<int16_t>(4, previousLineHeight / 3));

                State::Line& line = state.lines[state.lineCount];
                line = {.start = index, .flowTop = top, .top = top, .paragraphStart = startsParagraph};
                int16_t width = 0;
                uint8_t ascent = 0;
                uint8_t descent = 0;
                uint8_t yAdvance = 0;
                while (index < wordCount) {
                    if (index > line.start && paragraphStart(session, index))
                        break;
                    const FontCatalog::Face face = typeface(index);
                    // Ordinary text needs wrap metrics, not the paragraph's
                    // full text. Avoid rereading it from SD on each rebase.
                    if (face.shaper && shapingParagraph.wordOffsets.empty())
                        shapingParagraph = ReadingLoop::paragraphAt(session, index);
                    const uint8_t faceIndex = rememberFace(state, face);
                    const ui::fonts::AlphaFont& font = face.raster.get();
                    activateFace(text, face);
                    State::Word& prepared =
                        prepareWord(state, index, faceIndex, text, face, typography, session, shapingParagraph,
                                    shapingBidi, paragraphBidi, shapingBidiReady, shapingBidiLine);
                    const int16_t spaceWidth = std::max<int16_t>(1, text.glyphAdvance(' '));
                    const bool joinsCjk = index > line.start && prepared.cjk && wordAt(state, index - 1).cjk;
                    const int16_t gap = index == line.start ? startsParagraph ? spaceWidth * 2 : 0
                                      : joinsCjk            ? 0
                                                            : spaceWidth;
                    prepared.x = static_cast<int16_t>(area.x + kMarginX + width + gap);
                    const int16_t widthWithWord = static_cast<int16_t>(width + gap + prepared.width);
                    if (index > line.start && widthWithWord > maximumWidth)
                        break;
                    width = widthWithWord;
                    ascent = std::max(ascent, font.ascent);
                    descent = std::max(descent, font.descent);
                    yAdvance = std::max(yAdvance, font.yAdvance);
                    ++index;
                }
                if (index == line.start)
                    ++index;
                const int16_t textHeight = static_cast<int16_t>(ascent + descent);
                if (top + textHeight > bottom && state.lineCount > 0) {
                    index = line.start;
                    break;
                }
                line.y = static_cast<int16_t>(top + ascent);
                line.bottom = static_cast<int16_t>(top + textHeight);
                line.end = index;
                for (size_t word = line.start; word < line.end; ++word)
                    state.words[word - state.pageStart].y = line.y;
                ++state.lineCount;
                previousLineHeight = std::max<int16_t>(yAdvance, textHeight) + kLineGap;
                top = static_cast<int16_t>(top + previousLineHeight);
            }
            if (paragraphBidi && shapingParagraph.lastWord > shapingParagraph.firstWord)
                appendBidiParagraph(state, session, shapingParagraph, shapingBidi, shapingBidiReady, bidiFirstLine,
                                    state.lineCount, shapingBidiLine, visual);
            state.pageEnd = index;
            const size_t visibleWords = state.pageEnd - state.pageStart;
            if (state.words.size() > visibleWords) {
                const uint32_t glyphEnd = state.words[visibleWords].glyphStart;
                state.words.resize(visibleWords);
                state.glyphs.resize(glyphEnd);
            }
            if (state.bidi) {
                for (size_t lineIndex = 0; lineIndex < state.lineCount; ++lineIndex) {
                    if (!state.lines[lineIndex].bidi)
                        continue;
                    state.lines[lineIndex].width = lineAdvance(state, state.lines[lineIndex], text, typography);
                }
            }
            if (measureInk)
                measurePageInk(state, text, typography, session, area);
        }

        bool logicalWordPosition(const State& state, size_t index, int16_t& x, int16_t& y) {
            if (index < state.pageStart || index >= state.pageEnd)
                return false;
            const State::Word& word = wordAt(state, index);
            x = word.x;
            y = word.y;
            return true;
        }

        void layoutVertical(State& state, ui::fonts::AlphaTextRenderer<640>& text, const Typeface& typeface,
                            const ReadingSession& session, ui::Rect area, size_t start) {
            clearPage(state);
            state.vertical = true;
            const size_t wordCount = ReadingLoop::wordCount(session);
            size_t index = std::min(start, wordCount);
            state.pageStart = index;
            const int16_t left = static_cast<int16_t>(area.x + kMarginX);
            const int16_t right = static_cast<int16_t>(area.x + area.w - kMarginX);
            const int16_t bottom = static_cast<int16_t>(area.y + area.h - kMarginY);
            int16_t x = left;
            int16_t rowTop = static_cast<int16_t>(area.y + kMarginY);
            int16_t rowHeight = 0;
            while (index < wordCount) {
                const FontCatalog::Face face = typeface(index);
                activateFace(text, face);
                const std::string_view value = ReadingLoop::wordAt(session, index);
                const int16_t width = text.textAdvance(value);
                const int16_t glyphHeight = std::max<int16_t>(1, text.pixelsPerEm());
                const int16_t gap = paragraphStart(session, index) && x != left ? glyphHeight / 2 : 0;
                if (x != left && x + gap + width > right) {
                    rowTop = static_cast<int16_t>(rowTop + rowHeight + kLineGap);
                    x = left;
                    rowHeight = 0;
                }
                if constexpr (ui::Context::displayWriteAlignment() > 1) {
                    if (x == left && state.lineCount == state.lines.size())
                        break;
                }
                rowHeight = std::max(rowHeight, glyphHeight);
                if (rowTop + rowHeight > bottom)
                    break;
                if constexpr (ui::Context::displayWriteAlignment() > 1) {
                    if (x == left)
                        state.lines[state.lineCount++] = {.start = index,
                                                          .top = rowTop,
                                                          .bottom = rowTop,
                                                          .inkKnown = true};
                    State::Line& line = state.lines[state.lineCount - 1];
                    line.end = index + 1;
                }
                const uint8_t faceIndex = rememberFace(state, face);
                state.words.push_back({.width = width,
                                       .x = static_cast<int16_t>(x + gap),
                                       .y = static_cast<int16_t>(rowTop + rowHeight / 2),
                                       .faceIndex = faceIndex});
                State::Word& placed = state.words.back();
                ui::fonts::AlphaTextRenderer<640>::Bounds bounds;
                placed.inkKnown = text.measureVertical(value, placed.x, placed.y, bounds);
                placed.ink = inkRect(bounds);
                if constexpr (ui::Context::displayWriteAlignment() > 1) {
                    State::Line& line = state.lines[state.lineCount - 1];
                    line.ink = unionInk(line.ink, placed.ink);
                    line.inkKnown &= placed.inkKnown;
                    if (placed.ink.h > 0) {
                        line.top = std::min(line.top, placed.ink.y);
                        line.bottom = std::max<int16_t>(line.bottom, placed.ink.y + placed.ink.h);
                    }
                }
                x = static_cast<int16_t>(x + gap + width);
                ++index;
            }
            state.pageEnd = index;
        }

        void drawVerticalWord(const State& state, ui::Context& ui, ui::fonts::AlphaTextRenderer<640>& text,
                              const ReadingSession& session, size_t index, ui::themes::ColorRole role, int16_t dx = 0,
                              int16_t dy = 0) {
            activateFace(text, faceAt(state, index));
            text.setTextColor(ui.color(role), ui.color(ui::themes::ColorRole::Background));
            const State::Word& word = wordAt(state, index);
            int16_t x = static_cast<int16_t>(word.x + dx);
            std::string_view value = ReadingLoop::wordAt(session, index);
            uint32_t codepoint = 0;
            while (Utf8Text::next(value, codepoint))
                x = static_cast<int16_t>(x
                                         + text.drawVerticalCodepoint(codepoint, x, static_cast<int16_t>(word.y + dy)));
        }

        void drawShapedWord(const State& state, ui::Context& ui, ui::fonts::AlphaTextRenderer<640>& text, size_t index,
                            int16_t x, int16_t baseline, ui::themes::ColorRole role) {
            text.setTextColor(ui.color(role), ui.color(ui::themes::ColorRole::Background));
            const State::Word& word = wordAt(state, index);
            const auto glyphs = std::span{state.glyphs}.subspan(word.glyphStart, word.glyphCount);
            text.drawGlyphs(glyphs, x, baseline);
        }

        void drawWord(const State& state, ui::Context& ui, ui::fonts::AlphaTextRenderer<640>& text,
                      const settings::TypographySettings& typography, const ReadingSession& session, size_t index,
                      int16_t x, int16_t baseline, ui::themes::ColorRole role) {
            if (wordAt(state, index).shaped) {
                drawShapedWord(state, ui, text, index, x, baseline, role);
                return;
            }
            text.setTextColor(ui.color(role), ui.color(ui::themes::ColorRole::Background));
            text.drawString(ReadingLoop::wordAt(session, index), x, baseline, typography.tracking);
        }

        int16_t lineAdvance(State& state, const State::Line& line, ui::fonts::AlphaTextRenderer<640>& text,
                            const settings::TypographySettings& typography) {
            int16_t advance = 0;
            size_t activeWord = kInvalidIndex;
            for (size_t index = line.characterStart; index < line.characterEnd; ++index) {
                State::Character& character = state.characters[index];
                character.x = advance;
                const size_t wordIndex = state.pageStart + character.wordOffset;
                if (wordIndex != activeWord) {
                    activateFace(text, faceAt(state, wordIndex));
                    activeWord = wordIndex;
                    if (character.belongsToWord && wordAt(state, wordIndex).shaped) {
                        advance = static_cast<int16_t>(advance + wordAt(state, wordIndex).width);
                        continue;
                    }
                }
                if (character.belongsToWord && wordAt(state, wordIndex).shaped)
                    continue;
                if (index > line.characterStart) {
                    const State::Character& previous = state.characters[index - 1];
                    if (character.belongsToWord && previous.belongsToWord && character.wordOffset == previous.wordOffset
                        && !character.rightToLeft)
                        advance =
                            static_cast<int16_t>(advance + text.kerningAdjust(previous.codepoint, character.codepoint));
                }
                character.x = advance;
                advance = static_cast<int16_t>(advance + text.glyphAdvance(character.codepoint));
                if (index + 1 < line.characterEnd && character.belongsToWord
                    && state.characters[index + 1].belongsToWord
                    && character.wordOffset == state.characters[index + 1].wordOffset)
                    advance = static_cast<int16_t>(advance + typography.tracking);
            }
            return advance;
        }

        void measurePageInk(State& state, ui::fonts::AlphaTextRenderer<640>& text,
                            const settings::TypographySettings& typography, const ReadingSession& session,
                            ui::Rect area) {
            for (size_t index = state.pageStart; index < state.pageEnd; ++index) {
                State::Word& word = state.words[index - state.pageStart];
                word.ink = {};
                word.inkKnown = true;
            }
            for (size_t lineIndex = 0; lineIndex < state.lineCount; ++lineIndex) {
                State::Line& line = state.lines[lineIndex];
                line.ink = {};
                line.inkKnown = true;
                if (line.bidi) {
                    activateFace(text, faceAt(state, line.start));
                    const int16_t indent = line.paragraphStart ? std::max<int16_t>(1, text.glyphAdvance(' ')) * 2 : 0;
                    line.x = line.rightToLeft ? static_cast<int16_t>(area.x + area.w - kMarginX - indent - line.width)
                                              : static_cast<int16_t>(area.x + kMarginX + indent);
                    size_t activeWord = kInvalidIndex;
                    for (size_t index = line.characterStart; index < line.characterEnd; ++index) {
                        const auto& character = state.characters[index];
                        const size_t wordIndex = state.pageStart + character.wordOffset;
                        State::Word& word = state.words[character.wordOffset];
                        const bool first = wordIndex != activeWord;
                        if (first) {
                            activateFace(text, faceAt(state, wordIndex));
                            activeWord = wordIndex;
                        }
                        ui::fonts::AlphaTextRenderer<640>::Bounds bounds;
                        if (character.belongsToWord && word.shaped) {
                            if (!first)
                                continue;
                            word.inkKnown &=
                                text.measure(std::span{state.glyphs}.subspan(word.glyphStart, word.glyphCount),
                                             line.x + character.x, line.y, bounds);
                        } else {
                            std::array<char, 4> encoded{};
                            const size_t bytes = Utf8Text::encode(character.codepoint, encoded);
                            word.inkKnown &=
                                text.measure({encoded.data(), bytes}, line.x + character.x, line.y, bounds);
                        }
                        word.ink = unionInk(word.ink, inkRect(bounds));
                    }
                } else {
                    for (size_t index = line.start; index < line.end; ++index) {
                        State::Word& word = state.words[index - state.pageStart];
                        activateFace(text, faceAt(state, index));
                        ui::fonts::AlphaTextRenderer<640>::Bounds bounds;
                        if (word.shaped)
                            word.inkKnown =
                                text.measure(std::span{state.glyphs}.subspan(word.glyphStart, word.glyphCount), word.x,
                                             word.y, bounds);
                        else
                            word.inkKnown = text.measure(ReadingLoop::wordAt(session, index), word.x, word.y, bounds,
                                                         typography.tracking);
                        word.ink = inkRect(bounds);
                    }
                }
                for (size_t index = line.start; index < line.end; ++index) {
                    const auto& word = wordAt(state, index);
                    line.ink = unionInk(line.ink, word.ink);
                    line.inkKnown &= word.inkKnown;
                }
                if (line.ink.h > 0) {
                    line.top = std::min(line.top, line.ink.y);
                    line.bottom = std::max<int16_t>(line.bottom, line.ink.y + line.ink.h);
                }
            }
        }

        void drawLine(const State& state, const State::Line& line, ui::Context& ui,
                      ui::fonts::AlphaTextRenderer<640>& text, ui::Rect area, size_t highlighted, ui::Rect viewport) {
            const int16_t dx = area.x - state.layoutArea.x;
            size_t activeWord = kInvalidIndex;
            for (size_t index = line.characterStart; index < line.characterEnd; ++index) {
                const State::Character& character = state.characters[index];
                const size_t wordIndex = state.pageStart + character.wordOffset;
                const auto& word = wordAt(state, wordIndex);
                const bool first = wordIndex != activeWord;
                activeWord = wordIndex;
                if (!visibleInk(word.ink, word.inkKnown, viewport))
                    continue;
                const int16_t x = static_cast<int16_t>(line.x + character.x + dx);
                if (first) {
                    activateFace(text, faceAt(state, wordIndex));
                    if (character.belongsToWord && word.shaped) {
                        drawShapedWord(state, ui, text, wordIndex, x, line.y,
                                       wordIndex == highlighted ? ui::themes::ColorRole::Accent
                                                                : ui::themes::ColorRole::Foreground);
                        continue;
                    }
                }
                if (character.belongsToWord && word.shaped)
                    continue;
                text.setTextColor(ui.color(character.belongsToWord && wordIndex == highlighted
                                               ? ui::themes::ColorRole::Accent
                                               : ui::themes::ColorRole::Foreground),
                                  ui.color(ui::themes::ColorRole::Background));
                text.drawCodepoint(character.codepoint, x, line.y);
            }
        }

        void drawPageWords(const State& state, ui::Context& ui, ui::fonts::AlphaTextRenderer<640>& text,
                           const settings::TypographySettings& typography, const ReadingSession& session,
                           ui::Rect translatedArea, ui::Rect viewport, int16_t dx, int16_t dy, size_t highlighted) {
            if (state.vertical) {
                for (size_t index = state.pageStart; index < state.pageEnd; ++index) {
                    const auto& word = wordAt(state, index);
                    if (!visibleInk(word.ink, word.inkKnown, viewport))
                        continue;
                    drawVerticalWord(state, ui, text, session, index,
                                     index == highlighted ? ui::themes::ColorRole::Accent
                                                          : ui::themes::ColorRole::Foreground,
                                     dx, dy);
                }
            } else {
                for (size_t lineIndex = 0; lineIndex < state.lineCount; ++lineIndex) {
                    State::Line line = state.lines[lineIndex];
                    if (!visibleInk(line.ink, line.inkKnown, viewport))
                        continue;
                    line.y = static_cast<int16_t>(line.y + dy);
                    if (line.bidi) {
                        drawLine(state, line, ui, text, translatedArea, highlighted, viewport);
                        continue;
                    }
                    for (size_t index = line.start; index < line.end; ++index) {
                        const State::Word& word = wordAt(state, index);
                        if (!visibleInk(word.ink, word.inkKnown, viewport))
                            continue;
                        const auto role = index == highlighted ? ui::themes::ColorRole::Accent
                                                               : ui::themes::ColorRole::Foreground;
                        activateFace(text, faceAt(state, index));
                        drawWord(state, ui, text, typography, session, index, static_cast<int16_t>(word.x + dx),
                                 static_cast<int16_t>(word.y + dy), role);
                    }
                }
            }
        }

        void paintPage(const State& state, ui::Context& ui, ui::fonts::AlphaTextRenderer<640>& text,
                       const settings::TypographySettings& typography, const ReadingSession& session, ui::Rect area,
                       ui::Rect dirty, size_t highlighted, std::string_view overlay, bool clip = false) {
            dirty = ui.paintBounds(dirty);
            const int16_t overlayWidth = ui::Context::textWidth(overlay, kOverlayTextSize);
            const ui::Rect overlayBounds{static_cast<int16_t>(area.x + (area.w - overlayWidth) / 2),
                                         static_cast<int16_t>(area.y + area.h - kMarginY - kOverlayTextHeight),
                                         overlayWidth, kOverlayTextHeight};
            const auto overlayText = ui.prepareText(overlayBounds, overlay, kOverlayTextSize);
            ui.paint(dirty, [&](Arduino_GFX& output, ui::Rect translated) {
                if (clip) output.fillRect(translated.x,translated.y,translated.w,translated.h,ui.color(ui::themes::Background));

                const int16_t dx = static_cast<int16_t>(translated.x - dirty.x);
                const int16_t dy = static_cast<int16_t>(translated.y - dirty.y);
                const ui::Rect translatedArea{static_cast<int16_t>(area.x + dx), static_cast<int16_t>(area.y + dy),
                                              area.w, area.h};
                ui::ClippedGfx clipped(output, translatedArea);
                Arduino_GFX& previousOutput = text.setOutput(clip ? static_cast<Arduino_GFX&>(clipped) : output);
                const ui::Rect viewport{static_cast<int16_t>(-dx), static_cast<int16_t>(-dy), output.width(),
                                        output.height()};
                drawPageWords(state, ui, text, typography, session, translatedArea, viewport, dx, dy, highlighted);
                if (!overlay.empty()) {
                    output.fillRect(static_cast<int16_t>(overlayBounds.x + dx - 4),
                                    static_cast<int16_t>(overlayBounds.y + dy - 2),
                                    static_cast<int16_t>(overlayBounds.w + 8),
                                    static_cast<int16_t>(overlayBounds.h + 4),
                                    ui.color(ui::themes::ColorRole::Background));
                    ui.drawText(output, overlayText, ui.color(ui::themes::ColorRole::Accent), dx, dy);
                }
                text.setOutput(previousOutput);
            });
        }

        int16_t logicalExtent(int16_t pixels, uint8_t scale) {
            return static_cast<int16_t>((int(pixels) * kNativeScale + scale - 1) / scale);
        }

        class PageCanvas final : public Arduino_Canvas {
        public:
            PageCanvas(int16_t width, int16_t height) : Arduino_Canvas(width, height, nullptr) {}

            bool begin(int32_t speed) override {
#if defined(ESP32) && defined(BOARD_HAS_PSRAM)
                if (_framebuffer == nullptr) {
                    const size_t bytes = static_cast<size_t>(width()) * height() * sizeof(uint16_t);
                    _framebuffer = static_cast<uint16_t*>(heap_caps_aligned_alloc(16, bytes,
                                                                                 MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
                }
                return _framebuffer != nullptr;
#else
                return Arduino_Canvas::begin(speed);
#endif
            }
        };

        void ensureCanvas(std::unique_ptr<Arduino_Canvas>& canvas, int16_t width, int16_t height) {
            if (canvas && canvas->width() == width && canvas->height() == height)
                return;
            canvas.reset();
            auto buffer = std::make_unique<PageCanvas>(width, height);
            if (buffer->begin(GFX_SKIP_OUTPUT_BEGIN))
                canvas = std::move(buffer);
        }

        void paintBrowseText(const State& state, Arduino_Canvas& canvas, ui::Context& ui,
                             ui::fonts::AlphaTextRenderer<640>& text, const settings::TypographySettings& typography,
                             const ReadingSession& session, ui::Rect area, size_t highlighted) {
            const ui::Rect target{0, 0, area.w, area.h};
            canvas.fillRect(0, 0, target.w, target.h, ui.color(ui::themes::Background));
            ui::ClippedGfx clipped(canvas, target);
            Arduino_GFX& previous = text.setOutput(clipped);
            drawPageWords(state, ui, text, typography, session, target, area, -area.x, -area.y, highlighted);
            text.setOutput(previous);
        }

        uint16_t blendPixel(uint16_t a, uint16_t b, uint32_t fraction) {
            if (a == b || fraction == 0)
                return a;
            constexpr uint32_t redBlue = 0xf81f;
            constexpr uint32_t green = 0x07e0;
            const uint32_t inverse = 32 - fraction;
            // Five bits of weight let red and blue share one multiply without carries between them.
            return (((((a & redBlue) * inverse + (b & redBlue) * fraction) >> 5) & redBlue)
                  | ((((a & green) * inverse + (b & green) * fraction) >> 5) & green));
        }

        struct PageSampling {
            static constexpr uint32_t unit = 1U << 16;
            uint32_t stride;
            uint32_t first;

            explicit PageSampling(uint8_t scale) : stride(unit * kNativeScale / scale), first((stride - unit) / 2) {}

            uint32_t at(int pixel) const { return first + stride * pixel; }
            int low(uint32_t position, int length) const { return std::min<int>(position >> 16, length - 1); }
            int high(int low, int length) const { return std::min(low + 1, length - 1); }
            uint32_t weight(uint32_t position) const { return (position >> 11) & 31; }
        };

        void resamplePageRow(uint16_t* dest, int width, const uint16_t* upper, const uint16_t* lower,
                             int sourceWidth, uint32_t fy, const PageSampling& sample) {
            if (sample.stride == PageSampling::unit * 2 && sourceWidth == width * 2) {
                for (int x = 0; x < width; ++x) {
                    const uint16_t a = upper[2 * x], b = upper[2 * x + 1];
                    const uint16_t c = lower[2 * x], d = lower[2 * x + 1];
                    dest[x] = blendPixel(blendPixel(a, b, 16), blendPixel(c, d, 16), fy);
                }
                return;
            }
            uint32_t sourceX = sample.first;
            for (int x = 0; x < width; ++x, sourceX += sample.stride) {
                const int x0 = sample.low(sourceX, sourceWidth);
                const int x1 = sample.high(x0, sourceWidth);
                const uint32_t fx = sample.weight(sourceX);
                dest[x] = blendPixel(blendPixel(upper[x0], upper[x1], fx), blendPixel(lower[x0], lower[x1], fx), fy);
            }
        }

        void paintScaledPage(Arduino_Canvas& canvas, ui::Context& ui, ui::Rect area, ui::Rect logicalArea,
                             uint8_t scale) {
            const uint16_t* pixels = canvas.getFramebuffer();
            const PageSampling sample(scale);
            ui.paint(area, [&](Arduino_GFX& output, ui::Rect translated) {
                std::array<uint16_t, 640> row;
                if (translated.w > static_cast<int16_t>(row.size()))
                    return;
                for (int y = 0; y < translated.h; ++y) {
                    const auto sourceY = sample.at(y);
                    const int y0 = sample.low(sourceY, logicalArea.h);
                    const int y1 = sample.high(y0, logicalArea.h);
                    resamplePageRow(row.data(), translated.w, pixels + y0 * canvas.width(),
                                    pixels + y1 * canvas.width(), logicalArea.w, sample.weight(sourceY), sample);
                    output.draw16bitRGBBitmap(translated.x, translated.y + y, row.data(), translated.w, 1);
                }
            });
        }

        // Whole display-pixel drags reuse finished rows if the sampling phase
        // stays the same. Highlight-only changes keep that phase at any size.
        void paintCachedPage(BrowseState& browse, ui::Context& ui, ui::fonts::AlphaTextRenderer<640>& text,
                            const settings::TypographySettings& typography, const ReadingSession& session,
                            ui::Rect area) {
            struct Rows { int begin = 0, end = 0; };
            const auto& state = browse.layout;
            auto& source = *browse.scaleCanvas;
            auto* frame = browse.viewportCanvas->getFramebuffer();
            auto& cache = browse.rowCache;
            const auto highlighted = browse.highlighted;
            const bool native = browse.scale == kNativeScale;
            const PageSampling sample(browse.scale);
            const auto& logicalArea = state.layoutArea;
            const int origin = logicalArea.y + browse.offset;
            const auto highlightRows = [&]() -> Rows {
                if (highlighted == kInvalidIndex)
                    return {};
                const auto& word = wordAt(state, highlighted);
                if (!word.inkKnown)
                    return {0, area.h};
                const int y = word.ink.y - origin;
                if (native)
                    return {y, y + word.ink.h};
                // Bilinear sampling can affect the rows beside the glyph ink.
                const int scale = browse.scale;
                return {(y - 1) * scale / kNativeScale - 1,
                        (y + word.ink.h + 1) * scale / kNativeScale + 2};
            }();
            std::array<Rows, 3> dirty;
            size_t count = 0;
            const auto addRows = [&](Rows range) {
                range.begin = std::max(0, range.begin);
                range.end = std::min(int(area.h), range.end);
                if (range.begin < range.end)
                    dirty[count++] = range;
            };
            const int distance = (browse.offset - cache.offset) * browse.scale;
            const int rows = distance / kNativeScale;
            const bool exactShift = distance % kNativeScale == 0
                && (distance == 0 || (PageSampling::unit * kNativeScale) % browse.scale == 0);
            if (!cache.valid || !exactShift || std::abs(rows) >= area.h) {
                addRows({0, area.h});
            } else {
                if (rows != 0) {
                    const int from = std::max(0, rows);
                    const int to = std::max(0, -rows);
                    const size_t copied = static_cast<size_t>(area.h - std::abs(rows)) * area.w;
                    std::memmove(frame + to * area.w, frame + from * area.w, copied * sizeof(*frame));
                    addRows(rows > 0 ? Rows{area.h - rows, area.h} : Rows{0, -rows});
                }
                if (highlighted != cache.highlighted) {
                    if (cache.highlighted != kInvalidIndex)
                        addRows({cache.highlightTop - rows, cache.highlightBottom - rows});
                    addRows(highlightRows);
                }
            }
            std::sort(dirty.begin(), dirty.begin() + count, [](Rows a, Rows b) { return a.begin < b.begin; });
            for (size_t i = 0; i < count; ++i) {
                auto [begin, end] = dirty[i];
                while (i + 1 < count && dirty[i + 1].begin <= end)
                    end = std::max(end, dirty[++i].end);
                const int sourceTop = native ? begin : sample.low(sample.at(begin), logicalArea.h);
                const int sourceBottom = native ? end : sample.high(sample.low(sample.at(end - 1), logicalArea.h),
                                                                    logicalArea.h) + 1;
                const ui::Rect region{logicalArea.x, static_cast<int16_t>(origin + sourceTop), logicalArea.w,
                                      static_cast<int16_t>(sourceBottom - sourceTop)};
                paintBrowseText(state, source, ui, text, typography, session, region, highlighted);
                const int pitch = source.width();
                const uint16_t* pixels = source.getFramebuffer();
                for (int y = begin; y < end; ++y) {
                    uint16_t* dest = frame + y * area.w;
                    if (native) {
                        std::memcpy(dest, pixels + (y - begin) * pitch, area.w * sizeof(*frame));
                    } else {
                        const auto sourceY = sample.at(y);
                        const int y0 = sample.low(sourceY, logicalArea.h);
                        const int y1 = sample.high(y0, logicalArea.h);
                        resamplePageRow(dest, area.w, pixels + (y0 - sourceTop) * pitch,
                                        pixels + (y1 - sourceTop) * pitch, logicalArea.w, sample.weight(sourceY), sample);
                    }
                }
            }
            cache = {highlighted, browse.offset, highlightRows.begin, highlightRows.end, true};
            ui.paint(area, [&](Arduino_GFX& output, ui::Rect target) {
                output.draw16bitRGBBitmap(target.x, target.y, frame, target.w, target.h);
            });
        }

        bool fullyVisible(const State::Word& word, const State::Line& line, ui::Rect viewport) {
            const ui::Rect ink = word.inkKnown ? word.ink
                : ui::Rect{word.x, line.top, word.width, static_cast<int16_t>(line.bottom - line.top)};
            return ink.w > 0 && ink.h > 0 && ink.x >= viewport.x && ink.y >= viewport.y
                && ink.x + ink.w <= viewport.x + viewport.w && ink.y + ink.h <= viewport.y + viewport.h;
        }

        ui::Rect browseViewport(const BrowseState& browse) {
            auto viewport = browse.layout.layoutArea;
            viewport.y += browse.offset;
            viewport.w = static_cast<int16_t>(int(browse.displayArea.w) * kNativeScale / browse.scale);
            viewport.h = static_cast<int16_t>(int(browse.displayArea.h) * kNativeScale / browse.scale);
            return viewport;
        }

        size_t lineForWord(const State& state, size_t word) {
            for (size_t row = 0; row < state.lineCount; ++row) {
                if (word >= state.lines[row].start && word < state.lines[row].end)
                    return row;
            }
            return state.lineCount;
        }

        bool fullyVisibleLine(const State::Line& line, ui::Rect viewport) {
            const bool measured = line.inkKnown && line.ink.h > 0;
            const int top = measured ? line.ink.y : line.top;
            const int bottom = measured ? line.ink.y + line.ink.h : line.bottom;
            return top >= viewport.y && bottom <= viewport.y + viewport.h;
        }

        size_t automaticHighlight(const State& state, ui::Rect viewport, HighlightMode mode) {
            size_t firstWord = kInvalidIndex;
            size_t firstCompleteLine = kInvalidIndex;
            const int bottom = viewport.y + viewport.h;
            for (size_t row = 0; row < state.lineCount; ++row) {
                const auto& line = state.lines[row];
                if (line.top >= bottom)
                    break;
                if (line.bottom <= viewport.y)
                    continue;
                for (size_t index = line.start; index < line.end; ++index) {
                    if (!fullyVisible(wordAt(state, index), line, viewport))
                        continue;
                    if (firstWord == kInvalidIndex)
                        firstWord = index;
                    if (fullyVisibleLine(line, viewport)) {
                        if (mode != HighlightMode::secondScrollLine || firstCompleteLine != kInvalidIndex)
                            return index;
                        firstCompleteLine = index;
                    }
                    break;
                }
            }
            return firstCompleteLine == kInvalidIndex ? firstWord : firstCompleteLine;
        }

        size_t visibleHighlight(const State& state, ui::Rect viewport, size_t selected, HighlightMode mode) {
            if (selected != kInvalidIndex) {
                const auto row = lineForWord(state, selected);
                if (row < state.lineCount) {
                    const auto& line = state.lines[row];
                    if (fullyVisible(wordAt(state, selected), line, viewport)) {
                        if (mode == HighlightMode::selection)
                            return selected;
                        if (fullyVisibleLine(line, viewport)) {
                            const auto automatic = automaticHighlight(state, viewport, mode);
                            // Keep the actual word until the automatic
                            // reading line passes its row.
                            return automatic == kInvalidIndex || selected >= automatic ? selected : automatic;
                        }
                    }
                }
            }
            return automaticHighlight(state, viewport, mode);
        }

        void paintHighlights(const State& state, ui::Context& ui, ui::fonts::AlphaTextRenderer<640>& text,
                             const settings::TypographySettings& typography, const ReadingSession& session,
                             ui::Rect area, size_t highlighted, std::string_view overlay) {
            // Recompose complete rows, including unchanged words sharing a physical pixel pair.
            for (size_t index = 0; index < state.lineCount; ++index) {
                const State::Line& line = state.lines[index];
                if ((highlighted < line.start || highlighted >= line.end)
                    && (state.highlighted < line.start || state.highlighted >= line.end))
                    continue;
                if (!line.inkKnown) {
                    paintPage(state, ui, text, typography, session, area, area, highlighted, overlay);
                    return;
                }
                const int16_t top = std::max<int16_t>(area.y, line.top & ~1);
                const int16_t bottom = std::min<int16_t>(area.y + area.h, (line.bottom + 1) & ~1);
                paintPage(state, ui, text, typography, session, area,
                          {area.x, top, area.w, static_cast<int16_t>(bottom - top)}, highlighted, overlay);
            }
        }

    } // namespace

    void draw(State& state, ui::Context& ui, ui::fonts::AlphaTextRenderer<640>& text, const Typeface& typeface,
              const settings::TypographySettings& typography, uint32_t typographyRevision,
              const ReadingSession& session, ui::Rect area, std::string_view overlay) {
        area = ui.paintBounds(area);
        const size_t wordCount = ReadingLoop::wordCount(session);
        if (wordCount == 0 || area.w <= kMarginX * 2 || area.h <= kMarginY * 2) {
            ui.redraw(area, 0);
            return;
        }

        const bool vertical = session.metadata.writingMode == WritingMode::verticalRl;
        if (state.layoutArea != area || state.layoutRevision != typographyRevision || state.vertical != vertical) {
            state.layoutArea = area;
            state.layoutRevision = typographyRevision;
            invalidate(state);
            state.vertical = vertical;
        }
        const size_t current = std::min<size_t>(session.state.wordIndex, wordCount - 1);
        if (vertical) {
            if (state.pageStart == kInvalidIndex)
                layoutVertical(state, text, typeface, session, area, anchorIndex(session, current));
            if (current < state.pageStart)
                layoutVertical(state, text, typeface, session, area, anchorIndex(session, current));
            if (current >= state.pageEnd && state.pageEnd > state.pageStart)
                layoutVertical(state, text, typeface, session, area,
                               current == state.pageEnd ? state.pageEnd : anchorIndex(session, current));
            if (current >= state.pageEnd)
                layoutVertical(state, text, typeface, session, area, current);

            uint32_t pageSignature = ui::Context::combine(Fnv1a::kOffsetBasis, static_cast<uint32_t>(state.pageStart));
            pageSignature = ui::Context::combine(pageSignature, static_cast<uint32_t>(state.pageEnd));
            pageSignature = ui::Context::combine(pageSignature, typographyRevision);
            pageSignature = ui::Context::combine(pageSignature, static_cast<uint8_t>(WritingMode::verticalRl));
            const uint32_t signature = ui::Context::signature(overlay, pageSignature);
            const bool redraw = ui.redraw(area, signature, true);
            if constexpr (ui::Context::displayWriteAlignment() > 1) {
                if (!redraw) {
                    if (state.highlighted != current)
                        paintHighlights(state, ui, text, typography, session, area, current, overlay);
                    state.highlighted = current;
                    return;
                }
            }
            if (redraw) {
                paintPage(state, ui, text, typography, session, area, area, current, overlay);
            } else if (state.highlighted != current) {
                if (state.highlighted >= state.pageStart && state.highlighted < state.pageEnd)
                    drawVerticalWord(state, ui, text, session, state.highlighted, ui::themes::ColorRole::Foreground);
                drawVerticalWord(state, ui, text, session, current, ui::themes::ColorRole::Accent);
                ui.markDrawn();
            }
            state.highlighted = current;
            return;
        }
        if (state.pageStart == kInvalidIndex)
            layout(state, text, typeface, typography, session, area, anchorIndex(session, current));
        if (current < state.pageStart)
            layout(state, text, typeface, typography, session, area, anchorIndex(session, current));
        if (current >= state.pageEnd && state.pageEnd > state.pageStart) {
            const size_t nextPage = state.pageEnd;
            layout(state, text, typeface, typography, session, area,
                   current == nextPage ? nextPage : anchorIndex(session, current));
        }
        if (current >= state.pageEnd)
            layout(state, text, typeface, typography, session, area, current);

        uint32_t pageSignature = ui::Context::combine(Fnv1a::kOffsetBasis, static_cast<uint32_t>(state.pageStart));
        pageSignature = ui::Context::combine(pageSignature, static_cast<uint32_t>(state.pageEnd));
        pageSignature = ui::Context::combine(pageSignature, typographyRevision);
        const uint32_t signature = ui::Context::signature(overlay, pageSignature);
        const bool redraw = ui.redraw(area, signature, true);
        if constexpr (ui::Context::displayWriteAlignment() > 1) {
            if (!redraw) {
                if (state.highlighted != current)
                    paintHighlights(state, ui, text, typography, session, area, current, overlay);
                state.highlighted = current;
                return;
            }
        }
        if (!redraw) {
            if (state.highlighted == current)
                return;
            const auto end = state.lines.begin() + state.lineCount;
            const auto previousLine = std::ranges::find_if(state.lines.begin(), end, [&](const State::Line& line) {
                return state.highlighted >= line.start && state.highlighted < line.end;
            });
            const auto currentLine = std::ranges::find_if(state.lines.begin(), end, [&](const State::Line& line) {
                return current >= line.start && current < line.end;
            });
            if (previousLine != end) {
                if (previousLine->bidi) {
                    drawLine(state, *previousLine, ui, text, area, current, {0, 0, ui.width(), ui.height()});
                } else {
                    int16_t x = 0;
                    int16_t y = 0;
                    if (logicalWordPosition(state, state.highlighted, x, y)) {
                        activateFace(text, faceAt(state, state.highlighted));
                        drawWord(state, ui, text, typography, session, state.highlighted, x, y,
                                 ui::themes::ColorRole::Foreground);
                    }
                }
            }
            if (currentLine != end && currentLine != previousLine) {
                if (currentLine->bidi) {
                    drawLine(state, *currentLine, ui, text, area, current, {0, 0, ui.width(), ui.height()});
                } else {
                    int16_t x = 0;
                    int16_t y = 0;
                    if (logicalWordPosition(state, current, x, y)) {
                        activateFace(text, faceAt(state, current));
                        drawWord(state, ui, text, typography, session, current, x, y, ui::themes::ColorRole::Accent);
                    }
                }
            } else if (currentLine != end && !currentLine->bidi) {
                int16_t x = 0;
                int16_t y = 0;
                if (logicalWordPosition(state, current, x, y)) {
                    activateFace(text, faceAt(state, current));
                    drawWord(state, ui, text, typography, session, current, x, y, ui::themes::ColorRole::Accent);
                }
            }
            state.highlighted = current;
            ui.markDrawn();
            return;
        }

        paintPage(state, ui, text, typography, session, area, area, current, overlay);
        state.highlighted = current;
    }


    void browse(BrowseState& browse, ui::Context& ui, ui::fonts::AlphaTextRenderer<640>& text, const Typeface& typeface,
                const settings::TypographySettings& typography, uint32_t revision,
                const ReadingSession& session, ui::Rect area, size_t selected, uint8_t scale, bool followSelection, HighlightMode highlightMode, bool earlierFollow) {
        auto& state=browse.layout;
        if (ReadingLoop::wordCount(session) == 0) {
            browse.highlighted = kInvalidIndex;
            ui.redraw(area, 0);
            return;
        }
        if constexpr (ui::Context::displayWriteAlignment() == 1)
            ensureCanvas(browse.viewportCanvas, area.w, ui.height());
        if (scale != kNativeScale || browse.viewportCanvas) {
            const auto width = static_cast<int16_t>((logicalExtent(area.w, scale) + 7) & ~7);
            ensureCanvas(browse.scaleCanvas, width, logicalExtent(ui.height(), scale));
            if (!browse.scaleCanvas)
                scale = kNativeScale;
        } else {
            browse.scaleCanvas.reset();
        }
        const ui::Rect logicalArea = scale == kNativeScale ? area
            : ui::Rect{0, 0, logicalExtent(area.w, scale), logicalExtent(area.h, scale)};
        const auto previousArea = browse.displayArea;
        browse.displayArea = area;
        const size_t wordCount = ReadingLoop::wordCount(session);
        const bool wrapChanged = browse.scale != scale || state.layoutRevision != revision
                              || state.layoutArea.w != logicalArea.w;
        if (wrapChanged) {
            if (browse.topLine != kInvalidIndex)
                browse.first = browse.topLine;
            browse.previousLines.clear();
            browse.offset = 0;
            browse.scrollRemainder = 0;
        }
        browse.first = std::min(browse.first, wordCount - 1);
        browse.scale = scale;
        const bool headerMoved = !wrapChanged && previousArea.w == area.w && previousArea.y != area.y
                              && browse.step == ScrollStep::none;
        int oldViewportInset = 0;
        if (headerMoved) {
            const int oldOffset = browse.offset;
            browse.scrollBy(area.y - previousArea.y);
            oldViewportInset = std::max(0, oldOffset - browse.offset);
        }
        const auto selectionViewport = [&] {
            auto visible = browseViewport(browse);
            // On release, use the original reading area for the last touch
            // sample even though hiding the bar exposes more text above it.
            visible.y += oldViewportInset;
            visible.h -= oldViewportInset;
            return visible;
        };
        const auto rebuild = [&](bool findingLine = false) {
            browse.rowCache.valid = false;
            const int16_t height = findingLine ? 640 : std::min(640, logicalArea.h + 224);
            layout(state, text, typeface, typography, session,
                   {logicalArea.x, logicalArea.y, logicalArea.w, height}, browse.first, !findingLine);
            state.layoutRevision = revision;
            state.layoutArea = logicalArea;
        };
        const auto lineOffset = [&](size_t line) {
            return int(state.lines[line].flowTop) - state.lines[0].flowTop;
        };
        const auto rememberLines = [&](size_t count) {
            for (size_t i = 0; i < count; ++i)
                browse.previousLines.emplace_back(state.lines[i].start, lineOffset(i + 1) - lineOffset(i));
            if (browse.previousLines.size() > 128)
                browse.previousLines.erase(browse.previousLines.begin(), browse.previousLines.end() - 128);
        };
        const auto wrapToWord = [&](size_t target) {
            // Recover the real line breaks from the paragraph start. Cache
            // earlier rows as we go so a reverse drag can reuse their layout.
            browse.first = ReadingLoop::paragraphBoundsAt(session, target).first;
            if (browse.first == target) {
                rebuild();
                return;
            }
            while (true) {
                rebuild(true);
                const auto row = lineForWord(state, target);
                if (row < state.lineCount) {
                    rememberLines(row);
                    browse.first = state.lines[row].start;
                    break;
                }
                if (state.lineCount > 1) {
                    const auto last = state.lineCount - 1;
                    rememberLines(last);
                    browse.first = state.lines[last].start;
                } else {
                    browse.first = std::min(state.pageEnd, target);
                }
            }
            rebuild();
        };
        if (wrapChanged || state.pageStart == kInvalidIndex)
            wrapToWord(browse.first);
        else if (state.pageStart != browse.first || state.layoutArea != logicalArea)
            rebuild();

        // Use line origins so ink overhang does not affect how far we scroll.
        const auto lineForOffset = [&] {
            size_t line = 0;
            while (line + 1 < state.lineCount && lineOffset(line + 1) <= browse.offset)
                ++line;
            return line;
        };
        const size_t firstBeforeStep = browse.first;
        const int offsetBeforeStep = browse.offset;
        bool arrowTarget = false;
        const auto step = browse.step;
        if (step != ScrollStep::none) {
            const auto row = lineForWord(state, browse.stepWord);
            if (row < state.lineCount) {
                const auto visible = browseViewport(browse);
                if (step == ScrollStep::nextPage)
                    browse.offset = lineOffset(row);
                else if (state.lines[row].bottom + kMarginY < visible.y + visible.h)
                    browse.offset = state.lines[row].bottom - logicalArea.y - logicalArea.h + kMarginY;
                arrowTarget = true;
            }
            browse.scrollRemainder = 0;
            browse.step = ScrollStep::none;
            browse.stepWord = kInvalidIndex;
        } else if ((followSelection || (headerMoved && area.y > previousArea.y)) && selected != kInvalidIndex) {
            const auto row = lineForWord(state, selected);
            if (row < state.lineCount) {
                // Follow a forward line crossing near the reading position.
                // Reversing or scrubbing within one line keeps the page steady
                // until the selected line would actually leave the viewport.
                const auto previous = lineForWord(state, browse.highlighted);
                if (followSelection && previous < row) {
                    auto firstVisible = lineForOffset();
                    const int guide = oldViewportInset + selectionViewport().h * 2 / 5;
                    const size_t last = std::min(row, firstVisible + row - previous);
                    const size_t guideRow = earlierFollow && row + 1 < state.lineCount ? row + 1 : row;
                    while (firstVisible < last && lineOffset(guideRow) > browse.offset + guide)
                        browse.offset = lineOffset(++firstVisible);
                }
                const auto visible = browseViewport(browse);
                const auto& line = state.lines[row];
                const int top = line.ink.h > 0 ? line.ink.y : line.top;
                const int bottom = line.ink.h > 0 ? line.ink.y + line.ink.h : line.bottom;
                if (top < visible.y)
                    browse.offset -= visible.y - top;
                else if (bottom > visible.y + visible.h)
                    browse.offset += bottom - (visible.y + visible.h);
            } else if (selected < state.pageStart) {
                browse.offset = -1;
            }
        }

        const size_t priorFirst = browse.first;
        if (browse.offset < 0 && browse.first > 0) {
            while (browse.offset < 0 && browse.first > 0) {
                if (!browse.previousLines.empty()) {
                    const auto [start, height] = browse.previousLines.back();
                    browse.previousLines.pop_back();
                    browse.first = start;
                    browse.offset += height;
                    continue;
                }
                const size_t old = browse.first;
                wrapToWord(old - 1);
                const auto joining = lineForWord(state, old);
                const int height = joining < state.lineCount && joining > 0 ? lineOffset(joining)
                    : state.lines[0].bottom - state.lines[0].flowTop + kLineGap;
                browse.offset += std::max(1, height);
            }
            if (state.pageStart != browse.first)
                rebuild();
        }
        if (followSelection && selected < priorFirst) {
            const auto row = lineForWord(state, selected);
            if (row < state.lineCount)
                browse.offset = std::min(browse.offset, lineOffset(row));
        }
        if (browse.offset < 0) {
            browse.offset = 0;
            browse.scrollRemainder = 0;
            oldViewportInset = 0;
        }

        // Keep the near lines in the current layout across small forward and
        // backward drags. Rebase several lines at a time to avoid rewrapping
        // and reshaping the buffered text on every line crossing.
        while (state.lineCount > 2) {
            const size_t top = lineForOffset();
            const bool needsMore = state.pageEnd < wordCount
                                && logicalArea.y + browse.offset + logicalArea.h > state.lines[state.lineCount - 1].bottom;
            if (top < (needsMore ? 2u : 4u))
                break;
            const size_t trim = top - 1;
            rememberLines(trim);
            browse.first = state.lines[trim].start;
            browse.offset -= lineOffset(trim);
            rebuild();
        }
        if (state.pageEnd >= wordCount && state.lineCount) {
            const int bottom = state.lines[state.lineCount - 1].bottom - logicalArea.y - logicalArea.h + kMarginY;
            const int end = std::max(0, bottom);
            if (browse.offset > end) {
                browse.offset = end;
                browse.scrollRemainder = 0;
                oldViewportInset = 0;
            }
        }
        if (arrowTarget && (browse.first != firstBeforeStep || browse.offset != offsetBeforeStep)) {
            selected = kInvalidIndex;
            highlightMode = HighlightMode::secondScrollLine;
        }
        browse.topLine = state.lines[lineForOffset()].start;
        browse.highlighted = visibleHighlight(state, selectionViewport(), selected, highlightMode);
        uint32_t signature=ui::Context::combine(revision,browse.first);
        signature = ui::Context::combine(signature, scale);
        signature=ui::Context::combine(signature,browse.offset);signature=ui::Context::combine(signature,browse.highlighted);
        if (!ui.redrawReplaced(area, signature))
            return;
        auto region = logicalArea;
        region.y += browse.offset;
        if (browse.viewportCanvas && browse.scaleCanvas) {
            paintCachedPage(browse, ui, text, typography, session, area);
        } else if (scale != kNativeScale) {
            paintBrowseText(state, *browse.scaleCanvas, ui, text, typography, session, region, browse.highlighted);
            paintScaledPage(*browse.scaleCanvas, ui, area, logicalArea, scale);
        } else if (browse.viewportCanvas) {
            auto& canvas = *browse.viewportCanvas;
            paintBrowseText(state, canvas, ui, text, typography, session, region, browse.highlighted);
            ui.paint(area, [&](Arduino_GFX& output, ui::Rect target) {
                output.draw16bitRGBBitmap(target.x, target.y, canvas.getFramebuffer(), target.w, target.h);
            });
        } else {
            ui.paint(area, [&](Arduino_GFX& output, ui::Rect target) {
                output.fillRect(target.x, target.y, target.w, target.h, ui.color(ui::themes::Background));
                ui::ClippedGfx clipped(output, target);
                Arduino_GFX& previous = text.setOutput(clipped);
                drawPageWords(state, ui, text, typography, session, target, region,
                              target.x - region.x, target.y - region.y, browse.highlighted);
                text.setOutput(previous);
            });
        }
    }
    size_t edgeWord(const BrowseState& browse, bool bottom) {
        const auto& state = browse.layout;
        const auto viewport = browseViewport(browse);
        size_t edge = kInvalidIndex;
        for (size_t row = 0; row < state.lineCount; ++row) {
            const auto& line = state.lines[row];
            if (line.top >= viewport.y + viewport.h)
                break;
            if (line.bottom <= viewport.y)
                continue;
            for (size_t word = line.start; word < line.end; ++word) {
                if (!fullyVisible(wordAt(state, word), line, viewport))
                    continue;
                if (!bottom)
                    return word;
                edge = word;
            }
        }
        return edge;
    }

    size_t scrubWord(const BrowseState& browse, size_t selected, bool right) {
        const auto& state = browse.layout;
        const size_t row = lineForWord(state, selected);
        if (row >= state.lineCount)
            return kInvalidIndex;
        const auto& line = state.lines[row];
        const bool forward = right != line.rightToLeft;
        if (line.bidi) {
            const auto center = [&](size_t index) {
                const auto& word = wordAt(state, index);
                return word.inkKnown && word.ink.w > 0 ? word.ink.x + word.ink.w / 2 : word.x + word.width / 2;
            };
            size_t nearest = kInvalidIndex;
            const int current = center(selected);
            int distance = INT16_MAX;
            for (size_t word = line.start; word < line.end; ++word) {
                const int delta = center(word) - current;
                const int candidate = right ? delta : -delta;
                if (candidate > 0 && candidate < distance) {
                    distance = candidate;
                    nearest = word;
                }
            }
            if (nearest != kInvalidIndex)
                return nearest;
            if (forward)
                return line.end < state.pageEnd ? line.end : kInvalidIndex;
            return line.start > 0 ? line.start - 1 : kInvalidIndex;
        }
        return forward ? (selected + 1 < state.pageEnd ? selected + 1 : kInvalidIndex)
                       : (selected > 0 ? selected - 1 : kInvalidIndex);
    }

    size_t wordAtPoint(const BrowseState& browse,int16_t x,int16_t y) {
        if (!ui::contains(browse.displayArea, x, y))
            return kInvalidIndex;
        if (browse.scale != kNativeScale) {
            x = (x - browse.displayArea.x) * kNativeScale / browse.scale;
            y = (y - browse.displayArea.y) * kNativeScale / browse.scale;
        }
        const auto& state=browse.layout;y+=browse.offset;
        for(size_t l=0;l<state.lineCount;++l) {
            const auto& line=state.lines[l];
            if(y<line.top || y>line.bottom+4) continue;
            size_t nearest=line.start;int distance=INT16_MAX;
            for(size_t i=line.start;i<line.end;++i) {
                const auto& word=state.words[i-state.pageStart];
                if(!line.bidi) {
                    const int d=x<word.x ? word.x-x : x>word.x+word.width ? x-word.x-word.width : 0;
                    if(d<distance) { distance=d;nearest=i; }
                }
            }
            if(line.bidi) {
                for(size_t c=line.characterStart;c<line.characterEnd;++c) {
                    const auto& character=state.characters[c];
                    if(!character.belongsToWord) continue;
                    const int d=std::abs(x-character.x);
                    if(d<distance) {distance=d;nearest=state.pageStart+character.wordOffset;}
                }
            }
            return nearest;
        }
        return SIZE_MAX;
    }

} // namespace screens::PageReader
