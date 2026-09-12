#include <unity.h>

#include <algorithm>
#include <string>
#include <vector>

#include "Panel.h"
#include "reader/ReadingLoop.h"
#include "ui/screens/PageReaderScreen.h"
#include "ui/screens/ReaderLayout.h"

namespace {
    constexpr uint8_t kBitmap[]{0xA0, 0xE0, 0xA0, 0xE0, 0xA0, 0xE0, 0xA0, 0xFE, 0x82, 0xBA, 0xAA, 0xBA,
                                0x82, 0xFE, 0x82, 0xFE, 0xFF, 0xF8, 0x80, 0x08, 0xBA, 0xA8, 0xAA, 0xA8,
                                0xBA, 0xA8, 0x80, 0x08, 0xFF, 0xF8, 0x80, 0x08, 0xFF, 0xF8};
    constexpr ui::fonts::AlphaGlyph kGlyphs[]{
        {.xAdvance = 3},
        {.width = 3, .height = 7, .rowStride = 1, .xAdvance = 5, .yOffset = -5, .bitmapBytes = 7},
        {.bitmapOffset = 16,
         .width = 13,
         .height = 9,
         .rowStride = 2,
         .xAdvance = 9,
         .xOffset = -3,
         .yOffset = -10,
         .bitmapBytes = 18},
        {.bitmapOffset = 7, .width = 7, .height = 9, .rowStride = 1, .xAdvance = 9, .yOffset = -7, .bitmapBytes = 9},
    };
    constexpr ui::fonts::AlphaGlyphIdentity kIdentities[]{{' ', 0}, {'a', 1}, {'b', 2}, {0x65E5, 3}};
    constexpr ui::fonts::AlphaFont kFont{
        .bitmap = kBitmap,
        .glyphs = kGlyphs,
        .identities = kIdentities,
        .glyphCount = std::size(kGlyphs),
        .yAdvance = 9,
        .ascent = 7,
        .descent = 2,
        .pixelsPerEm = 9,
        .bitsPerPixel = 1,
    };
    constexpr ui::fonts::AlphaFont kTallFont = [] {
        auto font = kFont;
        font.yAdvance = 13;
        font.ascent = 9;
        font.descent = 4;
        font.pixelsPerEm = 13;
        return font;
    }();
    constexpr ui::fonts::AlphaGlyphIdentity kBidiIdentities[]{{' ', 0}, {'a', 1}, {0x05D0, 2}, {0x05D1, 3}};
    constexpr ui::fonts::AlphaFont kBidiFont = [] {
        auto font = kFont;
        font.identities = kBidiIdentities;
        return font;
    }();
    constexpr ui::fonts::AlphaFont kBidiTallFont = [] {
        auto font = kTallFont;
        font.identities = kBidiIdentities;
        return font;
    }();

    void checkPageHighlights(bool vertical, bool bidi = false) {
        for (uint8_t rotation = 0; rotation < 4; ++rotation) {
            testgfx::Panel panel{96, 112};
            ui::Context ui{panel};
            const auto theme = ui::themes::defaultTheme();
            ui.setTheme(theme);
            ui.setOrientation(static_cast<ui::Orientation>(rotation));
            ui::fonts::AlphaTextRenderer<640> text{panel};
            TEST_ASSERT_TRUE(text.begin());
            settings::TypographySettings typography;
            std::vector<std::string> words;
            for (int i = 0; i < 20; ++i)
                for (const char* word:
                     bidi ? std::array{"a", "א", "אב", "בא", "ב"} : std::array{"a", "b", "aa", "\xE6\x97\xA5", "ab"})
                    words.emplace_back(word);
            ReadingSession session;
            ReadingLoop::setWords(session, words, 0);
            if (vertical)
                session.metadata.writingMode = WritingMode::verticalRl;
            if (bidi) {
                session.metadata.baseDirection = TextDirection::rtl;
                session.metadata.requiredCapabilities = UnicodeText::CapabilityBidi;
            }
            screens::PageReader::State state;
            size_t selections = 0;
            const auto typeface = [&](size_t index) -> FontCatalog::Face {
                ++selections;
                return {std::cref(bidi ? (index % 3 == 1 ? kBidiTallFont : kBidiFont)
                                       : (index % 3 == 1 ? kTallFont : kFont)),
                        nullptr};
            };
            const ui::Rect area{3, 5, static_cast<int16_t>(ui.width() - 8), static_cast<int16_t>(ui.height() - 12)};
            const auto draw = [&] {
                ui.beginFrame(1);
                screens::PageReader::draw(state, ui, text, typeface, typography, 1, session, area);
                ui.endFrame();
            };
            draw();
            TEST_ASSERT_EQUAL(bidi, state.bidi);
            TEST_ASSERT_GREATER_THAN(2, state.lineCount);
            TEST_ASSERT_NOT_EQUAL(ui.color(ui::themes::ColorRole::Foreground), ui.color(ui::themes::ColorRole::Accent));
            TEST_ASSERT_GREATER_THAN(0, std::ranges::count(panel.pixels, ui.color(ui::themes::ColorRole::Accent)));
            TEST_ASSERT_GREATER_THAN(0, std::ranges::count(panel.pixels, ui.color(ui::themes::ColorRole::Foreground)));
            const size_t layoutSelections = selections;
            const auto* cachedWords = state.words.data();
            for (size_t highlighted = state.pageStart + 1; highlighted < state.pageEnd; ++highlighted) {
                ReadingLoop::seekTo(session, highlighted);
                draw();
                TEST_ASSERT_EQUAL(layoutSelections, selections);
                TEST_ASSERT_EQUAL_PTR(cachedWords, state.words.data());

                testgfx::Panel referencePanel{96, 112};
                ui::Context referenceUi{referencePanel};
                referenceUi.setTheme(theme);
                referenceUi.setOrientation(static_cast<ui::Orientation>(rotation));
                ui::fonts::AlphaTextRenderer<640> referenceText{referencePanel};
                TEST_ASSERT_TRUE(referenceText.begin());
                auto referenceState = state;
                for (auto& word: referenceState.words)
                    word.inkKnown = false;
                for (auto& line: referenceState.lines)
                    line.inkKnown = false;
                referenceUi.beginFrame(1);
                screens::PageReader::draw(referenceState, referenceUi, referenceText, typeface, typography, 1, session,
                                          area);
                referenceUi.endFrame();
                TEST_ASSERT_EQUAL_UINT16_ARRAY(referencePanel.pixels.data(), panel.pixels.data(), panel.pixels.size());
                TEST_ASSERT_EQUAL(0, panel.invalidWindows);
                TEST_ASSERT_EQUAL(0, panel.writes);
                TEST_ASSERT_EQUAL(0, panel.panelRotations);

                const int transfers = panel.transfers;
                draw();
                TEST_ASSERT_EQUAL(transfers, panel.transfers);
            }
        }
    }

    void test_horizontal_highlights_preserve_neighbouring_words_without_relayout() {
        checkPageHighlights(false);
    }

    void test_vertical_highlights_preserve_neighbouring_columns_without_relayout() {
        checkPageHighlights(true);
    }

    void test_bidi_highlights_preserve_exact_overhanging_ink() {
        checkPageHighlights(false, true);
    }

    void test_bidi_cached_positions_match_known_visual_coordinates() {
        testgfx::Panel panel{96, 64};
        ui::Context ui{panel};
        const auto theme = ui::themes::defaultTheme();
        ui.setTheme(theme);
        ui::fonts::AlphaTextRenderer<640> text{panel};
        TEST_ASSERT_TRUE(text.begin());
        settings::TypographySettings typography;
        typography.tracking = 0;
        const std::array<std::string, 3> words{"a", "אב", "a"};
        ReadingSession session;
        ReadingLoop::setWords(session, words, 0);
        session.metadata.baseDirection = TextDirection::rtl;
        session.metadata.requiredCapabilities = UnicodeText::CapabilityBidi;
        screens::PageReader::State state;
        const auto typeface = [](size_t) -> FontCatalog::Face {
            return {std::cref(kBidiFont), nullptr};
        };
        ui.beginFrame(1);
        screens::PageReader::draw(state, ui, text, typeface, typography, 1, session, {0, 0, 96, 64});
        ui.endFrame();
        TEST_ASSERT_EQUAL(1, state.lineCount);
        TEST_ASSERT_EQUAL(42, state.lines[0].x);
        TEST_ASSERT_EQUAL(34, state.lines[0].width);
        TEST_ASSERT_EQUAL(6, state.characters.size());
        constexpr int16_t expectedX[]{0, 5, 8, 17, 26, 29};
        for (size_t index = 0; index < state.characters.size(); ++index)
            TEST_ASSERT_EQUAL(expectedX[index], state.characters[index].x);

        Arduino_Canvas expected{96, 64, nullptr};
        expected.fillScreen(ui.color(ui::themes::Background));
        ui::fonts::AlphaTextRenderer<640> reference{expected};
        TEST_ASSERT_TRUE(reference.begin());
        reference.setFont(kBidiFont);
        reference.setTextColor(ui.color(ui::themes::Foreground), ui.color(ui::themes::Background));
        reference.drawCodepoint('a', 42, 11);
        reference.drawCodepoint(0x05D1, 50, 11);
        reference.drawCodepoint(0x05D0, 59, 11);
        reference.setTextColor(ui.color(ui::themes::Accent), ui.color(ui::themes::Background));
        reference.drawCodepoint('a', 71, 11);
        TEST_ASSERT_EQUAL_UINT16_ARRAY(expected.getFramebuffer(), panel.pixels.data(), panel.pixels.size());
    }

    void test_keyboard_input_box_uses_the_same_aligned_region_as_its_text() {
        for (const bool masked: {false, true}) {
            testgfx::Panel panel;
            ui::Context ui{panel};
            const auto theme = ui::themes::defaultTheme();
            ui.setTheme(theme);
            ui.setOrientation(ui::Orientation::Landscape);
            ui::KeyboardState keyboard;
            std::string value = "test password";
            const auto draw = [&] {
                ui.beginFrame(2);
                const auto action =
                    ui.keyboard({3, 5, static_cast<int16_t>(ui.width() - 8), static_cast<int16_t>(ui.height() - 12)},
                                value, 64, keyboard, "Password", masked);
                ui.endFrame();
                TEST_ASSERT_EQUAL(ui::KeyboardAction::None, action);
                TEST_ASSERT_EQUAL(0, panel.invalidWindows);
                TEST_ASSERT_EQUAL(0, panel.writes);
            };
            draw();
            TEST_ASSERT_GREATER_THAN(0, panel.transfers);
            const int transfers = panel.transfers;
            draw();
            TEST_ASSERT_EQUAL(transfers, panel.transfers);
            value += 'x';
            draw();
            TEST_ASSERT_GREATER_THAN(transfers, panel.transfers);
            TEST_ASSERT_EQUAL_STRING("test passwordx", value.c_str());
        }
    }

    void test_vertical_reader_region_does_not_clear_the_chrome() {
        using namespace screens::readerLayout;
        for (const auto [width, height]: {std::pair{448, 368}, std::pair{502, 410}, std::pair{480, 480},
                                          std::pair{600, 450}, std::pair{320, 172}}) {
            const ui::Rect area = readingArea(width, height, true);
            for (const ui::Rect chrome:
                 {portraitTopStrip(height), portraitBottomStrip(height, width), portraitChapterRect(height, width)}) {
                const auto overlap = ui::intersection(area, ui::rotateClockwise(chrome, height));
                TEST_ASSERT_TRUE(overlap.w == 0 || overlap.h == 0);
            }
        }
    }
} // namespace

void runAmoledReaderTests() {
    RUN_TEST(test_horizontal_highlights_preserve_neighbouring_words_without_relayout);
    RUN_TEST(test_vertical_highlights_preserve_neighbouring_columns_without_relayout);
    RUN_TEST(test_bidi_highlights_preserve_exact_overhanging_ink);
    RUN_TEST(test_bidi_cached_positions_match_known_visual_coordinates);
    RUN_TEST(test_keyboard_input_box_uses_the_same_aligned_region_as_its_text);
    RUN_TEST(test_vertical_reader_region_does_not_clear_the_chrome);
}
