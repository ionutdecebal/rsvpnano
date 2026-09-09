#include <unity.h>
#include "AppearanceChecks.h"
#include "ui/Layouts.h"
#include "ui/screens/ChaptersScreen.h"
#include "ui/screens/ReaderLayout.h"
#include "ui/screens/watch/Layout.h"

namespace {
    ui::TouchContact contact;
    ui::TouchSampleResult poll(ui::TouchContact& out) {
        out = contact;
        return ui::TouchSampleResult::Contact;
    }

    class BoundsGfx : public Arduino_GFX {
    public:
        using Arduino_GFX::Arduino_GFX;
        bool outside = false;
        void record(int x, int y, int w, int h) {
            outside |= w < 0 || h < 0 || x < 0 || y < 0 || x + w > width() || y + h > height();
        }
        void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t) override {
            record(x, y, w, h);
        }
        void fillRoundRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t, uint16_t) override {
            record(x, y, w, h);
        }
        void drawRoundRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t, uint16_t) override {
            record(x, y, w, h);
        }
        void drawLine(int16_t x, int16_t y, int16_t x2, int16_t y2, uint16_t) override {
            record(x, y, 1, 1);
            record(x2, y2, 1, 1);
        }
    };

    void test_screens_fit_every_watch_resolution() {
        constexpr std::array<ui::TouchSurface, 5> sizes{{{600, 450}, {480, 480}, {502, 410}, {448, 368}, {320, 172}}};
        for (const auto size: sizes) {
            BoundsGfx gfx(size.width, size.height);
            ui::Context ui(gfx);
            const auto theme = ui::themes::defaultTheme();
            ui.setTheme(theme);
            settings::ReadingSettings reading;
            settings::PacingSettings pacing;
            constexpr std::array screens{screens::Screen::Read, screens::Screen::Settings, screens::Screen::Device,
                                         screens::Screen::ReadingSettings, screens::Screen::PacingSettings};
            for (auto screen: screens) {
                ui.beginFrame(static_cast<uint8_t>(screen));
                switch (screen) {
                case screens::Screen::Read:
                    screens::read(ui, "The Left Hand of Darkness", "Ursula K. Le Guin", 42, screen);
                    break;
                case screens::Screen::Settings:
                    screens::settings(ui, screen);
                    break;
                case screens::Screen::Device:
                    screens::device(ui, true, 18, settings::NvsEncryptionState::Available, screen);
                    break;
                case screens::Screen::ReadingSettings:
                    screens::readingSettings(ui, reading, screen);
                    break;
                case screens::Screen::PacingSettings:
                    screens::pacingSettings(ui, pacing, screen);
                    break;
                default:
                    break;
                }
                ui.endFrame();
                TEST_ASSERT_FALSE_MESSAGE(gfx.outside, "watch screen painted outside the physical display");
            }
            screens::status(ui, "Preparing book", "The Left Hand of Darkness", "Preparing typography", 64);
            TEST_ASSERT_FALSE(gfx.outside);
        }
    }

    void test_carousel_swipes_are_directional_and_wrap() {
        ui::CarouselGesture gesture;
        const ui::Rect area{0, 0, 320, 172};
        ui::Touch event{ui::TouchStart, 200, 80};
        TEST_ASSERT_EQUAL(0, gesture.update(&event, area));
        event = {ui::TouchRelease, 120, 83};
        TEST_ASSERT_EQUAL(1, gesture.update(&event, area));
        TEST_ASSERT_EQUAL(0, gesture.update(&event, area));
        event = {ui::TouchStart, 100, 80};
        gesture.update(&event, area);
        event = {ui::TouchRelease, 110, 150};
        TEST_ASSERT_EQUAL(0, gesture.update(&event, area));
        TEST_ASSERT_EQUAL(5, ui::rotateIndex(0, 6, -1));
        TEST_ASSERT_EQUAL(0, ui::rotateIndex(5, 6, 1));
        TEST_ASSERT_EQUAL(0, ui::rotateIndex(0, 0, 1));
    }

    void test_rotary_uses_relative_delta_and_clamps() {
        Arduino_GFX gfx;
        ui::Context ui(gfx);
        const auto theme = ui::themes::defaultTheme();
        ui.setTheme(theme);
        ui.setTouchSource({.surface = {320, 172}, .poll = &poll});
        int value = 300;
        contact = {true, 100, 80};
        ui.pollTouch(100);
        ui.beginFrame(1);
        ui.rotary({50, 20, 200, 120}, value, 10, 1000, 10);
        ui.endFrame();
        contact = {true, 132, 80};
        ui.pollTouch(120);
        ui.beginFrame(1);
        ui.rotary({50, 20, 200, 120}, value, 10, 1000, 10);
        ui.endFrame();
        TEST_ASSERT_EQUAL(340, value);
        contact = {false, 132, 80};
        ui.pollTouch(140);
        ui.beginFrame(1);
        ui.rotary({50, 20, 200, 120}, value, 10, 1000, 10);
        ui.endFrame();
        value = 990;
        contact = {true, 100, 80};
        ui.pollTouch(200);
        ui.beginFrame(1);
        ui.rotary({50, 20, 200, 120}, value, 10, 1000, 10);
        ui.endFrame();
        contact = {true, 200, 80};
        ui.pollTouch(220);
        ui.beginFrame(1);
        ui.rotary({50, 20, 200, 120}, value, 10, 1000, 10);
        ui.endFrame();
        TEST_ASSERT_EQUAL(1000, value);
    }

    void test_paging_never_exposes_offscreen_hit_targets_and_resets() {
        Arduino_GFX gfx;
        ui::Context ui(gfx);
        const auto theme = ui::themes::defaultTheme();
        ui.setTheme(theme);
        ui.beginFrame(1);
        auto page = ui.pagedGrid({8, 48, 304, 116}, 8, 1, 56);
        TEST_ASSERT_EQUAL(1, page.count);
        TEST_ASSERT_EQUAL(0, page.item(7).w);
        TEST_ASSERT_GREATER_THAN(0, page.item(0).h);
        ui.endFrame();
        ui.setTouchSource({.surface = {320, 172}, .poll = &poll});
        contact = {true, 285, 146};
        ui.pollTouch(300);
        ui.beginFrame(1);
        ui.pagedGrid({8, 48, 304, 116}, 8, 1, 56);
        ui.endFrame();
        contact = {false, 285, 146};
        ui.pollTouch(320);
        ui.beginFrame(1);
        page = ui.pagedGrid({8, 48, 304, 116}, 8, 1, 56);
        ui.endFrame();
        TEST_ASSERT_EQUAL(1, page.first);
        ui.beginFrame(2);
        page = ui.pagedGrid({8, 48, 304, 116}, 8, 1, 56);
        ui.endFrame();
        TEST_ASSERT_EQUAL(0, page.first);
    }
    void test_reader_layout_fits_and_preserves_handedness() {
        constexpr std::array<ui::TouchSurface, 5> sizes{{{600, 450}, {480, 480}, {502, 410}, {448, 368}, {320, 172}}};
        for (const auto size: sizes) {
            BoundsGfx gfx(size.width, size.height);
            ui::Context ui(gfx);
            const auto theme = ui::themes::defaultTheme();
            ui.setTheme(theme);
            settings::ReadingSettings settings;
            const Board::Power::BatteryState battery{{true, 3.9f, 64}, 0, false};
            for (bool vertical: {false, true}) {
                for (bool left: {false, true}) {
                    settings.leftHanded = left;
                    ui.beginFrame(42);
                    ui.invalidate();
                    screens::readerLayout::chrome(ui,
                                                  {.vertical = vertical,
                                                   .chapter = "Chapter 12",
                                                   .locale = "en",
                                                   .footer = "64%",
                                                   .batteryLabel = "64%",
                                                   .overlay = "300 WPM",

                                                   .topState = 1,
                                                   .bottomState = 1},
                                                  settings, battery);
                    ui.endFrame();
                    TEST_ASSERT_FALSE(gfx.outside);
                    const auto area = screens::readerLayout::readingArea(size.width, size.height, vertical);
                    TEST_ASSERT_GREATER_THAN(0, area.w);
                    TEST_ASSERT_GREATER_THAN(0, area.h);
                    TEST_ASSERT_LESS_OR_EQUAL(size.width, area.x + area.w);
                    TEST_ASSERT_LESS_OR_EQUAL(size.height, area.y + area.h);
                }
            }
        }
    }

    void test_dock_reaches_all_four_destinations() {
        constexpr std::array destinations{screens::Screen::Read, screens::Screen::Settings, screens::Screen::Device,
                                          screens::Screen::FocusTimers};
        constexpr std::array<uint16_t, 4> x{70, 180, 230, 282};
        for (size_t i = 0; i < destinations.size(); ++i) {
            Arduino_GFX gfx;
            ui::Context ui(gfx);
            const auto theme = ui::themes::defaultTheme();
            ui.setTheme(theme);
            ui.setTouchSource({.surface = {320, 172}, .poll = &poll});
            auto screen = screens::Screen::Read;
            for (bool down: {true, false}) {
                contact = {down, x[i], 146};
                ui.pollTouch(down ? 100 : 130);
                ui.beginFrame(0);
                screens::detail::navigation(ui, screens::Screen::Read, screen);
                ui.endFrame();
            }
            TEST_ASSERT_EQUAL(destinations[i], screen);
        }
    }
    void test_chapter_selection_does_not_activate_until_center_tap() {
        Arduino_GFX gfx;
        ui::Context ui(gfx);
        const auto theme = ui::themes::defaultTheme();
        ui.setTheme(theme);
        ui.setTouchSource({.surface = {320, 172}, .poll = &poll});
        screens::ChaptersScreen chapters;
        ReadingSession reader;
        std::array<std::string, 15> words;
        words.fill("word");
        ReadingLoop::setWords(reader, words, 0);
        const std::array<ChapterMarker, 3> markers{{{"First", 0}, {"Second", 5}, {"Third", 10}}};
        settings::ReadingSettings settings;
        auto screen = screens::Screen::Chapters;
        uint32_t now = 0;
        const auto frame = [&](bool down, uint16_t x) {
            contact = {down, x, 60};
            now += 30;
            ui.pollTouch(now);
            ui.beginFrame(static_cast<uint8_t>(screen));
            const auto action = chapters.draw(ui, markers, reader, settings, now, screen);
            ui.endFrame();
            return action;
        };
        TEST_ASSERT_EQUAL(screens::Action::None, frame(true, 280));
        TEST_ASSERT_EQUAL(screens::Action::None, frame(false, 280));
        TEST_ASSERT_EQUAL(0, reader.state.wordIndex);
        frame(true, 160);
        TEST_ASSERT_EQUAL(screens::Action::Resume, frame(false, 160));
        TEST_ASSERT_EQUAL(5, reader.state.wordIndex);
        frame(true, 210);
        frame(true, 90);
        TEST_ASSERT_EQUAL(screens::Action::None, frame(false, 90));
        TEST_ASSERT_EQUAL(5, reader.state.wordIndex);
        frame(true, 160);
        TEST_ASSERT_EQUAL(screens::Action::Resume, frame(false, 160));
        TEST_ASSERT_EQUAL(10, reader.state.wordIndex);
    }
} // namespace

void setUp() {}
void tearDown() {}
void test_appearance_controls_fit_watch_displays() {
    for (const auto size: std::array<ui::TouchSurface, 5>{{{600, 450}, {480, 480}, {502, 410}, {448, 368}, {320, 172}}})
        appearanceChecks::layout(size.width, size.height);
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_screens_fit_every_watch_resolution);
    RUN_TEST(test_appearance_controls_fit_watch_displays);
    RUN_TEST(appearanceChecks::fourRotaries);
    RUN_TEST(appearanceChecks::wordTargets);
    RUN_TEST(test_carousel_swipes_are_directional_and_wrap);
    RUN_TEST(test_rotary_uses_relative_delta_and_clamps);
    RUN_TEST(test_paging_never_exposes_offscreen_hit_targets_and_resets);
    RUN_TEST(test_reader_layout_fits_and_preserves_handedness);
    RUN_TEST(test_dock_reaches_all_four_destinations);
    RUN_TEST(test_chapter_selection_does_not_activate_until_center_tap);
    return UNITY_END();
}
