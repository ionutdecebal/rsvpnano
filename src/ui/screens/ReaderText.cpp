#include "ui/PocketLines.h"
#include "ui/screens/ReaderScreen.h"
#include "text/Utf8Text.h"
#include "text/UnicodeText.h"

namespace screens {
    namespace {
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


    int16_t ReaderScreen::wordAnchorOffset(std::string_view word,
                                          std::span<const ui::fonts::PositionedGlyph> glyphs) const {
        const int focus = focusOffset(word);
        if (focus < 0) return 0;
        if (!glyphs.empty()) {
            uint32_t cluster = UINT32_MAX;
            for (const auto& glyph : glyphs)
                if (glyph.cluster <= static_cast<uint32_t>(focus) && (cluster == UINT32_MAX || glyph.cluster > cluster))
                    cluster = glyph.cluster;
            int16_t cursor = 0;
            for (const auto& glyph : glyphs) {
                if (glyph.cluster == cluster) return cursor + glyph.xOffset + glyph.xAdvance/2;
                cursor += glyph.xAdvance;
            }
        }
        size_t offset = 0;
        int16_t cursor = 0;
        uint32_t previous = 0;
        bool hasPrevious = false;
        while (!word.empty()) {
            const auto bytes = word.size();
            uint32_t cp = 0; Utf8Text::next(word, cp);
            if (hasPrevious) cursor += text_.kerningAdjust(previous, cp);
            if (offset == static_cast<size_t>(focus)) return cursor + text_.glyphAdvance(cp)/2;
            cursor += text_.glyphAdvance(cp) + typography_.tracking;
            offset += bytes - word.size(); previous = cp; hasPrevious = true;
        }
        return cursor/2;
    }

    void ReaderScreen::drawWord(std::string_view word, int16_t x, int16_t baseline, int16_t centerY, int focus,
                                bool vertical, ui::Context& ui) {
        size_t offset = 0;
        uint32_t previousCodepoint = 0;
        bool hasPrevious = false;
        while (!word.empty()) {
            const size_t bytes = word.size();
            uint32_t codepoint = 0;
            Utf8Text::next(word, codepoint);
            if (hasPrevious && !vertical) x += text_.kerningAdjust(previousCodepoint, codepoint);
            previousCodepoint = codepoint; hasPrevious = true;
            text_.setTextColor(focusHighlightEnabled() && focus >= 0 && offset == static_cast<size_t>(focus)
                                   ? ui.readingFocusColor()
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
            text_.setTextColor(focusHighlightEnabled() && focus >= 0
                                       && codepoint.offset == wordOffset + static_cast<size_t>(focus)
                                   ? ui.readingFocusColor()
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

    void ReaderScreen::drawGuides(ui::Context& ui, Arduino_GFX& gfx, int16_t anchor, int16_t baseline, ui::Rect bounds) {
        if (settings_.mode == settings::ReadingMode::chunks) return;
        const auto& raster=face_.raster.get();
        const bool highlighted=focusHighlightEnabled();
        ui::pocketlines::guides(ui,gfx,bounds,anchor,baseline+raster.wordInkTop,baseline+raster.wordInkBottom,
            typography_.guideSize,typography_.guideTextGap,
            highlighted?ui::themes::GuideFocus:ui::themes::Foreground,highlighted?255:96);
    }

} // namespace screens
