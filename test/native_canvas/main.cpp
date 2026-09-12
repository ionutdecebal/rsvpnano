#include <array>
#include <canvas/Arduino_Canvas.h>
#include <cstdio>
#include <vector>
#include "fonts/UiFont6x9.h"

namespace {
    constexpr uint16_t background = 0x18e3;
    constexpr uint16_t foreground = 0xf7de;

    void drawText(Arduino_GFX& output, int x, int baseline, int size) {
        output.setFont(u8g2_font_rsvpnano_ui_6x9_tf);
        output.setUTF8Print(true);
        output.setTextWrap(false);
        output.setTextColor(foreground);
        output.setTextSize(size);
        output.setCursor(x, baseline);
        for (const unsigned char value: std::string{"Aa Gg WPM 123 \xd0\x96\xd1\x8f"})
            output.write(value);
    }

    size_t differences(int panelWidth, int panelHeight, int rotation, int size, int textX, int baseline,
                       bool fullTextBounds) {
        Arduino_Canvas reference(panelWidth, panelHeight, nullptr);
        reference.begin(GFX_SKIP_OUTPUT_BEGIN);
        reference.setRotation(rotation);
        reference.fillScreen(background);
        drawText(reference, textX, baseline, size);

        const int pitch = (panelWidth + 3) & ~3;
        Arduino_Canvas strip(pitch, 2, nullptr);
        strip.begin(GFX_SKIP_OUTPUT_BEGIN);
        strip.setRotation(rotation);
        std::vector<uint16_t> assembled(panelWidth * panelHeight, background);
        const int logicalWidth = reference.width(), logicalHeight = reference.height();
        for (int y = 0; y < panelHeight; y += 2) {
            int dx = 0, dy = -y;
            switch (rotation) {
            case 1:
                dx = -y;
                dy = pitch - panelWidth;
                break;
            case 2:
                dx = pitch - panelWidth;
                dy = y - panelHeight + 2;
                break;
            case 3:
                dx = y - panelHeight + 2;
                dy = 0;
                break;
            default:
                break;
            }
            strip.fillScreen(background);
            if (fullTextBounds)
                strip.setTextBound(dx, dy, logicalWidth, logicalHeight);
            drawText(strip, textX + dx, baseline + dy, size);
            const uint16_t* pixels = strip.getFramebuffer();
            for (int row = 0; row < 2; ++row)
                for (int x = 0; x < panelWidth; ++x)
                    assembled[(y + row) * panelWidth + x] = pixels[row * pitch + x];
        }
        size_t mismatches = 0;
        const uint16_t* expected = reference.getFramebuffer();
        for (size_t i = 0; i < assembled.size(); ++i)
            mismatches += assembled[i] != expected[i];
        return mismatches;
    }
} // namespace

int main() {
    size_t cases = 0, failed = 0, withoutFix = 0;
    for (const auto dimensions: std::array<std::array<int, 2>, 3>{{{96, 74}, {98, 76}, {368, 448}}}) {
        for (int rotation = 0; rotation < 4; ++rotation) {
            for (int size = 1; size <= 4; ++size) {
                for (const auto position: std::array<std::array<int, 2>, 4>{{{3, 31}, {4, 32}, {-2, 17}, {17, 55}}}) {
                    const auto mismatches =
                        differences(dimensions[0], dimensions[1], rotation, size, position[0], position[1], true);
                    ++cases;
                    if (mismatches) {
                        ++failed;
                        if (failed <= 8)
                            std::printf("FAIL %dx%d rotation=%d size=%d x=%d baseline=%d pixels=%zu\n", dimensions[0],
                                        dimensions[1], rotation, size, position[0], position[1], mismatches);
                    }
                    withoutFix +=
                        differences(dimensions[0], dimensions[1], rotation, size, position[0], position[1], false) != 0;
                }
            }
        }
    }
    std::printf("Native Arduino_GFX + Canvas u8g2: %zu cases, %zu failures; %zu cases fail without full text bounds\n",
                cases, failed, withoutFix);
    return failed || !withoutFix;
}
