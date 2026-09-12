#include <unity.h>

#include <chrono>
#include <cstdio>

#include "Panel.h"
#include "ui/Ui.h"

namespace {
    void test_card_prepares_each_text_line_once_and_skips_unchanged_frames() {
        for (const auto orientation: {ui::Orientation::Portrait, ui::Orientation::Landscape}) {
            for (const bool rtl: {false, true}) {
                testgfx::Panel panel(368, 448);
                ui::Context ui(panel);
                const auto theme = ui::themes::defaultTheme();
                ui.setTheme(theme);
                ui.setOrientation(orientation);
                ui.setLanguageAssets({.direction = rtl ? TextDirection::rtl : TextDirection::ltr});
                const ui::Rect card{16, 24, static_cast<int16_t>(ui.width() - 32), 112};
                const std::string_view title =
                    rtl ? "הספר שלנו פרק 12 — ממשיכים לקרוא בקצב נוח" : "The quick brown fox jumps over the lazy dog";
                const std::string_view detail = rtl ? "פרק 12 מתוך 36" : "Chapter 12 of 36";
                ui.beginFrame(1);
                Arduino_GFX::allFontSelections = 0;
                Arduino_GFX::allTextBoundsCalls = 0;
                Arduino_GFX::allTextBytes = 0;
                const int beforeTransfers = panel.transfers;
                const auto started = std::chrono::steady_clock::now();
                ui.card(card, title, detail, 3);
                ui.endFrame();
                const auto elapsed =
                    std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - started)
                        .count();
                // Counts exercise real UI layout/bidi calls. Timings use a mock font/pixel backend,
                // not native Arduino_GFX decoding, device CPU time, or QSPI transfer time.
                std::printf("MOCK_TEXT_PROFILE direction=%s orientation=%s card=%dx%d transfers=%d "
                            "font_selections=%llu bounds_calls=%llu text_bytes=%llu elapsed_us=%lld\n",
                            rtl ? "rtl" : "latin", orientation == ui::Orientation::Portrait ? "portrait" : "landscape",
                            card.w, card.h, panel.transfers - beforeTransfers,
                            static_cast<unsigned long long>(Arduino_GFX::allFontSelections),
                            static_cast<unsigned long long>(Arduino_GFX::allTextBoundsCalls),
                            static_cast<unsigned long long>(Arduino_GFX::allTextBytes),
                            static_cast<long long>(elapsed));
                TEST_ASSERT_EQUAL(0, panel.invalidWindows);
                TEST_ASSERT_EQUAL_UINT64(3, Arduino_GFX::allTextBoundsCalls);
                const int completedTransfers = panel.transfers;
                Arduino_GFX::allFontSelections = 0;
                Arduino_GFX::allTextBoundsCalls = 0;
                Arduino_GFX::allTextBytes = 0;
                ui.beginFrame(1);
                ui.card(card, title, detail, 3);
                ui.endFrame();
                TEST_ASSERT_EQUAL_UINT64(0, Arduino_GFX::allFontSelections);
                TEST_ASSERT_EQUAL_UINT64(0, Arduino_GFX::allTextBoundsCalls);
                TEST_ASSERT_EQUAL_UINT64(0, Arduino_GFX::allTextBytes);
                TEST_ASSERT_EQUAL(completedTransfers, panel.transfers);
            }
        }
    }

    void test_portrait_chrome_prepares_each_line_once() {
        testgfx::Panel panel;
        ui::Context ui(panel);
        ui.setOrientation(ui::Orientation::Landscape);
        Arduino_GFX::allTextBoundsCalls = 0;
        ui.portraitText({8, 8, 72, 26}, "Title", 2, 0xFFFF);
        ui.portraitVerticalText({88, 8, 26, 100}, "ABC", 2, 0xFFFF);
        ui.portraitBattery({8, 126, 90, 22}, 63, false, "63%", true);
        TEST_ASSERT_EQUAL_UINT64(5, Arduino_GFX::allTextBoundsCalls);
        TEST_ASSERT_EQUAL(0, panel.invalidWindows);
    }

    void test_keyboard_input_change_only_prepares_its_caption_and_value() {
        testgfx::Panel panel;
        ui::Context ui(panel);
        ui.setOrientation(ui::Orientation::Landscape);
        ui::KeyboardState state;
        std::string value = "one";
        const ui::Rect area{0, 0, ui.width(), ui.height()};
        ui.beginFrame(1);
        ui.keyboard(area, value, 32, state, "Name");
        ui.endFrame();
        value = "two";
        Arduino_GFX::allTextBoundsCalls = 0;
        ui.beginFrame(1);
        ui.keyboard(area, value, 32, state, "Name");
        ui.endFrame();
        TEST_ASSERT_EQUAL_UINT64(2, Arduino_GFX::allTextBoundsCalls);
        TEST_ASSERT_EQUAL(0, panel.invalidWindows);
    }
} // namespace

void runAmoledTextWorkTests() {
    RUN_TEST(test_card_prepares_each_text_line_once_and_skips_unchanged_frames);
    RUN_TEST(test_portrait_chrome_prepares_each_line_once);
    RUN_TEST(test_keyboard_input_change_only_prepares_its_caption_and_value);
}
