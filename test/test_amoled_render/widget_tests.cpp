#include <unity.h>

#include "Panel.h"
#include "ui/Ui.h"

namespace {
    void expectBackground(const testgfx::Panel& panel, ui::Rect rect, uint16_t background) {
        for (int y = rect.y; y < rect.y + rect.h; ++y)
            for (int x = rect.x; x < rect.x + rect.w; ++x)
                TEST_ASSERT_EQUAL_UINT16(background, panel.at(x, y));
    }

    void test_opaque_widget_updates_send_one_pass() {
        for (int widget = 0; widget < 5; ++widget) {
            testgfx::Panel panel{120, 120};
            ui::Context ui{panel};
            const auto theme = ui::themes::defaultTheme();
            ui.setTheme(theme);
            const ui::Rect rect{4, 4, 80, 40};
            const auto draw = [&](int value) {
                ui.beginFrame(1);
                switch (widget) {
                case 0: ui.button(rect, value == 0 ? "First" : "Next"); break;
                case 1: ui.card(rect, value == 0 ? "First" : "Next", "Detail"); break;
                case 2: ui.dockItem(rect, value == 0 ? "First" : "Next", ui::Icon::Books, 0xFFFF); break;
                case 3: ui.progressRing(rect, value * 50); break;
                default: ui.hourglass(rect, value * 500, false, false); break;
                }
                ui.endFrame();
            };
            draw(0);
            const int transfers = panel.transfers;
            draw(1);
            TEST_ASSERT_EQUAL(rect.h / 2, panel.transfers - transfers);
            TEST_ASSERT_EQUAL(0, panel.invalidWindows);
            TEST_ASSERT_EQUAL(0, panel.writes);
        }
    }

    void test_empty_battery_and_steps_still_erase_their_pixels() {
        for (const bool battery : {false, true}) {
            testgfx::Panel panel{120, 120};
            ui::Context ui{panel};
            const auto theme = ui::themes::defaultTheme();
            ui.setTheme(theme);
            const ui::Rect rect{4, 4, 80, 20};
            const auto draw = [&](bool visible) {
                ui.beginFrame(1);
                if (battery)
                    ui.battery(rect, 50, false, visible ? "50%" : "", visible);
                else
                    ui.steps(rect, 2, visible ? 3 : 0);
                ui.endFrame();
            };
            draw(true);
            const auto original = panel.pixels;
            const int transfers = panel.transfers;
            draw(false);
            TEST_ASSERT_EQUAL(rect.h / 2, panel.transfers - transfers);
            TEST_ASSERT_TRUE(original != panel.pixels);
            expectBackground(panel, rect, ui.color(ui::themes::Background));
            TEST_ASSERT_EQUAL(0, panel.invalidWindows);
            TEST_ASSERT_EQUAL(0, panel.writes);
        }
    }

    void test_removed_and_moved_widgets_clear_old_ownership() {
        testgfx::Panel panel{120, 120};
        ui::Context ui{panel};
        const auto theme = ui::themes::defaultTheme();
        ui.setTheme(theme);
        const ui::Rect first{4, 4, 44, 20}, second{60, 4, 44, 20}, moved{4, 40, 44, 20};
        ui.beginFrame(1);
        ui.button(first, "First");
        ui.button(second, "Second");
        ui.endFrame();
        int transfers = panel.transfers;
        ui.beginFrame(1);
        ui.button(first, "First");
        ui.endFrame();
        TEST_ASSERT_EQUAL(second.h / 2, panel.transfers - transfers);
        expectBackground(panel, second, ui.color(ui::themes::Background));
        transfers = panel.transfers;
        ui.beginFrame(1);
        ui.button(moved, "First");
        ui.endFrame();
        TEST_ASSERT_EQUAL((first.h + moved.h) / 2, panel.transfers - transfers);
        expectBackground(panel, first, ui.color(ui::themes::Background));
        TEST_ASSERT_EQUAL(0, panel.invalidWindows);
        TEST_ASSERT_EQUAL(0, panel.writes);
    }
} // namespace

void runAmoledWidgetTests() {
    RUN_TEST(test_opaque_widget_updates_send_one_pass);
    RUN_TEST(test_empty_battery_and_steps_still_erase_their_pixels);
    RUN_TEST(test_removed_and_moved_widgets_clear_old_ownership);
}
