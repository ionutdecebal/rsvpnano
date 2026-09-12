#pragma once

#include <Arduino_GFX_Library.h>
#include <vector>

namespace testgfx {
    class Panel final : public Arduino_GFX {
    public:
        Panel(int16_t w = 368, int16_t h = 448, uint16_t initial = 0) :
                Arduino_GFX(w, h),
                pixels(static_cast<size_t>(w) * h, initial) {}

        void draw16bitRGBBitmap(int16_t x, int16_t y, uint16_t* data, int16_t w, int16_t h) override {
            if (x < 0 || y < 0 || w <= 0 || h != 2 || ((x | y | w | h) & 1) || x + w > width() || y + h > height()) {
                ++invalidWindows;
                return;
            }
            ++transfers;
            for (int row = 0; row < h; ++row)
                std::copy_n(data + row * w, w, pixels.data() + (y + row) * width() + x);
        }
        void setRotation(uint8_t r) override {
            ++panelRotations;
            Arduino_GFX::setRotation(r);
        }
        uint16_t at(int x, int y) const {
            return pixels[y * width() + x];
        }

        std::vector<uint16_t> pixels;
        int transfers = 0;
        int invalidWindows = 0;
        int panelRotations = 0;
    };
} // namespace testgfx
