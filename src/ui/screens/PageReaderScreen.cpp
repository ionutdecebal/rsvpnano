#include "ui/screens/PageReaderScreen.h"

#include <algorithm>
#include <iterator>

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
                const std::string_view locale = session.metadata.localeAt(index);
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

        int16_t lineAdvance(const State& state, const State::Line& line, ui::fonts::AlphaTextRenderer<640>& text,
                            const settings::TypographySettings& typography);

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
                    size_t start) {
            const size_t wordCount = ReadingLoop::wordCount(session);
            size_t index = std::min(start, wordCount);
            clearPage(state);
            state.vertical = false;
            state.pageStart = index;
            const int16_t maximumWidth = std::max<int16_t>(1, static_cast<int16_t>(area.w - kMarginX * 2));
            const int16_t bottom = static_cast<int16_t>(area.y + area.h - kMarginY);
            int16_t top = static_cast<int16_t>(area.y + kMarginY);
            int16_t previousLineHeight = 0;
            ReadingLoop::TextParagraph shapingParagraph;
            BidiText::Analysis shapingBidi;
            bool paragraphBidi = false;
            bool shapingBidiReady = true;
            BidiText::Line shapingBidiLine;
            std::vector<BidiText::Codepoint> visual;
            size_t bidiFirstLine = 0;
            state.characters.clear();

            while (index < wordCount && state.lineCount < state.lines.size()) {
                if (index < shapingParagraph.firstWord || index >= shapingParagraph.lastWord) {
                    if (paragraphBidi && shapingParagraph.lastWord > shapingParagraph.firstWord)
                        appendBidiParagraph(state, session, shapingParagraph, shapingBidi, shapingBidiReady,
                                            bidiFirstLine, state.lineCount, shapingBidiLine, visual);
                    shapingParagraph = ReadingLoop::paragraphAt(session, index);
                    bidiFirstLine = state.lineCount;
                    paragraphBidi =
                        session.metadata.requiresBidi(shapingParagraph.firstWord, shapingParagraph.lastWord);
                    if (paragraphBidi)
                        shapingBidiReady =
                            shapingBidi
                                .reset(shapingParagraph.text, session.metadata.directionAt(shapingParagraph.firstWord))
                                .has_value();
                }
                const bool startsParagraph = paragraphStart(session, index);
                if (state.lineCount > 0 && startsParagraph)
                    top = static_cast<int16_t>(top + std::max<int16_t>(4, previousLineHeight / 3));

                State::Line& line = state.lines[state.lineCount];
                line = {.start = index, .top = top, .paragraphStart = startsParagraph};
                int16_t width = 0;
                uint8_t ascent = 0;
                uint8_t descent = 0;
                uint8_t yAdvance = 0;
                while (index < wordCount) {
                    if (index > line.start && paragraphStart(session, index))
                        break;
                    const std::string_view word = ReadingLoop::wordAt(session, index);
                    const FontCatalog::Face face = typeface(index);
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
                        state.lines[state.lineCount++] = {.start = index, .top = rowTop, .bottom = rowTop};
                    State::Line& line = state.lines[state.lineCount - 1];
                    line.end = index + 1;
                    const int16_t inkHeight = text.verticalInkHeight(value);
                    const int16_t inkTop = static_cast<int16_t>(rowTop + rowHeight / 2 - inkHeight / 2);
                    line.top = std::min(line.top, inkTop);
                    line.bottom = std::max(line.bottom, static_cast<int16_t>(inkTop + inkHeight));
                }
                const uint8_t faceIndex = rememberFace(state, face);
                state.words.push_back({.width = width,
                                       .x = static_cast<int16_t>(x + gap),
                                       .y = static_cast<int16_t>(rowTop + rowHeight / 2),
                                       .faceIndex = faceIndex});
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

        void drawOverlay(ui::Context& ui, Arduino_GFX& output, ui::Rect area, std::string_view overlay) {
            if (overlay.empty())
                return;
            const int16_t width = ui::Context::textWidth(overlay, kOverlayTextSize);
            const int16_t x = static_cast<int16_t>(area.x + (area.w - width) / 2);
            const int16_t y = static_cast<int16_t>(area.y + area.h - kMarginY - kOverlayTextHeight);
            output.fillRect(static_cast<int16_t>(x - 4), static_cast<int16_t>(y - 2), static_cast<int16_t>(width + 8),
                            static_cast<int16_t>(kOverlayTextHeight + 4), ui.color(ui::themes::ColorRole::Background));
            ui.drawText(output, {x, y, width, kOverlayTextHeight}, overlay, kOverlayTextSize,
                        ui.color(ui::themes::ColorRole::Accent));
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

        int16_t lineAdvance(const State& state, const State::Line& line, ui::fonts::AlphaTextRenderer<640>& text,
                            const settings::TypographySettings& typography) {
            int16_t advance = 0;
            size_t activeWord = kInvalidIndex;
            for (size_t index = line.characterStart; index < line.characterEnd; ++index) {
                const State::Character& character = state.characters[index];
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
                advance = static_cast<int16_t>(advance + text.glyphAdvance(character.codepoint));
                if (index + 1 < line.characterEnd && character.belongsToWord
                    && state.characters[index + 1].belongsToWord
                    && character.wordOffset == state.characters[index + 1].wordOffset)
                    advance = static_cast<int16_t>(advance + typography.tracking);
            }
            return advance;
        }

        void drawLine(const State& state, const State::Line& line, ui::Context& ui,
                      ui::fonts::AlphaTextRenderer<640>& text, const settings::TypographySettings& typography,
                      ui::Rect area, size_t highlighted) {
            int16_t indent = 0;
            activateFace(text, faceAt(state, line.start));
            if (line.paragraphStart)
                indent = std::max<int16_t>(1, text.glyphAdvance(' ')) * 2;
            int16_t x = line.rightToLeft ? static_cast<int16_t>(area.x + area.w - kMarginX - indent - line.width)
                                         : static_cast<int16_t>(area.x + kMarginX + indent);
            size_t activeWord = kInvalidIndex;
            for (size_t index = line.characterStart; index < line.characterEnd; ++index) {
                const State::Character& character = state.characters[index];
                const size_t wordIndex = state.pageStart + character.wordOffset;
                if (wordIndex != activeWord) {
                    activateFace(text, faceAt(state, wordIndex));
                    activeWord = wordIndex;
                    if (character.belongsToWord && wordAt(state, wordIndex).shaped) {
                        drawShapedWord(state, ui, text, wordIndex, x, line.y,
                                       wordIndex == highlighted ? ui::themes::ColorRole::Accent
                                                                : ui::themes::ColorRole::Foreground);
                        x = static_cast<int16_t>(x + wordAt(state, wordIndex).width);
                        continue;
                    }
                }
                if (character.belongsToWord && wordAt(state, wordIndex).shaped)
                    continue;
                if (index > line.characterStart) {
                    const State::Character& previous = state.characters[index - 1];
                    if (character.belongsToWord && previous.belongsToWord && character.wordOffset == previous.wordOffset
                        && !character.rightToLeft)
                        x = static_cast<int16_t>(x + text.kerningAdjust(previous.codepoint, character.codepoint));
                }
                text.setTextColor(ui.color(character.belongsToWord && wordIndex == highlighted
                                               ? ui::themes::ColorRole::Accent
                                               : ui::themes::ColorRole::Foreground),
                                  ui.color(ui::themes::ColorRole::Background));
                x = static_cast<int16_t>(x + text.drawCodepoint(character.codepoint, x, line.y));
                if (index + 1 < line.characterEnd && character.belongsToWord
                    && state.characters[index + 1].belongsToWord
                    && character.wordOffset == state.characters[index + 1].wordOffset)
                    x = static_cast<int16_t>(x + typography.tracking);
            }
        }

        void paintPage(const State& state, ui::Context& ui, ui::fonts::AlphaTextRenderer<640>& text,
                       const settings::TypographySettings& typography, const ReadingSession& session, ui::Rect area,
                       ui::Rect dirty, size_t highlighted, std::string_view overlay) {
            dirty = ui.paintBounds(dirty);
            ui.paint(dirty, [&](Arduino_GFX& output, ui::Rect translated) {
                Arduino_GFX& previousOutput = text.setOutput(output);
                const int16_t dx = static_cast<int16_t>(translated.x - dirty.x);
                const int16_t dy = static_cast<int16_t>(translated.y - dirty.y);
                const ui::Rect translatedArea{static_cast<int16_t>(area.x + dx), static_cast<int16_t>(area.y + dy),
                                              area.w, area.h};
                if (state.vertical) {
                    for (size_t index = state.pageStart; index < state.pageEnd; ++index)
                        drawVerticalWord(state, ui, text, session, index,
                                         index == highlighted ? ui::themes::ColorRole::Accent
                                                              : ui::themes::ColorRole::Foreground,
                                         dx, dy);
                } else {
                    for (size_t lineIndex = 0; lineIndex < state.lineCount; ++lineIndex) {
                        State::Line line = state.lines[lineIndex];
                        line.y = static_cast<int16_t>(line.y + dy);
                        if (line.bidi) {
                            drawLine(state, line, ui, text, typography, translatedArea, highlighted);
                            continue;
                        }
                        for (size_t index = line.start; index < line.end; ++index) {
                            const auto role = index == highlighted ? ui::themes::ColorRole::Accent
                                                                   : ui::themes::ColorRole::Foreground;
                            activateFace(text, faceAt(state, index));
                            const State::Word& word = wordAt(state, index);
                            drawWord(state, ui, text, typography, session, index, static_cast<int16_t>(word.x + dx),
                                     static_cast<int16_t>(word.y + dy), role);
                        }
                    }
                }
                drawOverlay(ui, output, translatedArea, overlay);
                text.setOutput(previousOutput);
            });
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
            const bool redraw = ui.redraw(area, signature);
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
        const bool redraw = ui.redraw(area, signature);
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
                    drawLine(state, *previousLine, ui, text, typography, area, current);
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
                    drawLine(state, *currentLine, ui, text, typography, area, current);
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

} // namespace screens::PageReader
