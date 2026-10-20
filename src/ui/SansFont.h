#pragma once

#include <Arduino_GFX_Library.h>
#include <cstdint>
#include <string_view>

namespace ui::sans {
    struct Glyph {
        uint32_t codepoint, offset;
        uint8_t width, height;
        int8_t left, top;
        uint8_t advance;
    };
    struct Font {
        const Glyph* glyphs;
        const uint8_t* pixels;
        uint16_t count;
        uint8_t size, ascent, height;
    };
    const Font& font(uint8_t size);
    const Glyph* glyph(const Font& font, uint32_t codepoint);
    bool supports(std::string_view text);
    int width(std::string_view text, const Font& font);
    void draw(Arduino_GFX& output, std::string_view text, const Font& font, int x, int baseline,
              uint16_t foreground, uint16_t background, int clipX, int clipY, int clipW, int clipH);
}
