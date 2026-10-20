#include "ui/SansFont.h"
#include "fonts/UiSans.generated.h"
#include "text/Utf8Text.h"
#include <algorithm>

namespace ui::sans {
    const Font& font(uint8_t size) {
        if (size >= 66) return assets::font66;
        if (size >= 39) return assets::font39;
        if (size >= 28) return assets::font28;
        if (size >= 26) return assets::font26;
        if (size >= 21) return assets::font21;
        if (size >= 17) return assets::font17;
        if (size >= 15) return assets::font15;
        if (size >= 14) return assets::font14;
        return size >= 13 ? assets::font13 : assets::font11;
    }
    const Glyph* glyph(const Font& font, uint32_t codepoint) {
        const auto* end = font.glyphs + font.count;
        const auto* found = std::lower_bound(font.glyphs, end, codepoint,
                                              [](const Glyph& g, uint32_t cp) { return g.codepoint < cp; });
        return found != end && found->codepoint == codepoint ? found : nullptr;
    }
    bool supports(std::string_view text) {
        uint32_t cp;
        while (Utf8Text::next(text, cp))
            if (cp != '\n' && glyph(assets::font13, cp) == nullptr)
                return false;
        return true;
    }
    int width(std::string_view text, const Font& font) {
        int advance = 0, extent = 0, left = 0;
        uint32_t cp;
        while (Utf8Text::next(text, cp)) {
            const Glyph* g = glyph(font, cp);
            if (!g) g = glyph(font, '?');
            left = std::min(left, advance + g->left);
            extent = std::max(extent, advance + g->left + g->width);
            advance += g->advance;
        }
        return std::max(advance, extent) - left;
    }
    static int leftBearing(std::string_view text, const Font& font) {
        int advance = 0, left = 0;
        uint32_t cp;
        while (Utf8Text::next(text, cp)) {
            const auto* g = glyph(font, cp);
            if (!g) g = glyph(font, '?');
            left = std::min(left, advance + g->left);
            advance += g->advance;
        }
        return left;
    }
    static uint16_t blend(uint16_t fg, uint16_t bg, int alpha) {
        const int r = (((fg >> 11) & 31)*alpha + ((bg >> 11) & 31)*(15-alpha)+7)/15;
        const int g = (((fg >> 5) & 63)*alpha + ((bg >> 5) & 63)*(15-alpha)+7)/15;
        const int b = ((fg & 31)*alpha + (bg & 31)*(15-alpha)+7)/15;
        return static_cast<uint16_t>((r<<11)|(g<<5)|b);
    }
    void draw(Arduino_GFX& output, std::string_view text, const Font& font, int x, int baseline,
              uint16_t foreground, uint16_t background, int clipX, int clipY, int clipW, int clipH) {
        // Alignment owns the full ink bounds, including the left overhang of j/î/ï.
        x -= leftBearing(text, font);
        uint16_t colors[16];
        for (int a=0;a<16;++a) colors[a]=blend(foreground,background,a);
        uint32_t cp;
        while (Utf8Text::next(text, cp)) {
            const Glyph* g=glyph(font,cp);
            if (!g) g=glyph(font,'?');
            for (int row=0;row<g->height;++row) {
                const int y=baseline+g->top+row;
                if (y<clipY || y>=clipY+clipH) continue;
                for (int col=0;col<g->width;++col) {
                    const int px=x+g->left+col;
                    if (px<clipX || px>=clipX+clipW) continue;
                    const int index=row*g->width+col;
                    const uint8_t packed=font.pixels[g->offset+index/2];
                    const uint8_t alpha=(index&1) ? packed&15 : packed>>4;
                    if (alpha) output.drawPixel(px,y,colors[alpha]);
                }
            }
            x+=g->advance;
        }
    }
}
