#include <unity.h>

#include "Panel.h"
#include "ui/Ui.h"

namespace {
    void test_opaque_custom_claim_paints_once_and_partial_claim_still_clears() {
        for (uint8_t rotation = 0; rotation < 4; ++rotation) {
            testgfx::Panel panel{120, 120};
            ui::Context ui{panel};
            const auto theme = ui::themes::defaultTheme();
            ui.setTheme(theme);
            ui.setOrientation(static_cast<ui::Orientation>(rotation));
            const ui::Rect rect{8, 8, 80, 40};
            for (int state = 0; state < 2; ++state) {
                ui.beginFrame(1);
                const int transfers = panel.transfers;
                TEST_ASSERT_TRUE(ui.redraw(rect, state, true));
                TEST_ASSERT_EQUAL(transfers, panel.transfers);
                ui.paint(rect, [&](Arduino_GFX& output, ui::Rect local) {
                    output.fillRect(local.x + 2 + state * 4, local.y + 2, 2, 2, 0xffff);
                });
                ui.endFrame();
                TEST_ASSERT_EQUAL((rotation & 1 ? rect.w : rect.h) / 2, panel.transfers - transfers);
                TEST_ASSERT_EQUAL(4, std::ranges::count(panel.pixels, uint16_t{0xffff}));
            }
            ui.beginFrame(1);
            const int transfers = panel.transfers;
            TEST_ASSERT_TRUE(ui.redraw(rect, 2));
            ui.endFrame();
            TEST_ASSERT_EQUAL((rotation & 1 ? rect.w : rect.h) / 2, panel.transfers - transfers);
            TEST_ASSERT_EQUAL(0, std::ranges::count(panel.pixels, uint16_t{0xffff}));
            TEST_ASSERT_EQUAL(0, panel.invalidWindows);
        }
    }

    void test_prepared_text_keeps_alignment_bidi_and_translation() {
        testgfx::Panel panel{120, 120};
        ui::Context context{panel};
        for (const bool rtl: {false, true}) {
            context.setLanguageAssets({.direction = rtl ? TextDirection::rtl : TextDirection::ltr});
            for (const auto align:
                 {ui::TextAlign::Start, ui::TextAlign::Left, ui::TextAlign::Center, ui::TextAlign::Right}) {
                const std::string_view text = rtl ? "אבגדהוזחטיכ" : "Alpha beta gamma delta";
                const auto local = context.prepareText({0, 0, 54, 36}, text, 2, align, 2);
                const auto positioned = context.prepareText({13, 17, 54, 36}, text, 2, align, 2);
                TEST_ASSERT_EQUAL(2, local.lines.size());
                TEST_ASSERT_EQUAL(local.lines.size(), positioned.lines.size());
                for (size_t i = 0; i < local.lines.size(); ++i) {
                    const auto& a = local.lines[i];
                    const auto& b = positioned.lines[i];
                    TEST_ASSERT_EQUAL_STRING(a.text.c_str(), b.text.c_str());
                    TEST_ASSERT_EQUAL_PTR(a.font, b.font);
                    TEST_ASSERT_EQUAL(a.size, b.size);
                    TEST_ASSERT_EQUAL(a.x + 13, b.x);
                    TEST_ASSERT_EQUAL(a.y + 17, b.y);
                    TEST_ASSERT_EQUAL(a.ink.x + 13, b.ink.x);
                    TEST_ASSERT_EQUAL(a.ink.y + 17, b.ink.y);
                    TEST_ASSERT_EQUAL(a.ink.w, b.ink.w);
                    TEST_ASSERT_EQUAL(a.ink.h, b.ink.h);
                }
                Arduino_GFX output{120, 120};
                const auto measured = Arduino_GFX::allTextBoundsCalls;
                context.drawText(output, local, 0xffff, 13, 17);
                const auto bytes = output.textWrites;
                TEST_ASSERT_GREATER_THAN(0, bytes);
                context.drawText(output, local, 0xffff, 13, 17);
                TEST_ASSERT_EQUAL(bytes * 2, output.textWrites);
                TEST_ASSERT_EQUAL_UINT64(measured, Arduino_GFX::allTextBoundsCalls);
            }
        }
        const auto truncated = context.prepareText({0, 0, 36, 18}, "אבגדהוזחטיכ", 1);
        TEST_ASSERT_EQUAL_STRING("...גבא", truncated.lines.front().text.c_str());
        context.setLanguageAssets({.direction = TextDirection::ltr});
        const auto multiline = context.prepareText({60, 0, 60, 36}, "A\nB", 1);
        TEST_ASSERT_EQUAL(1, multiline.lines.size());
        TEST_ASSERT_EQUAL(0, multiline.lines[0].ink.w);
        TEST_ASSERT_EQUAL(0, multiline.lines[0].ink.h);
        Arduino_GFX narrowStrip{2, 120};
        context.drawText(narrowStrip, multiline, 0xffff);
        TEST_ASSERT_EQUAL(3, narrowStrip.textWrites);
        TEST_ASSERT_EQUAL(0, panel.writes);
    }

    void test_prepared_fixed_text_retains_consumption_and_line_limit() {
        testgfx::Panel panel{120, 120};
        ui::Context context{panel};
        const auto clipped = context.prepareFixedText({0, 0, 30, 36}, "ABCDE FGHIJ KLMNO", 1, ui::TextAlign::Left, 2);
        TEST_ASSERT_EQUAL(2, clipped.lines.size());
        TEST_ASSERT_EQUAL(12, clipped.consumed);
        TEST_ASSERT_EQUAL_STRING("ABCDE", clipped.lines[0].text.c_str());
        TEST_ASSERT_EQUAL_STRING("FG...", clipped.lines[1].text.c_str());
        const auto withoutEllipsis =
            context.prepareFixedText({0, 0, 30, 36}, "ABCDE FGHIJ KLMNO", 1, ui::TextAlign::Left, 2, false);
        TEST_ASSERT_EQUAL(clipped.consumed, withoutEllipsis.consumed);
        TEST_ASSERT_EQUAL_STRING("FGHIJ", withoutEllipsis.lines[1].text.c_str());
        const auto capped = context.prepareFixedText({0, 0, 6, 108}, "abcdefghijkl", 1, ui::TextAlign::Left, 12, false);
        TEST_ASSERT_EQUAL(8, capped.lines.size());
        TEST_ASSERT_EQUAL(8, capped.consumed);
        TEST_ASSERT_TRUE(context.prepareFixedText({0, 0, 0, 36}, "Text", 1).lines.empty());
        TEST_ASSERT_EQUAL(0, panel.writes);
    }

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
                case 0:
                    ui.button(rect, value == 0 ? "First" : "Next");
                    break;
                case 1:
                    ui.card(rect, value == 0 ? "First" : "Next", "Detail");
                    break;
                case 2:
                    ui.dockItem(rect, value == 0 ? "First" : "Next", ui::Icon::Books, 0xFFFF);
                    break;
                case 3:
                    ui.progressRing(rect, value * 50);
                    break;
                default:
                    ui.hourglass(rect, value * 500, false, false);
                    break;
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
        for (const bool battery: {false, true}) {
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
    RUN_TEST(test_opaque_custom_claim_paints_once_and_partial_claim_still_clears);
    RUN_TEST(test_prepared_text_keeps_alignment_bidi_and_translation);
    RUN_TEST(test_prepared_fixed_text_retains_consumption_and_line_limit);
    RUN_TEST(test_opaque_widget_updates_send_one_pass);
    RUN_TEST(test_empty_battery_and_steps_still_erase_their_pixels);
    RUN_TEST(test_removed_and_moved_widgets_clear_old_ownership);
}
