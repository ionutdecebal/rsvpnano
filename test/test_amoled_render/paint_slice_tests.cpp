#include <unity.h>

#include "Panel.h"
#include "ui/Ui.h"

namespace {
    void test_narrow_paint_clears_only_transmitted_physical_slices() {
        constexpr uint16_t stale = 0xDEAD;
        constexpr uint16_t ink = 0xF81F;
        for (int rotation = 0; rotation < 4; ++rotation) {
            testgfx::Panel panel(36, 28, stale);
            ui::Context ui(panel);
            const auto theme = ui::themes::defaultTheme();
            ui.setTheme(theme);
            ui.setOrientation(static_cast<ui::Orientation>(rotation));
            Arduino_Canvas* canvas = nullptr;
            ui.paint({0, 0, ui.width(), ui.height()}, [&](Arduino_GFX& output, ui::Rect) {
                canvas = &static_cast<Arduino_Canvas&>(output);
                output.fillScreen(stale);
            });
            TEST_ASSERT_NOT_NULL(canvas);
            const int pitch = rotation & 1 ? canvas->height() : canvas->width();
            const int rows = rotation & 1 ? canvas->width() : canvas->height();
            constexpr int windowWidth = 8;
            const ui::Rect region = rotation & 1 ? ui::Rect{4, 6, 2, windowWidth} : ui::Rect{4, 6, windowWidth, 2};
            const uint16_t background = ui.color(ui::themes::Background);
            for (int pass = 0; pass < 2; ++pass) {
                const std::vector<uint16_t> before(canvas->getFramebuffer(), canvas->getFramebuffer() + pitch * rows);
                const int transfers = panel.transfers;
                int callbacks = 0;
                ui.paint(region, [&](Arduino_GFX& output, ui::Rect local) {
                    ++callbacks;
                    for (int row = 0; row < rows; ++row) {
                        for (int x = 0; x < pitch; ++x) {
                            const int index = row * pitch + x;
                            TEST_ASSERT_EQUAL_UINT16(row < 2 && x < windowWidth ? background : before[index],
                                                     canvas->getFramebuffer()[index]);
                        }
                    }
                    if (pass == 0)
                        output.drawPixel(local.x + 1, local.y + 1, ink);
                });
                TEST_ASSERT_EQUAL(1, callbacks);
                TEST_ASSERT_EQUAL(transfers + 1, panel.transfers);
                for (int y = 0; y < panel.height(); ++y) {
                    for (int x = 0; x < panel.width(); ++x) {
                        int logicalX = x, logicalY = y;
                        switch (rotation) {
                        case 1:
                            logicalX = y;
                            logicalY = panel.width() - 1 - x;
                            break;
                        case 2:
                            logicalX = panel.width() - 1 - x;
                            logicalY = panel.height() - 1 - y;
                            break;
                        case 3:
                            logicalX = panel.height() - 1 - y;
                            logicalY = x;
                            break;
                        default:
                            break;
                        }
                        const bool inside = logicalX >= region.x && logicalX < region.x + region.w
                                         && logicalY >= region.y && logicalY < region.y + region.h;
                        const uint16_t expected = pass == 0 && logicalX == region.x + 1 && logicalY == region.y + 1
                                                    ? ink
                                                : inside ? background
                                                         : stale;
                        TEST_ASSERT_EQUAL_UINT16(expected, panel.at(x, y));
                    }
                }
            }
            TEST_ASSERT_EQUAL(0, panel.invalidWindows);
            TEST_ASSERT_EQUAL(0, panel.writes);
            TEST_ASSERT_EQUAL(0, panel.panelRotations);
        }
    }
} // namespace

void runAmoledPaintSliceTests() {
    RUN_TEST(test_narrow_paint_clears_only_transmitted_physical_slices);
}
