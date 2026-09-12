#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <string_view>
#include <vector>

#include "fonts/FontCatalog.h"
#include "reader/ReadingSession.h"
#include "ui/Ui.h"

namespace screens::PageReader {

    using Typeface = std::function<FontCatalog::Face(size_t)>;

    struct State {
        struct Word {
            uint16_t glyphStart = 0;
            uint16_t glyphCount = 0;
            int16_t width = 0;
            int16_t x = 0;
            int16_t y = 0;
            uint8_t faceIndex = 0;
            bool shaped : 1 = false;
            bool cjk : 1 = false;
            bool inkKnown : 1 = false;
            ui::Rect ink;
        };
        static_assert(sizeof(Word) == 20);

        struct Character {
            uint32_t codepoint = 0;
            uint16_t wordOffset = 0;
            int16_t x = 0;
            bool belongsToWord = false;
            bool rightToLeft = false;
        };

        struct Line {
            size_t start = 0;
            size_t end = 0;
            size_t characterStart = 0;
            size_t characterEnd = 0;
            int16_t y = 0;
            int16_t top = 0;
            int16_t bottom = 0;
            int16_t width = 0;
            int16_t x = 0;
            bool paragraphStart = false;
            bool bidi = false;
            bool rightToLeft = false;
            bool inkKnown = false;
            ui::Rect ink;
        };

        static constexpr size_t kMaximumLines = 24;

        std::array<Line, kMaximumLines> lines;
        std::vector<FontCatalog::Face> faces;
        std::vector<Word> words;
        std::vector<ui::fonts::PositionedGlyph> glyphs;
        std::vector<Character> characters;
        size_t lineCount = 0;
        size_t pageStart = std::numeric_limits<size_t>::max();
        size_t pageEnd = 0;
        size_t highlighted = std::numeric_limits<size_t>::max();
        ui::Rect layoutArea;
        uint32_t layoutRevision = 0;
        bool bidi = false;
        bool vertical = false;
    };

    void draw(State& state, ui::Context& ui, ui::fonts::AlphaTextRenderer<640>& text, const Typeface& typeface,
              const settings::TypographySettings& typography, uint32_t typographyRevision,
              const ReadingSession& session, ui::Rect area, std::string_view overlay = {});

} // namespace screens::PageReader
