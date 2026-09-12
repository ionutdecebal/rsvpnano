#include <unity.h>

#include "Panel.h"
#include "ui/screens/ChaptersScreen.h"
#include "ui/screens/watch/Layout.h"

namespace {
    ui::TouchContact contact;
    ui::TouchSampleResult pollContact(ui::TouchContact& result) {
        result = contact;
        return ui::TouchSampleResult::Contact;
    }

    void test_cards_and_dock_use_one_slot_per_control() {
        for (const bool dock: {false, true}) {
            testgfx::Panel panel{120, 120};
            ui::Context ui{panel};
            ui.setTouchSource({.surface = {120, 120}, .poll = pollContact});
            const auto draw = [&] {
                int activated = -1;
                ui.beginFrame(1);
                for (int index = 0; index < 64; ++index) {
                    const ui::Rect rect{static_cast<int16_t>((index % 8) * 14), static_cast<int16_t>((index / 8) * 14),
                                        14, 14};
                    const bool tapped = dock ? ui.dockItem(rect, {}, ui::Icon::None, 0xffff) : ui.card(rect, {});
                    if (tapped)
                        activated = index;
                }
                ui.endFrame();
                return activated;
            };
            contact = {true, 105, 105};
            ui.pollTouch(100);
            TEST_ASSERT_EQUAL(-1, draw());
            const auto pixels = panel.pixels;
            const int transfers = panel.transfers;
            contact.touched = false;
            ui.pollTouch(130);
            TEST_ASSERT_EQUAL(63, draw());
            TEST_ASSERT_EQUAL(-1, draw());
            TEST_ASSERT_EQUAL(transfers, panel.transfers);
            TEST_ASSERT_TRUE(pixels == panel.pixels);
            TEST_ASSERT_EQUAL(0, panel.invalidWindows);
        }
    }

    void test_card_disabled_and_moved_capture_cannot_activate() {
        testgfx::Panel panel{120, 120};
        ui::Context ui{panel};
        ui.setTouchSource({.surface = {120, 120}, .poll = pollContact});
        uint32_t now = 0;
        const auto frame = [&](bool down, bool enabled, ui::Rect rect = {4, 4, 44, 24}) {
            contact = {down, 20, 12};
            ui.pollTouch(now += 30);
            ui.beginFrame(1);
            const bool activated = ui.card(rect, "Card", {}, 2, ui::themes::Accent, ui::Icon::None, enabled);
            ui.endFrame();
            return activated;
        };
        TEST_ASSERT_FALSE(frame(true, true));
        TEST_ASSERT_FALSE(frame(false, false));
        TEST_ASSERT_FALSE(frame(true, false));
        TEST_ASSERT_FALSE(frame(false, true));
        TEST_ASSERT_FALSE(frame(true, true));
        TEST_ASSERT_FALSE(frame(false, true, {60, 4, 44, 24}));
        TEST_ASSERT_FALSE(frame(true, true));
        TEST_ASSERT_TRUE(frame(false, true));
        TEST_ASSERT_EQUAL(0, panel.invalidWindows);
    }

    void test_chapter_selection_redraws_only_cards_and_preserves_full_redraw_pixels() {
        testgfx::Panel panel;
        ui::Context ui{panel};
        const auto theme = ui::themes::defaultTheme();
        ui.setTheme(theme);
        ui.setOrientation(ui::Orientation::Landscape);
        ui.setTouchSource({.surface = {368, 448}, .poll = pollContact});
        screens::ChaptersScreen chapters;
        ReadingSession reader;
        std::array<std::string, 15> words;
        words.fill("word");
        ReadingLoop::setWords(reader, words, 0);
        const std::array<ChapterMarker, 3> markers{{{"First", 0}, {"Second", 5}, {"Third", 10}}};
        settings::ReadingSettings settings;
        auto screen = screens::Screen::Chapters;
        uint32_t now = 0;
        const auto draw = [&] {
            ui.beginFrame(static_cast<uint8_t>(screen));
            const auto action = chapters.draw(ui, markers, reader, settings, now, screen);
            ui.endFrame();
            return action;
        };
        const auto touch = [&](bool down, uint16_t x, uint16_t y) {
            contact = {down, y, static_cast<uint16_t>(447 - x)};
            ui.pollTouch(now += 30);
            return draw();
        };
        const auto cards = screens::watch::carousel(screens::detail::tabContent(ui));
        int cardTransfers = 0;
        for (const auto rect: cards)
            cardTransfers += testgfx::transfersFor(ui.paintBounds(rect).w);
        draw();
        TEST_ASSERT_EQUAL(screens::Action::None, touch(true, 280, 100));
        TEST_ASSERT_EQUAL(screens::Action::None, touch(true, 160, 100));
        int transfers = panel.transfers;
        TEST_ASSERT_EQUAL(screens::Action::None, touch(false, 160, 100));
        TEST_ASSERT_EQUAL(cardTransfers, panel.transfers - transfers);
        TEST_ASSERT_EQUAL(0, reader.state.wordIndex);
        transfers = panel.transfers;
        draw();
        TEST_ASSERT_EQUAL(cardTransfers, panel.transfers - transfers);
        transfers = panel.transfers;
        draw();
        TEST_ASSERT_EQUAL(transfers, panel.transfers);
        const auto selectedPixels = panel.pixels;
        ui.invalidate();
        draw();
        TEST_ASSERT_TRUE(selectedPixels == panel.pixels);
        TEST_ASSERT_EQUAL(screens::Action::None, touch(true, 224, 100));
        TEST_ASSERT_EQUAL(screens::Action::Resume, touch(false, 224, 100));
        TEST_ASSERT_EQUAL(5, reader.state.wordIndex);
        TEST_ASSERT_EQUAL(screens::Action::None, touch(true, 360, 100));
        TEST_ASSERT_EQUAL(screens::Action::None, touch(false, 360, 100));
        transfers = panel.transfers;
        draw();
        TEST_ASSERT_EQUAL(cardTransfers, panel.transfers - transfers);
        TEST_ASSERT_EQUAL(5, reader.state.wordIndex);
        const auto tappedPixels = panel.pixels;
        ui.invalidate();
        draw();
        TEST_ASSERT_TRUE(tappedPixels == panel.pixels);
        TEST_ASSERT_EQUAL(screens::Action::None, touch(true, 224, 100));
        TEST_ASSERT_EQUAL(screens::Action::Resume, touch(false, 224, 100));
        TEST_ASSERT_EQUAL(10, reader.state.wordIndex);
        TEST_ASSERT_EQUAL(0, panel.invalidWindows);
        TEST_ASSERT_EQUAL(0, panel.writes);
    }
} // namespace

void runAmoledInteractionTests() {
    RUN_TEST(test_cards_and_dock_use_one_slot_per_control);
    RUN_TEST(test_card_disabled_and_moved_capture_cannot_activate);
    RUN_TEST(test_chapter_selection_redraws_only_cards_and_preserves_full_redraw_pixels);
}
