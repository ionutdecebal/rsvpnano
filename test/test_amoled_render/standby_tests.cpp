#include <unity.h>

#include "Panel.h"
#include "screensavers/Screensaver.h"
#include "ui/screens/StandbyScreen.h"

namespace {
    void checkStandby(standby::Kind kind, ui::Orientation orientation) {
        testgfx::Panel panel(410, 502);
        ui::Context ui(panel);
        const auto theme = ui::themes::defaultTheme();
        ui.setTheme(theme);
        ui.setOrientation(orientation);
        constexpr uint32_t now = 1200;
        constexpr size_t book = 3, word = 19;
        constexpr uint16_t sentinel = 0xDEAD;
        screens::StandbyScreen screen;
        screen.begin(ui, now, book, word, kind);

        const int columns = std::min<int>((ui.width() + 3) / 4, standby::kMaxStandbyColumns);
        const int rows = std::min<int>((ui.height() + 3) / 4, standby::kMaxStandbyRows);
        const int originX = ((ui.width() - columns * 4) / 2) & ~1;
        const int originY = ((ui.height() - rows * 4) / 2) & ~1;
        standby::ScreensaverSlot reference;
        reference.select(kind, columns, rows);
        reference.seed(now ^ (static_cast<uint32_t>(book) << 16U) ^ (static_cast<uint32_t>(word) * 2654435761U));

        const auto checkPixels = [&](const standby::Frame& frame, size_t preserved = SIZE_MAX) {
            for (int y = 0; y < panel.height(); ++y) {
                for (int x = 0; x < panel.width(); ++x) {
                    const int logicalX = orientation == ui::Orientation::Landscape ? panel.height() - 1 - y : x;
                    const int logicalY = orientation == ui::Orientation::Landscape ? x : y;
                    uint16_t expected = ui.color(ui::themes::Background);
                    if (logicalX >= originX && logicalY >= originY && logicalX < originX + columns * 4
                        && logicalY < originY + rows * 4) {
                        const size_t cell = (logicalY - originY) / 4 * columns + (logicalX - originX) / 4;
                        if (standby::cellAlive(frame.cells, cell))
                            expected =
                                ui.color(kind == standby::Kind::life ? ui::themes::Foreground : ui::themes::Accent);
                        else if (standby::cellAlive(frame.dimCells, cell))
                            expected = ui.blend(ui::themes::Foreground, 72);
                    }
                    if (static_cast<size_t>(y * panel.width() + x) == preserved)
                        expected = sentinel;
                    TEST_ASSERT_EQUAL_UINT16(expected, panel.at(x, y));
                }
            }
        };

        screen.draw(ui);
        checkPixels(reference.frame());
        reference.step();
        const auto next = reference.frame();
        TEST_ASSERT_FALSE(next.fullRedraw);
        size_t preserved = SIZE_MAX;
        for (int cell = 0; cell < columns * rows; ++cell) {
            if (standby::cellAlive(next.dirtyCells, cell))
                continue;
            const int logicalX = originX + cell % columns * 4 + 1;
            const int logicalY = originY + cell / columns * 4 + 1;
            if (logicalX < 0 || logicalY < 0 || logicalX >= ui.width() || logicalY >= ui.height())
                continue;
            const int x = orientation == ui::Orientation::Landscape ? logicalY : logicalX;
            const int y = orientation == ui::Orientation::Landscape ? panel.height() - 1 - logicalX : logicalY;
            preserved = y * panel.width() + x;
            panel.pixels[preserved] = sentinel;
            break;
        }
        TEST_ASSERT_TRUE(preserved != SIZE_MAX);
        const int initialTransfers = panel.transfers;
        screen.update(ui, now);
        TEST_ASSERT_TRUE(panel.transfers > initialTransfers);
        checkPixels(next, preserved);

        const auto afterUpdate = panel.pixels;
        const int transfers = panel.transfers;
        screen.update(ui, now + 159);
        TEST_ASSERT_EQUAL(transfers, panel.transfers);
        TEST_ASSERT_TRUE(afterUpdate == panel.pixels);
        TEST_ASSERT_EQUAL(0, panel.invalidWindows);
        TEST_ASSERT_EQUAL(0, panel.writes);
        TEST_ASSERT_EQUAL(0, panel.panelRotations);
    }

    void test_standby_preserves_frozen_frame_and_dirty_ownership() {
        for (const auto orientation: {ui::Orientation::Portrait, ui::Orientation::Landscape})
            for (const auto kind: {standby::Kind::life, standby::Kind::maze, standby::Kind::reaction})
                checkStandby(kind, orientation);
    }

    void test_standby_entry_and_reseed_clear_the_panel_once() {
        constexpr uint16_t stale = 0xDEAD;
        for (const auto orientation: {ui::Orientation::Portrait, ui::Orientation::Landscape}) {
            testgfx::Panel panel(410, 502);
            ui::Context ui(panel);
            const auto theme = ui::themes::defaultTheme();
            ui.setTheme(theme);
            ui.setOrientation(orientation);
            screens::StandbyScreen screen;
            for (const uint32_t seedTime: {1200U, 2400U}) {
                // The second seed is drawn on the same screen, with no external UI invalidation.
                screen.begin(ui, seedTime, 3, 19, standby::Kind::maze);
                std::fill(panel.pixels.begin(), panel.pixels.end(), stale);
                const int before = panel.transfers;
                screen.draw(ui);
                const int transfers = panel.transfers - before;
                // A seeded maze adds only its tiny starting cell to the one full-panel clear.
                TEST_ASSERT_GREATER_OR_EQUAL(testgfx::transfersFor(panel.height()), transfers);
                TEST_ASSERT_LESS_OR_EQUAL(testgfx::transfersFor(panel.height()) + 2, transfers);
                for (const uint16_t pixel: panel.pixels)
                    TEST_ASSERT_NOT_EQUAL(stale, pixel);
            }
            TEST_ASSERT_EQUAL(0, panel.invalidWindows);
            TEST_ASSERT_EQUAL(0, panel.writes);
        }
    }
} // namespace

void runAmoledStandbyTests() {
    RUN_TEST(test_standby_preserves_frozen_frame_and_dirty_ownership);
    RUN_TEST(test_standby_entry_and_reseed_clear_the_panel_once);
}
