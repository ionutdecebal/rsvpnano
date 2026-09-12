#include <unity.h>
#include "Panel.h"
#include "ui/Ui.h"

void runAmoledReaderTests();
void runAmoledStandbyTests();
void runAmoledWidgetTests();

void setUp() {}
void tearDown() {}

namespace {
    ui::TouchContact contact;
    ui::TouchSampleResult pollContact(ui::TouchContact& result) {
        result = contact;
        return ui::TouchSampleResult::Contact;
    }
    std::pair<int, int> logicalPoint(int x, int y, int w, int h, int rotation) {
        switch (rotation) {
        case 1:
            return {y, w - 1 - x};
        case 2:
            return {w - 1 - x, h - 1 - y};
        case 3:
            return {h - 1 - y, x};
        default:
            return {x, y};
        }
    }
    uint16_t ink(int x, int y) {
        return static_cast<uint16_t>(1 + x + 64 * y);
    }

    void test_rotated_touch_matches_the_painted_pixel() {
        for (int rotation = 0; rotation < 4; ++rotation) {
            testgfx::Panel panel(36, 28);
            ui::Context ui(panel);
            ui.setTouchSource({.surface = {36, 28}, .poll = pollContact});
            ui.setOrientation(static_cast<ui::Orientation>(rotation));
            for (const auto [x, y]: {std::pair{0, 0}, {35, 0}, {0, 27}, {35, 27}, {13, 19}}) {
                contact = {true, static_cast<uint16_t>(x), static_cast<uint16_t>(y)};
                TEST_ASSERT_TRUE(ui.pollTouch(100));
                const auto [lx, ly] = logicalPoint(x, y, 36, 28, rotation);
                TEST_ASSERT_EQUAL(lx, ui.touch()->x);
                TEST_ASSERT_EQUAL(ly, ui.touch()->y);
                contact.touched = false;
                TEST_ASSERT_TRUE(ui.pollTouch(110));
                TEST_ASSERT_TRUE(ui::hasTouch(*ui.touch(), ui::TouchTap));
            }
        }
    }

    void test_two_rows_rotate_without_rotating_panel() {
        for (int rotation = 0; rotation < 4; ++rotation) {
            testgfx::Panel panel(36, 28);
            ui::Context ui(panel);
            ui.setOrientation(static_cast<ui::Orientation>(rotation));
            const ui::Rect region{0, 0, ui.width(), ui.height()};
            ui.paint(region, [&](Arduino_GFX& output, ui::Rect local) {
                for (int y = 0; y < region.h; ++y)
                    for (int x = 0; x < region.w; ++x)
                        output.drawPixel(local.x + x, local.y + y, ink(x, y));
            });
            for (int y = 0; y < 28; ++y)
                for (int x = 0; x < 36; ++x) {
                    const auto [lx, ly] = logicalPoint(x, y, 36, 28, rotation);
                    TEST_ASSERT_EQUAL_UINT16(ink(lx, ly), panel.at(x, y));
                }
            TEST_ASSERT_EQUAL(0, panel.panelRotations);
            TEST_ASSERT_EQUAL(0, panel.invalidWindows);
            TEST_ASSERT_EQUAL(0, panel.writes);
            TEST_ASSERT_EQUAL(14, panel.transfers);
        }
    }

    void test_partial_odd_and_clipped_regions_preserve_neighbours() {
        constexpr uint16_t sentinel = 0xDEAD;
        for (int rotation = 0; rotation < 4; ++rotation) {
            for (const ui::Rect requested:
                 {ui::Rect{3, 5, 11, 9}, ui::Rect{-3, -5, 15, 17}, ui::Rect{23, 23, 25, 21}}) {
                testgfx::Panel panel(36, 28, sentinel);
                ui::Context ui(panel);
                ui.setOrientation(static_cast<ui::Orientation>(rotation));
                const ui::Rect region = ui.paintBounds(requested);
                ui.paint(region, [&](Arduino_GFX& output, ui::Rect local) {
                    for (int y = 0; y < region.h; ++y)
                        for (int x = 0; x < region.w; ++x)
                            output.drawPixel(local.x + x, local.y + y, ink(x, y));
                });
                for (int y = 0; y < 28; ++y)
                    for (int x = 0; x < 36; ++x) {
                        const auto [lx, ly] = logicalPoint(x, y, 36, 28, rotation);
                        const bool inside =
                            lx >= region.x && ly >= region.y && lx < region.x + region.w && ly < region.y + region.h;
                        TEST_ASSERT_EQUAL_UINT16(inside ? ink(lx - region.x, ly - region.y) : sentinel, panel.at(x, y));
                    }
                TEST_ASSERT_EQUAL(0, panel.invalidWindows);
                TEST_ASSERT_EQUAL(0, panel.writes);
            }
        }
    }

    void test_widget_updates_use_aligned_payloads_and_keep_siblings() {
        testgfx::Panel panel;
        ui::Context ui(panel);
        ui.setOrientation(ui::Orientation::Landscape);
        const auto draw = [&](int progress) {
            ui.beginFrame(1);
            ui.button({3, 5, 129, 63}, "Start", true, ui::Icon::Books);
            ui.progressRing({139, 5, 85, 85}, progress);
            ui.dockItem({3, 281, 143, 57}, "Read", ui::Icon::Bookmark, 0xF800);
            ui.endFrame();
        };
        draw(25);
        const auto first = panel.pixels;
        const int transfers = panel.transfers;
        draw(25);
        TEST_ASSERT_EQUAL(transfers, panel.transfers);
        draw(70);
        TEST_ASSERT_TRUE(panel.transfers > transfers);
        const auto ring = ui.paintBounds({139, 5, 85, 85});
        for (int y = 0; y < panel.height(); ++y)
            for (int x = 0; x < panel.width(); ++x) {
                const auto [lx, ly] = logicalPoint(x, y, panel.width(), panel.height(), 3);
                if (lx < ring.x || lx >= ring.x + ring.w || ly < ring.y || ly >= ring.y + ring.h)
                    TEST_ASSERT_EQUAL_UINT16(first[y * panel.width() + x], panel.at(x, y));
            }
        TEST_ASSERT_EQUAL(0, panel.writes);
        TEST_ASSERT_EQUAL(0, panel.invalidWindows);
    }
} // namespace

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_rotated_touch_matches_the_painted_pixel);
    RUN_TEST(test_two_rows_rotate_without_rotating_panel);
    RUN_TEST(test_partial_odd_and_clipped_regions_preserve_neighbours);
    RUN_TEST(test_widget_updates_use_aligned_payloads_and_keep_siblings);
    runAmoledReaderTests();
    runAmoledStandbyTests();
    runAmoledWidgetTests();
    return UNITY_END();
}
