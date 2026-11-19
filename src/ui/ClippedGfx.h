#pragma once

#include "ui/Ui.h"

namespace ui {
    // A non-allocating viewport for the existing rasterizer. Coordinates retain
    // their screen meaning, but every pixel/strip is confined to its owned area.
    class ClippedGfx final : public Arduino_GFX {
    public:
        ClippedGfx(Arduino_GFX& output, Rect clip)
            : Arduino_GFX(output.width(), output.height()), output_(output), clip_(clip) {}
        bool begin(int32_t = -1) override { return true; }
        void writePixelPreclipped(int16_t x, int16_t y, uint16_t color) override {
            if (x >= clip_.x && y >= clip_.y && x < clip_.x + clip_.w && y < clip_.y + clip_.h)
                output_.drawPixel(x, y, color);
        }
        void writeFastHLine(int16_t x, int16_t y, int16_t w, uint16_t color) override {
            fill(x, y, w, 1, color);
        }
        void writeFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color) override {
            fill(x, y, 1, h, color);
        }
        void writeFillRectPreclipped(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) override {
            fill(x, y, w, h, color);
        }
        using Arduino_GFX::draw16bitRGBBitmap;
        void draw16bitRGBBitmap(int16_t x, int16_t y, uint16_t* bitmap, int16_t w, int16_t h) override {
            const auto rect = intersection({x, y, w, h}, clip_);
            if (rect.w <= 0 || rect.h <= 0)
                return;

            auto* firstRow = bitmap + (rect.y - y) * w + (rect.x - x);
            if (rect.w == w) {
                output_.draw16bitRGBBitmap(rect.x, rect.y, firstRow, rect.w, rect.h);
                return;
            }

            // A horizontal crop keeps the original bitmap's row stride.
            for (int row = 0; row < rect.h; ++row)
                output_.draw16bitRGBBitmap(rect.x, rect.y + row, firstRow + row * w, rect.w, 1);
        }
    private:
        void fill(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
            const auto rect = intersection({x, y, w, h}, clip_);
            if (rect.w > 0 && rect.h > 0) output_.fillRect(rect.x, rect.y, rect.w, rect.h, color);
        }
        Arduino_GFX& output_;
        Rect clip_;
    };
}
