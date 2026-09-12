#include <array>
#include <canvas/Arduino_Canvas.h>
#include <cstdio>
#include <string_view>
#include <vector>
#include "fonts/UiFont6x9.h"

namespace {
    constexpr uint16_t background = 0x18e3;
    constexpr uint16_t foreground = 0xf7de;
    constexpr std::string_view sample = "Aa Gg WPM 123 \xd0\x96\xd1\x8f";

    void drawText(Arduino_GFX& output, int x, int baseline, int size, std::string_view text = sample) {
        output.setFont(u8g2_font_rsvpnano_ui_6x9_tf);
        output.setUTF8Print(true);
        output.setTextWrap(false);
        output.setTextColor(foreground);
        output.setTextSize(size);
        output.setCursor(x, baseline);
        for (const unsigned char value: text)
            output.write(value);
    }

    size_t differences(int panelWidth, int panelHeight, int rotation, int size, int textX, int baseline,
                       bool fullTextBounds, bool preparedBounds = false, std::string_view text = sample) {
        Arduino_Canvas reference(panelWidth, panelHeight, nullptr);
        reference.begin(GFX_SKIP_OUTPUT_BEGIN);
        reference.setRotation(rotation);
        reference.fillScreen(background);
        drawText(reference, textX, baseline, size, text);
        int16_t inkX = 0, inkY = 0;
        uint16_t inkW = 0, inkH = 0;
        reference.getTextBounds(std::string{text}.c_str(), 0, 0, &inkX, &inkY, &inkW, &inkH);
        const bool reliableInk = text.find('\n') == std::string_view::npos;

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
            const int x1 = textX + inkX + dx, y1 = baseline + inkY + dy;
            if (!preparedBounds || !reliableInk
                || (x1 < strip.width() && y1 < strip.height() && x1 + inkW > 0 && y1 + inkH > 0))
                drawText(strip, textX + dx, baseline + dy, size, text);
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
    size_t cases = 0, failed = 0, withoutFix = 0, preparedFailures = 0, newlineFailures = 0, newlineCullingFailures = 0;
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
                    const auto prepared =
                        differences(dimensions[0], dimensions[1], rotation, size, position[0], position[1], true, true);
                    if (prepared && ++preparedFailures <= 8)
                        std::printf("PREPARED FAIL %dx%d rotation=%d size=%d x=%d baseline=%d pixels=%zu\n",
                                    dimensions[0], dimensions[1], rotation, size, position[0], position[1], prepared);
                    constexpr std::string_view multiline = "Aa Gg\nWPM 123 \xd0\x96\xd1\x8f";
                    newlineFailures += differences(dimensions[0], dimensions[1], rotation, size, position[0],
                                                   position[1], true, false, multiline)
                                    != 0;
                    newlineCullingFailures += differences(dimensions[0], dimensions[1], rotation, size, position[0],
                                                          position[1], true, true, multiline)
                                           != 0;
                }
            }
        }
    }
    std::printf("Native Arduino_GFX + Canvas u8g2: %zu cases, %zu failures, %zu prepared-ink failures; "
                "%zu cases fail without full text bounds\n",
                cases, failed, preparedFailures, withoutFix);
    std::printf("Native newline: %zu failures; %zu failures with prepared-ink culling\n", newlineFailures,
                newlineCullingFailures);
    return failed || preparedFailures || newlineFailures || newlineCullingFailures || !withoutFix;
}
