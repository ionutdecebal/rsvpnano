#include <unity.h>
#include "ui/screens/ModernScreens.h"
#include "ui/screens/LibraryScreen.h"
#include "ui/screens/ChaptersScreen.h"
#include "ui/ClippedGfx.h"
#include "ui/SansFont.h"
#include "text/Utf8Text.h"
#include "ui/screens/ModernReaderChrome.h"
#include "ui/screens/OnboardingScreen.h"
#include "ui/screens/OnboardingCopy.h"
#include "ui/screens/TouchCheckScreen.h"

namespace BookLibrary {
    std::string_view displayName(const Entry& book) { return book.title; }
    bool isArticle(const Entry& book) { return std::string_view{book.path}.starts_with("/library/articles/"); }
}

namespace {
    class BoundsGfx : public Arduino_GFX {
    public:
        BoundsGfx(int16_t width = 640, int16_t height = 172) : Arduino_GFX(width, height), expected{0, 0, width, height} {}
        bool outside = false;
        ui::Rect expected{0, 0, 640, 172};
        void check(int x, int y, int w, int h) {
            if (w > 0 && h > 0 && (x < expected.x || y < expected.y || x + w > expected.x + expected.w
                                  || y + h > expected.y + expected.h)) outside = true;
        }
        void drawPixel(int16_t x, int16_t y, uint16_t color) override {
            check(x, y, 1, 1); Arduino_GFX::drawPixel(x, y, color);
        }
        void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) override {
            check(x, y, w, h); Arduino_GFX::fillRect(x, y, w, h, color);
        }
        void fillRoundRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t r, uint16_t color) override {
            check(x, y, w, h); Arduino_GFX::fillRoundRect(x, y, w, h, r, color);
        }
    };
    ui::TouchContact contact;
    ui::TouchSampleResult poll(ui::TouchContact& out) { out = contact; return ui::TouchSampleResult::Contact; }
    struct Fixture {
        BoundsGfx gfx;
        ui::Context ui{gfx};
        ui::themes::Theme theme = ui::themes::defaultTheme();
        screens::Screen screen = screens::Screen::Read;
        uint32_t time = 0;
        Fixture() {
            contact = {};
            ui.setTheme(theme); ui.setModernPresentation(true);
            ui.setTouchSource({.surface = {640, 172}, .poll = &poll});
        }
        template<typename Draw> screens::Action frame(Draw draw, bool down = false, uint16_t x = 0, uint16_t y = 0) {
            contact = {down, x, y}; ui.pollTouch(time += 25);
            ui.beginFrame(static_cast<uint8_t>(screen));
            auto action = draw(); ui.endFrame();
            TEST_ASSERT_FALSE(gfx.outside);
            return action;
        }
        template<typename Draw> screens::Action tap(Draw draw, uint16_t x, uint16_t y) {
            frame(draw, true, x, y); return frame(draw, false, x, y);
        }
    };
}

void setUp() {}
void tearDown() {}

void test_proportional_utf8_layout_is_measured_and_clipped() {
    Fixture f;
    const auto& font = ui::sans::font(26);
    TEST_ASSERT_LESS_THAN(ui::sans::width("WWW", font), ui::sans::width("iii", font));
    const auto* j = ui::sans::glyph(font, 'j');
    TEST_ASSERT_TRUE(j->left < 0);
    TEST_ASSERT_GREATER_OR_EQUAL(j->advance - j->left, ui::sans::width("j", font));
    for (const auto text : {"A very long title that needs several lines and an ellipsis",
                            "Über außergewöhnliche Bücher und große Wörter", "Supercalifragilisticexpialidocious"}) {
        const ui::Rect rect{88, 48, 144, 66};
        auto layout = f.ui.prepareText(rect, text, 3, ui::TextAlign::Start, 2);
        TEST_ASSERT_FALSE(layout.lines.empty());
        TEST_ASSERT_LESS_OR_EQUAL(2, layout.lines.size());
        for (const auto& line : layout.lines) {
            TEST_ASSERT_EQUAL(26, line.proportionalSize);
            TEST_ASSERT_LESS_OR_EQUAL(rect.w, ui::sans::width(line.text, font));
            TEST_ASSERT_TRUE(line.ink.y >= rect.y && line.ink.y + line.ink.h <= rect.y + rect.h);
        }
        f.gfx.expected = rect;
        f.ui.drawText(f.gfx, layout, 0xffff);
        TEST_ASSERT_FALSE(f.gfx.outside);
    }
}

void test_modern_palette_is_opt_in_and_preserves_custom_themes() {
    TEST_ASSERT_TRUE(screens::modern::enabled(640, 172, screens::Screen::Library));
    TEST_ASSERT_FALSE(screens::modern::enabled(640, 172, screens::Screen::Reader));
    TEST_ASSERT_TRUE(screens::modern::enabled(640, 172, screens::Screen::StorageEncryption));
    TEST_ASSERT_TRUE(screens::modern::enabled(640, 172, screens::Screen::WifiConnect));
    TEST_ASSERT_FALSE(screens::modern::enabled(320, 172, screens::Screen::Library));
    Fixture f;
    TEST_ASSERT_EQUAL(ui::modern::inkColors().background, f.ui.color(ui::themes::Background));
    TEST_ASSERT_EQUAL(ui::modern::inkColors().guideFocus, f.ui.readingFocusColor());
    f.ui.setModernPresentation(false);
    TEST_ASSERT_EQUAL(f.theme.definition.colors.background, f.ui.color(ui::themes::Background));
    auto classic = f.ui.prepareText({0, 0, 200, 44}, "Reader", 2);
    TEST_ASSERT_EQUAL(0, classic.lines.front().proportionalSize);
    f.ui.setModernReadingPalette(true);
    TEST_ASSERT_EQUAL(ui::modern::inkColors().background, f.ui.color(ui::themes::Background));
    TEST_ASSERT_FALSE(f.ui.modernPresentation());
    TEST_ASSERT_EQUAL(17, f.ui.prepareText({0, 0, 200, 44}, "Reader", 2).lines.front().proportionalSize);
    f.ui.setModernReadingPalette(false);
    f.theme.definition.colors.background = 0x1357;
    f.ui.setModernPresentation(true);
    TEST_ASSERT_EQUAL(0x1357, f.ui.color(ui::themes::Background));
    TEST_ASSERT_EQUAL(f.theme.definition.colors.accent, f.ui.readingFocusColor());
}

void test_explicit_line_breaks_do_not_rewrap_the_previous_word() {
    Fixture f;
    auto layout = f.ui.prepareText({16, 52, 608, 66}, "One complete line\nAnd another line", 2, ui::TextAlign::Start, 2);
    TEST_ASSERT_EQUAL(2, layout.lines.size());
    TEST_ASSERT_EQUAL_STRING("One complete line", layout.lines[0].text.c_str());
    TEST_ASSERT_EQUAL_STRING("And another line", layout.lines[1].text.c_str());
}

void test_menu_routes_brightness_bounds_and_unchanged_frame() {
    Fixture f;
    settings::InterfaceSettings interface;
    const auto draw = [&] { return screens::modern::menu(f.ui, "A book", "An author", 42, true, interface, f.screen); };
    f.frame(draw);
    const auto writes = f.gfx.writes;
    f.frame(draw);
    TEST_ASSERT_EQUAL(writes, f.gfx.writes);
    f.tap(draw,608,8);TEST_ASSERT_EQUAL(70,static_cast<int>(interface.brightnessPercent));
    TEST_ASSERT_EQUAL(static_cast<int>(screens::Action::Resume),static_cast<int>(f.tap(draw,100,140)));
    f.tap(draw,440,84);TEST_ASSERT_EQUAL(static_cast<int>(screens::Screen::Library),static_cast<int>(f.screen));
    settings::ReadingSettings reading;f.screen=screens::Screen::QuickControls;
    const auto controls=[&]{return screens::modern::quickControls(f.ui,reading,interface,f.screen);};
    f.frame(controls);f.tap(controls,606,8);TEST_ASSERT_EQUAL(100,static_cast<int>(interface.brightnessPercent));
    f.frame(controls,true,606,86);f.frame(controls,true,560,171);TEST_ASSERT_EQUAL(5,static_cast<int>(interface.brightnessPercent));
    f.frame(controls,true,560,0);TEST_ASSERT_EQUAL(100,static_cast<int>(interface.brightnessPercent));f.frame(controls,false,560,0);
    f.tap(controls,540,70);TEST_ASSERT_EQUAL(310,static_cast<int>(reading.wpm));
    f.tap(controls,286,70);TEST_ASSERT_EQUAL(300,static_cast<int>(reading.wpm));
    TEST_ASSERT_EQUAL(static_cast<int>(screens::Action::OpenPage),static_cast<int>(f.tap(controls,180,44)));
    TEST_ASSERT_EQUAL(static_cast<int>(screens::Action::OpenDictionary),static_cast<int>(f.tap(controls,50,127)));
    TEST_ASSERT_EQUAL(static_cast<int>(screens::Action::OpenDictionary),static_cast<int>(f.tap(controls,180,127)));
    f.tap(controls,65,44);TEST_ASSERT_EQUAL(static_cast<int>(screens::Screen::Read),static_cast<int>(f.screen));

}

void test_filled_slider_uses_whole_tile_and_commits_after_drag() {
    Fixture f;
    int delay=200, tracking=0;
    bool changed=false, adjacent=false;
    const auto draw=[&] {
        changed=f.ui.slider({12,49,198,50},"Long words",delay,0,600,50," ms");
        adjacent|=ui::modern::button(f.ui,{222,49,198,50},"Adjacent");
        f.ui.slider({420,110,208,40},"Tracking",tracking,-2,3,1);
        return screens::Action::None;
    };
    f.frame(draw);
    f.frame(draw,true,12,50); // top-left of the tile is part of the slider
    TEST_ASSERT_FALSE(changed);TEST_ASSERT_EQUAL(200,delay);
    f.frame(draw,true,300,60); // drift over a neighbour retains slider capture
    TEST_ASSERT_EQUAL(200,delay);TEST_ASSERT_FALSE(adjacent);
    f.frame(draw,false,300,60);
    TEST_ASSERT_TRUE(changed);TEST_ASSERT_EQUAL(600,delay);TEST_ASSERT_FALSE(adjacent);
    f.tap(draw,12,98);TEST_ASSERT_EQUAL(0,delay); // bottom edge works too
    f.tap(draw,111,75);TEST_ASSERT_EQUAL(300,delay);
    f.tap(draw,420,110);TEST_ASSERT_EQUAL(-2,tracking);
    f.tap(draw,627,149);TEST_ASSERT_EQUAL(3,tracking);
}

void test_library_arrows_wrap_without_opening_and_details_preserve_selection() {
    Fixture f;
    f.screen = screens::Screen::Library;
    screens::LibraryScreen library;
    const BookLibrary::Entry a{.title = "The Left Hand of Darkness", .author = "Ursula K. Le Guin"};
    const BookLibrary::Entry b{.title = "A much longer title which needs to wrap over two lines", .author = "Another author"};
    const std::vector<screens::LibraryItem> books{{&a, "Chapter one", 42}, {&b, "Chapter two", 17}};
    const auto draw = [&] { return library.draw(f.ui, books, f.time, f.screen); };
    f.frame(draw);
    TEST_ASSERT_EQUAL(static_cast<int>(screens::Action::None), static_cast<int>(f.tap(draw, 434, 100)));
    TEST_ASSERT_EQUAL(1, library.selectedIndex());
    f.tap(draw, 434, 100);
    TEST_ASSERT_EQUAL(0, library.selectedIndex());
    f.tap(draw, 40, 100);
    TEST_ASSERT_EQUAL(1, library.selectedIndex());
    f.tap(draw, 550, 110); f.frame(draw);
    TEST_ASSERT_EQUAL(1, library.selectedIndex());
    TEST_ASSERT_EQUAL(static_cast<int>(screens::Action::OpenBook), static_cast<int>(f.tap(draw, 550, 68)));
    f.tap(draw, 60, 22); f.frame(draw);
    TEST_ASSERT_EQUAL(1, library.selectedIndex());
    TEST_ASSERT_EQUAL(static_cast<int>(screens::Screen::Library), static_cast<int>(f.screen));
}

void test_empty_library_has_no_open_action_and_can_add_books() {
    Fixture f;
    f.screen = screens::Screen::Library;
    screens::LibraryScreen library;
    const auto draw = [&] { return library.draw(f.ui, {}, f.time, f.screen); };
    f.frame(draw);
    TEST_ASSERT_EQUAL(static_cast<int>(screens::Action::None), static_cast<int>(f.tap(draw, 434, 100)));
    f.tap(draw, 530, 100);
    TEST_ASSERT_EQUAL(static_cast<int>(screens::Screen::Connections), static_cast<int>(f.screen));
}

void test_chapter_browsing_only_seeks_on_explicit_open() {
    Fixture f;
    f.screen = screens::Screen::Chapters;
    screens::ChaptersScreen chapters;
    ReadingSession reader;
    const std::vector<std::string> words(30, "word");
    ReadingLoop::setWords(reader, words, 0);
    const std::array<ChapterMarker, 3> markers{{{"First", 0}, {"Second", 10}, {"Third", 20}}};
    settings::ReadingSettings settings;
    const auto draw = [&] { return chapters.draw(f.ui, markers, reader, settings, f.time, f.screen); };
    f.frame(draw);
    f.tap(draw, 414, 138);
    TEST_ASSERT_EQUAL(0, reader.state.wordIndex);
    f.tap(draw, 414, 138);
    TEST_ASSERT_EQUAL(0, reader.state.wordIndex);
    TEST_ASSERT_EQUAL(static_cast<int>(screens::Action::Resume), static_cast<int>(f.tap(draw, 550, 140)));
    TEST_ASSERT_EQUAL(20, reader.state.wordIndex);
}

void test_settings_and_connections_keep_existing_actions() {
    Fixture f;
    f.screen = screens::Screen::Settings;
    auto settings = [&] { return screens::settings(f.ui, f.screen); };
    f.frame(settings); f.tap(settings, 100, 140);
    TEST_ASSERT_EQUAL(static_cast<int>(screens::Screen::Connections), static_cast<int>(f.screen));
    auto connections = [&] { return screens::modern::hub(f.ui, f.screen); };
    f.frame(connections);
    TEST_ASSERT_EQUAL(static_cast<int>(screens::Action::CompanionSync), static_cast<int>(f.tap(connections, 320, 75)));
    TEST_ASSERT_EQUAL(static_cast<int>(screens::Action::OpenDictionary), static_cast<int>(f.tap(connections, 520, 75)));
    TEST_ASSERT_EQUAL(static_cast<int>(screens::Action::UsbTransfer), static_cast<int>(f.tap(connections, 130, 138)));
    TEST_ASSERT_EQUAL(static_cast<int>(screens::Action::RssRefresh), static_cast<int>(f.tap(connections, 320, 138)));
    f.tap(connections,520,138);TEST_ASSERT_EQUAL(static_cast<int>(screens::Screen::DictionaryHelp),static_cast<int>(f.screen));
    f.screen = screens::Screen::Device;
    auto device = [&] { return screens::device(f.ui, true, 5, settings::NvsEncryptionState::Available, f.screen); };
    f.frame(device);
    TEST_ASSERT_EQUAL(static_cast<int>(screens::Action::StorageStatus), static_cast<int>(f.tap(device,70,77)));
    TEST_ASSERT_EQUAL(static_cast<int>(screens::Screen::Device),static_cast<int>(f.screen));
    TEST_ASSERT_EQUAL(static_cast<int>(screens::Action::PowerOff),static_cast<int>(f.tap(device,550,140)));

}

void test_preview_fonts_navigation_symbols_and_two_line_book_title() {
    TEST_ASSERT_TRUE(ui::sans::supports("↑ ↓ ↶ ⌫"));
    Fixture f;
    for (const auto label : {"Back to book ↗", "Guides →", "← Visibility", "−"}) {
        TEST_ASSERT_TRUE(ui::sans::supports(label));
        const auto text = f.ui.prepareText({0, 0, 300, 43}, label, 13);
        TEST_ASSERT_EQUAL(13, text.lines.front().proportionalSize);
    }
    const auto title = f.ui.prepareText({84, 48, 312, 72}, "The Left Hand of Darkness", 28,
                                       ui::TextAlign::Start, 2);
    TEST_ASSERT_EQUAL(2, title.lines.size());
    TEST_ASSERT_EQUAL_STRING("The Left Hand of", title.lines[0].text.c_str());
    TEST_ASSERT_EQUAL_STRING("Darkness", title.lines[1].text.c_str());
    for (const auto size : {11, 13, 14, 15, 17, 21, 26, 28, 39, 66})
        TEST_ASSERT_EQUAL(size, ui::sans::font(size).size);
}

void test_reader_controls_respect_visibility_and_do_not_cover_the_word() {
    using namespace screens::modern;
    settings::ReadingSettings config;
    const auto control = [&](int x, int y, bool playing = false, bool book = true) {
        return static_cast<int>(readerControlAt(x, y, config, playing, book));
    };
    for (int x = 0; x < 640; x += 4)
        for (int y = 36; y < 120; y += 4)
            TEST_ASSERT_EQUAL(static_cast<int>(ReaderControl::None), control(x, y));
    TEST_ASSERT_EQUAL(static_cast<int>(ReaderControl::None),control(310,155));
    TEST_ASSERT_EQUAL(static_cast<int>(ReaderControl::None),control(450,155));
    TEST_ASSERT_EQUAL(static_cast<int>(ReaderControl::None),control(50,155));
    TEST_ASSERT_EQUAL(static_cast<int>(ReaderControl::None),control(560,18));
    config.arrowsVisibility=settings::Visibility::always;
    TEST_ASSERT_EQUAL(static_cast<int>(ReaderControl::None),control(160,155,true));
    TEST_ASSERT_EQUAL(static_cast<int>(ReaderControl::None),control(120,122,true));
    config.arrowsVisibility=settings::Visibility::never;
    TEST_ASSERT_EQUAL(static_cast<int>(ReaderControl::None),control(50,155,true));
    Fixture f;
    for (bool playing : {false, true}) {
        f.ui.beginFrame(0);
        readerChrome(f.ui, {.chapter="A long chapter title", .footer="42%", .batteryLabel="84%", .reading=playing,
                            .percent=42}, config, {{true, 3.9f, 84}, 0, false});
        f.ui.endFrame();
        TEST_ASSERT_FALSE(f.gfx.outside);
    }
}

void test_preview_clipper_contains_native_rasterizer_writes() {
    BoundsGfx output;
    output.expected = {16, 52, 328, 76};
    ui::ClippedGfx clipped(output, output.expected);
    clipped.writePixelPreclipped(-1, 5, 0xffff);
    clipped.writePixelPreclipped(20, 54, 0xffff);
    clipped.writeFastHLine(0, 60, 640, 0xffff);
    clipped.writeFastVLine(30, 0, 172, 0xffff);
    clipped.writeFillRectPreclipped(0, 0, 640, 172, 0xffff);
    TEST_ASSERT_FALSE(output.outside);
    TEST_ASSERT_GREATER_THAN(0, output.writes);
}

void test_startup_status_is_bounded_and_skips_unchanged_frames() {
    Fixture f;
    f.ui.setStartupPresentation(true);
    screens::status(f.ui, "Opening book", "The Discreet Charm of Long Titles and Other Unexpected Adventures",
                    "Reading your saved position", 42);
    TEST_ASSERT_FALSE(f.gfx.outside);
    TEST_ASSERT_TRUE(f.ui.modernPresentation());
    const auto writes = f.gfx.writes;
    screens::status(f.ui, "Opening book", "The Discreet Charm of Long Titles and Other Unexpected Adventures",
                    "Reading your saved position", 42);
    TEST_ASSERT_EQUAL(writes, f.gfx.writes);
    screens::status(f.ui, "Book failed", "A book", "Check SD card");
    TEST_ASSERT_GREATER_THAN(writes, f.gfx.writes);
    TEST_ASSERT_FALSE(f.gfx.outside);
    for (const int progress : {-100, -1, 0, 1, 99, 100, 1000}) {
        screens::status(f.ui, "A very long status title that must wrap safely", "", "", progress);
        TEST_ASSERT_FALSE(f.gfx.outside);
    }
}

void test_startup_presentation_does_not_leak_into_runtime_status() {
    Fixture f;
    TEST_ASSERT_FALSE(f.ui.startupPresentation());
    f.ui.setStartupPresentation(true);
    screens::status(f.ui, "Storage", "Checking");
    TEST_ASSERT_TRUE(f.ui.modernPresentation());
    f.ui.setStartupPresentation(false);
    screens::status(f.ui, "Ready");
    TEST_ASSERT_FALSE(f.ui.startupPresentation());
    TEST_ASSERT_TRUE(f.ui.modernPresentation());
    TEST_ASSERT_EQUAL(ui::modern::inkColors().background, f.ui.color(ui::themes::Background));
    TEST_ASSERT_FALSE(f.gfx.outside);
}

void test_startup_respects_custom_theme_and_other_display_sizes() {
    Fixture f;
    f.theme.definition.colors.background = 0x1357;
    f.ui.setStartupPresentation(true);
    screens::status(f.ui, "Storage", "Checking");
    TEST_ASSERT_EQUAL(0x1357, f.ui.color(ui::themes::Background));
    BoundsGfx small(320, 172);
    ui::Context ui(small);
    ui.setTheme(f.theme);
    ui.setStartupPresentation(true);
    screens::status(ui, "Ready");
    TEST_ASSERT_FALSE(ui.modernPresentation());
    TEST_ASSERT_FALSE(small.outside);
}


void test_focus_shortcut_only_exists_during_a_session() {
    using namespace screens::modern;
    settings::ReadingSettings config;
    TEST_ASSERT_EQUAL(static_cast<int>(ReaderControl::None),static_cast<int>(readerControlAt(370,18,config,false,true,false)));
    TEST_ASSERT_EQUAL(static_cast<int>(ReaderControl::Focus),static_cast<int>(readerControlAt(370,18,config,false,true,true)));
}

void test_same_screen_surface_change_repaints_once() {
    Fixture f;
    const auto draw=[&](uint8_t surface) {
        f.ui.beginFrame(42,surface);f.ui.label({500,0,130,36},"84%",13);f.ui.endFrame();
    };
    draw(0);const auto first=f.gfx.writes;draw(0);TEST_ASSERT_EQUAL(first,f.gfx.writes);
    draw(1);TEST_ASSERT_GREATER_THAN(first,f.gfx.writes);
    const auto page=f.gfx.writes;draw(0);TEST_ASSERT_GREATER_THAN(page,f.gfx.writes);
    const auto restored=f.gfx.writes;draw(0);TEST_ASSERT_EQUAL(restored,f.gfx.writes);
}

void test_modern_keyboard_uses_full_lcd_for_legacy_content_rectangles() {
    Fixture f;std::string value;ui::KeyboardState state;
    ui::KeyboardAction result=ui::KeyboardAction::None;
    const auto draw=[&] {result=f.ui.keyboard({8,8,624,156},value,4,state,"Wi-Fi password",true);return screens::Action::None;};
    f.frame(draw);f.tap(draw,26,60);TEST_ASSERT_EQUAL_STRING("q",value.c_str());
    f.tap(draw,430,18);TEST_ASSERT_TRUE(state.passwordVisible);f.tap(draw,430,18);TEST_ASSERT_FALSE(state.passwordVisible);
    f.tap(draw,550,18);f.tap(draw,26,60);TEST_ASSERT_EQUAL_STRING("q1",value.c_str());
    f.tap(draw,610,18);f.tap(draw,26,150);TEST_ASSERT_EQUAL_STRING("q1\\",value.c_str());
    f.tap(draw,504,18);f.tap(draw,26,150);f.tap(draw,26,60);TEST_ASSERT_EQUAL_STRING("q1\\Q",value.c_str());
    f.tap(draw,80,60);TEST_ASSERT_EQUAL(4,value.size());
    f.tap(draw,532,104);TEST_ASSERT_EQUAL_STRING("q1\\",value.c_str());
    f.tap(draw,600,150);TEST_ASSERT_EQUAL(static_cast<int>(ui::KeyboardAction::Submit),static_cast<int>(result));
    f.tap(draw,600,104);TEST_ASSERT_EQUAL(static_cast<int>(ui::KeyboardAction::Cancel),static_cast<int>(result));
}

void test_encryption_retains_acknowledgement_and_back_route() {
    Fixture f;f.screen=screens::Screen::StorageEncryption;
    const auto draw=[&] {return screens::storageEncryption(f.ui,settings::NvsEncryptionState::Available,f.screen);};
    f.frame(draw);f.tap(draw,550,22);f.frame(draw);
    TEST_ASSERT_EQUAL(static_cast<int>(screens::Action::None),static_cast<int>(f.tap(draw,490,138)));
    f.tap(draw,490,76);
    TEST_ASSERT_EQUAL(static_cast<int>(screens::Action::EnableStorageEncryption),static_cast<int>(f.tap(draw,490,138)));
    f.ui.invalidate();f.frame(draw);f.tap(draw,550,22);f.frame(draw);
    TEST_ASSERT_EQUAL(static_cast<int>(screens::Action::None),static_cast<int>(f.tap(draw,490,138)));
    f.tap(draw,40,22);TEST_ASSERT_EQUAL(static_cast<int>(screens::Screen::Device),static_cast<int>(f.screen));
}

void test_book_language_override_and_automatic_fallback() {
    ReadingSession session;session.metadata.locale="en";session.metadata.textRuns={{2,"fr"}};
    TEST_ASSERT_EQUAL_STRING("en",std::string(session.localeAt(0)).c_str());
    TEST_ASSERT_EQUAL_STRING("fr",std::string(session.localeAt(2)).c_str());
    session.state.overrides.locale="de";
    TEST_ASSERT_EQUAL_STRING("de",std::string(session.localeAt(0)).c_str());
    TEST_ASSERT_EQUAL_STRING("de",std::string(session.localeAt(2)).c_str());
    session.state.overrides.locale.reset();TEST_ASSERT_EQUAL_STRING("fr",std::string(session.localeAt(2)).c_str());
}


void test_onboarding_drafts_apply_only_when_requested(){
    Fixture f;settings::DeviceSettings config;screens::OnboardingScreen intro;
    config.reading.wpm=420;config.reading.holdToRead=true;
    intro.begin(f.ui,config,0);intro.next(3);f.screen=screens::Screen::Onboarding;
    const auto draw=[&]{return intro.draw(f.ui,config,f.time);};
    f.frame(draw);f.tap(draw,232,90);TEST_ASSERT_EQUAL(430,int(intro.draft().wpm));TEST_ASSERT_EQUAL(420,int(config.reading.wpm));
    f.tap(draw,560,20);TEST_ASSERT_EQUAL(4,intro.step());f.ui.invalidate();f.frame(draw);
    f.tap(draw,280,65);TEST_ASSERT_EQUAL(600,int(intro.draft().pacing.longWordDelayMs));TEST_ASSERT_EQUAL(200,int(config.reading.pacing.longWordDelayMs));
    f.tap(draw,416,20);TEST_ASSERT_EQUAL(5,intro.step());TEST_ASSERT_EQUAL(420,int(config.reading.wpm));
    intro.next(-1);f.ui.invalidate();f.frame(draw);f.tap(draw,560,20);
    TEST_ASSERT_EQUAL(430,int(config.reading.wpm));TEST_ASSERT_EQUAL(600,int(config.reading.pacing.longWordDelayMs));
    TEST_ASSERT_TRUE(config.reading.holdToRead);TEST_ASSERT_TRUE(intro.takeSettingsChanged());TEST_ASSERT_FALSE(intro.takeSettingsChanged());
    TEST_ASSERT_FALSE(config.interface.onboardingComplete); // Only App completion/skip persists the first-run marker.
}
void test_onboarding_uses_real_pacing_and_stops_at_sample_end(){
    Fixture f;settings::DeviceSettings config;config.reading.wpm=600;screens::OnboardingScreen intro;
    intro.begin(f.ui,config,0);intro.next(3);intro.play(0,false);
    intro.update(99);TEST_ASSERT_EQUAL(0,intro.sample().state.wordIndex);intro.update(100);TEST_ASSERT_EQUAL(1,intro.sample().state.wordIndex);
    intro.next();TEST_ASSERT_FALSE(intro.sample().playing);intro.play(200,true);
    const uint32_t delay=ReadingLoop::wordDurationMs(intro.sample(),intro.draft(),0);
    intro.update(200+delay-1);TEST_ASSERT_EQUAL(0,intro.sample().state.wordIndex);
    intro.update(200+delay);TEST_ASSERT_EQUAL(1,intro.sample().state.wordIndex);
    intro.play(1000,false);TEST_ASSERT_EQUAL(0,intro.sample().state.wordIndex);
    for(uint32_t i=1;i<=intro.sample().words.size();++i)intro.update(1000+i*100);
    TEST_ASSERT_FALSE(intro.sample().playing);TEST_ASSERT_EQUAL(intro.sample().words.size()-1,intro.sample().state.wordIndex);
    intro.play(UINT32_MAX-49,false);intro.update(49);TEST_ASSERT_EQUAL(0,intro.sample().state.wordIndex);
    intro.update(50);TEST_ASSERT_EQUAL(1,intro.sample().state.wordIndex);
    intro.close();TEST_ASSERT_FALSE(intro.active());TEST_ASSERT_FALSE(intro.sample().playing);
}
void test_all_introduction_steps_are_bounded_and_unchanged_frames_stay_idle(){
    Fixture f;settings::DeviceSettings config;screens::OnboardingScreen intro;
    for(auto locale:{"en","de","ro"}){
        config.interface.locale=locale;f.ui.setLocale(locale);intro.begin(f.ui,config,0);
        for(int step=0;step<11;++step){
            f.ui.invalidate();f.screen=screens::Screen::Onboarding;
            const auto draw=[&]{return intro.draw(f.ui,config,f.time);};
            f.frame(draw);const auto writes=f.gfx.writes;f.frame(draw);TEST_ASSERT_EQUAL(writes,f.gfx.writes);intro.next();
        }
    }
    f.ui.setLocale("en");
    TEST_ASSERT_EQUAL_STRING("Left button",std::string(screens::modern::copy(f.ui,screens::modern::Copy::LeftThree)).c_str());
}

void test_onboarding_copy_fits_native_fonts_without_ellipsis(){
    using C=screens::modern::Copy;Fixture f;
    TEST_ASSERT_TRUE(ui::sans::supports("ȘșȚț"));
    const auto fits=[&](ui::Rect rect,C key,uint8_t size,uint8_t lines){
        const auto text=screens::modern::copy(f.ui,key);const auto layout=f.ui.prepareText(rect,text,size,ui::TextAlign::Start,lines);
        TEST_ASSERT_FALSE(layout.lines.empty());for(const auto& line:layout.lines){TEST_ASSERT_NOT_EQUAL_MESSAGE(0,line.proportionalSize,std::string(text).c_str());TEST_ASSERT_EQUAL_MESSAGE(std::string::npos,line.text.find("..."),std::string(text).c_str());}
    };
    for(auto locale:{"en","de","ro"}){
        f.ui.setLocale(locale);fits({16,50,310,68},C::WelcomeTitle,26,2);fits({12,115,244,51},C::SpeedAdvice,13,3);
        for(auto key:{C::PacingAdvice,C::LongDelayAdvice,C::ComplexDelayAdvice,C::PunctuationDelayAdvice})fits({296,89,332,35},key,11,3);
        for(auto key:{C::SwipeBody,C::SwipeHelp,C::DictionaryLesson,C::MenuLibrary,C::MenuChapters,C::MenuFocus,C::MenuSettings,C::MenuBook,C::ReadingExplain,C::AppearanceExplain,C::DisplayExplain,C::ConnectionsExplain,C::DeviceExplain,C::HelpExplain,C::BrowseExplain,C::FindExplain,C::SearchExplain,C::PositionExplain,C::DemoBookReturn,C::ReadFocusExplain,C::TimerOnlyExplain})fits({16,94,224,70},key,13,4);
        for(auto key:{C::SdChecking,C::SdReady,C::SdUnavailable,C::SdFolders,C::SdWrite,C::SdTimeout})fits({16,51,310,28},key,21,1);
        for(auto key:{C::SdCheckingBody,C::SdReadyBody,C::SdUnavailableBody,C::SdFoldersBody,C::SdWriteBody,C::SdTimeoutBody,C::SdBusyBody})fits({16,84,310,51},key,13,3);
        fits({16,140,310,30},C::SdDemoNote,11,2);
        for(auto key:{C::SdOpen,C::SdPrepare,C::SdSave})fits({380,52,151,36},key,13,2);
        for(auto key:{C::SdPassed,C::SdWorking,C::SdIncomplete,C::SdFailed,C::SdNotRun})fits({536,52,88,36},key,11,2);
        for(auto key:{C::UploadWifiOne,C::UploadWifiTwo,C::UploadUsbOne,C::UploadUsbTwo})fits({168,91,456,20},key,13,1);
        for(auto key:{C::UploadWifiThree,C::UploadUsbThree})fits({168,135,456,34},key,13,2);
    }
}

void test_storage_check_waits_for_explicit_continue_and_blocks_completion(){
    using S=OnboardingStorage::State;Fixture f;settings::DeviceSettings config;screens::OnboardingScreen intro;
    intro.begin(f.ui,config,0);f.screen=screens::Screen::Onboarding;
    const auto draw=[&]{return intro.draw(f.ui,config,f.time);};
    f.frame(draw);TEST_ASSERT_TRUE(intro.canComplete());
    TEST_ASSERT_EQUAL(int(screens::Action::CheckOnboardingStorage),int(f.tap(draw,450,135)));
    const auto request=intro.storageRequest();TEST_ASSERT_TRUE(intro.storageVisible());TEST_ASSERT_FALSE(intro.canComplete());
    intro.next();intro.hardwarePlay(f.time);TEST_ASSERT_EQUAL(0,intro.step());TEST_ASSERT_FALSE(intro.sample().playing);
    intro.acceptStorageProgress(request,{S::Checking,1},f.time);TEST_ASSERT_EQUAL(1,intro.storageProgress().stage);
    intro.acceptStorageProgress(request,{S::Ready,2},f.time);TEST_ASSERT_TRUE(intro.canComplete());TEST_ASSERT_TRUE(intro.storageVisible());
    f.ui.invalidate();f.frame(draw);f.tap(draw,560,20);TEST_ASSERT_FALSE(intro.storageVisible());TEST_ASSERT_EQUAL(1,intro.step());
    intro.next(-1);TEST_ASSERT_TRUE(intro.storageVisible());TEST_ASSERT_EQUAL(int(S::Ready),int(intro.storageProgress().state));
    TEST_ASSERT_FALSE(config.interface.onboardingComplete); // Only the app's Finish/Skip handler persists it.
}

void test_storage_timeout_back_and_close_reject_late_results(){
    using S=OnboardingStorage::State;Fixture f;settings::DeviceSettings config;screens::OnboardingScreen intro;
    intro.begin(f.ui,config,0);intro.beginStorageCheck(UINT32_MAX-99);const auto expired=intro.storageRequest();
    intro.update(OnboardingStorage::kTimeoutMs-101);TEST_ASSERT_TRUE(intro.storageChecking());
    intro.update(OnboardingStorage::kTimeoutMs-100);TEST_ASSERT_EQUAL(int(S::TimedOut),int(intro.storageProgress().state));
    intro.acceptStorageProgress(expired,{S::Ready,2},OnboardingStorage::kTimeoutMs);TEST_ASSERT_FALSE(intro.canComplete());
    intro.beginStorageCheck(40000);const auto left=intro.storageRequest();intro.next(-1);
    intro.acceptStorageProgress(left,{S::Ready,2},40001);TEST_ASSERT_FALSE(intro.storageVisible());TEST_ASSERT_FALSE(intro.canComplete());
    intro.beginStorageCheck(40002);const auto current=intro.storageRequest();
    intro.acceptStorageProgress(left,{S::Ready,2},40003);TEST_ASSERT_TRUE(intro.storageChecking());
    intro.acceptStorageProgress(current,{S::Checking,1},40004);TEST_ASSERT_EQUAL(1,intro.storageProgress().stage);
    intro.close();intro.acceptStorageProgress(current,{S::Ready,2},40005);TEST_ASSERT_FALSE(intro.active());TEST_ASSERT_FALSE(intro.canComplete());
}

void test_failed_check_demo_and_pending_retry_do_not_overlap_storage_jobs(){
    using S=OnboardingStorage::State;Fixture f;settings::DeviceSettings config;screens::OnboardingScreen intro;
    intro.begin(f.ui,config,0);intro.beginStorageCheck(0);const auto request=intro.storageRequest();
    intro.acceptStorageProgress(request,{S::SaveFailed,2},1);intro.setStorageBusy(true);
    f.screen=screens::Screen::Onboarding;const auto draw=[&]{return intro.draw(f.ui,config,f.time);};
    f.frame(draw);TEST_ASSERT_EQUAL(int(screens::Action::None),int(f.tap(draw,560,20)));TEST_ASSERT_EQUAL(request,intro.storageRequest());
    f.tap(draw,400,20);TEST_ASSERT_FALSE(intro.storageVisible());TEST_ASSERT_EQUAL(1,intro.step());TEST_ASSERT_FALSE(intro.canComplete());
    intro.acceptStorageProgress(request,{S::Ready,2},f.time);TEST_ASSERT_FALSE(intro.canComplete());
    intro.next(-1);intro.next(-1);TEST_ASSERT_EQUAL(0,intro.step());TEST_ASSERT_FALSE(intro.storageVisible());
    f.ui.invalidate();f.frame(draw);TEST_ASSERT_EQUAL(int(screens::Action::None),int(f.tap(draw,450,135)));
    TEST_ASSERT_TRUE(intro.storageVisible());TEST_ASSERT_FALSE(intro.storageChecking());
    TEST_ASSERT_EQUAL(int(screens::Action::None),int(f.frame(draw)));
    intro.setStorageBusy(false);TEST_ASSERT_EQUAL(int(screens::Action::CheckOnboardingStorage),int(f.frame(draw)));
    const auto retry=intro.storageRequest();TEST_ASSERT_TRUE(intro.storageChecking());TEST_ASSERT_NOT_EQUAL(request,retry);
    TEST_ASSERT_EQUAL(int(screens::Action::None),int(f.frame(draw)));
    intro.acceptStorageProgress(retry,{S::Ready,2},f.time);TEST_ASSERT_TRUE(intro.canComplete());
}

void test_help_opens_book_upload_and_failed_checks_preserve_replay_completion(){
    using S=OnboardingStorage::State;Fixture f;settings::DeviceSettings config;screens::OnboardingScreen intro;
    f.screen=screens::Screen::Help;const auto help=[&]{return intro.help(f.ui,config.reading,f.screen);};
    f.frame(help);f.tap(help,450,76);TEST_ASSERT_EQUAL(int(screens::Screen::AddBooks),int(f.screen));
    const auto books=[&]{intro.books(f.ui);return screens::Action::None;};
    f.frame(books);f.tap(books,250,70);f.frame(books);const auto writes=f.gfx.writes;f.frame(books);TEST_ASSERT_EQUAL(writes,f.gfx.writes);
    config.interface.onboardingComplete=true;intro.begin(f.ui,config,0);intro.beginStorageCheck(0);
    intro.acceptStorageProgress(intro.storageRequest(),{S::Unavailable,0},0);intro.close();TEST_ASSERT_TRUE(config.interface.onboardingComplete);
}

void test_bottom_margin_belongs_to_last_button_and_slider_only(){
    Fixture f;bool pressed=false,above=false;int value=0;
    const auto draw=[&]{
        above|=ui::modern::button(f.ui,{378,87,250,33},"Above");
        pressed|=ui::modern::button(f.ui,{378,124,156,44},"Font");
        f.ui.slider({542,130,86,36},"Value",value,0,100,1);
        return screens::Action::None;
    };
    f.frame(draw);f.tap(draw,440,171);TEST_ASSERT_TRUE(pressed);TEST_ASSERT_FALSE(above);
    f.frame(draw,true,543,171);f.frame(draw,true,627,171);f.frame(draw,false,627,171);
    TEST_ASSERT_EQUAL(100,value);TEST_ASSERT_FALSE(above);
    pressed=false;f.tap(draw,440,122);TEST_ASSERT_FALSE(pressed);TEST_ASSERT_FALSE(above);
}

void test_touch_check_reaches_edges_and_returns_to_display(){
    Fixture f;screens::TouchCheckScreen check;f.screen=screens::Screen::TouchCheck;
    const auto draw=[&]{return check.draw(f.ui,f.screen);};
    f.frame(draw);
    for(auto y:{44,86,129,171})for(auto x:{0,106,319,533,639})f.frame(draw,true,x,y);
    f.frame(draw,false,639,171);const auto writes=f.gfx.writes;f.frame(draw);TEST_ASSERT_EQUAL(writes,f.gfx.writes);
    f.tap(draw,40,20);TEST_ASSERT_EQUAL(int(screens::Screen::InterfaceSettings),int(f.screen));
}

void test_dictionary_onboarding_and_chunk_hint_fit_all_languages() {
    Fixture f;
    using C = screens::modern::Copy;
    for(const auto* locale : {"en", "de", "ro"}) {
        f.ui.setLocale(locale);
        for(const auto [key, rect, lines, size] : {
            std::tuple{C::DictionaryOfflineBody,ui::Rect{268,70,356,42},3,11},
            std::tuple{C::DictionaryOnlineBody,ui::Rect{268,134,356,36},2,11},
            std::tuple{C::GuidesUnavailableChunks,ui::Rect{378,50,250,70},3,13}}) {
            const auto text = screens::modern::copy(f.ui,key);
            const auto layout = f.ui.prepareText(rect,text,size,ui::TextAlign::Start,lines);
            std::string joined;
            for(const auto& line : layout.lines) { if(!joined.empty())joined+=' '; joined+=line.text; }
            TEST_ASSERT_EQUAL_STRING_MESSAGE(std::string{text}.c_str(),joined.c_str(),locale);
        }
    }
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_dictionary_onboarding_and_chunk_hint_fit_all_languages);
    RUN_TEST(test_bottom_margin_belongs_to_last_button_and_slider_only);
    RUN_TEST(test_touch_check_reaches_edges_and_returns_to_display);
    RUN_TEST(test_storage_check_waits_for_explicit_continue_and_blocks_completion);
    RUN_TEST(test_storage_timeout_back_and_close_reject_late_results);
    RUN_TEST(test_failed_check_demo_and_pending_retry_do_not_overlap_storage_jobs);
    RUN_TEST(test_help_opens_book_upload_and_failed_checks_preserve_replay_completion);
    RUN_TEST(test_onboarding_copy_fits_native_fonts_without_ellipsis);
    RUN_TEST(test_onboarding_drafts_apply_only_when_requested);
    RUN_TEST(test_onboarding_uses_real_pacing_and_stops_at_sample_end);
    RUN_TEST(test_all_introduction_steps_are_bounded_and_unchanged_frames_stay_idle);
    RUN_TEST(test_focus_shortcut_only_exists_during_a_session);
    RUN_TEST(test_same_screen_surface_change_repaints_once);
    RUN_TEST(test_modern_keyboard_uses_full_lcd_for_legacy_content_rectangles);
    RUN_TEST(test_encryption_retains_acknowledgement_and_back_route);
    RUN_TEST(test_book_language_override_and_automatic_fallback);
    RUN_TEST(test_proportional_utf8_layout_is_measured_and_clipped);
    RUN_TEST(test_modern_palette_is_opt_in_and_preserves_custom_themes);
    RUN_TEST(test_explicit_line_breaks_do_not_rewrap_the_previous_word);
    RUN_TEST(test_menu_routes_brightness_bounds_and_unchanged_frame);
    RUN_TEST(test_filled_slider_uses_whole_tile_and_commits_after_drag);
    RUN_TEST(test_library_arrows_wrap_without_opening_and_details_preserve_selection);
    RUN_TEST(test_empty_library_has_no_open_action_and_can_add_books);
    RUN_TEST(test_chapter_browsing_only_seeks_on_explicit_open);
    RUN_TEST(test_settings_and_connections_keep_existing_actions);
    RUN_TEST(test_preview_fonts_navigation_symbols_and_two_line_book_title);
    RUN_TEST(test_reader_controls_respect_visibility_and_do_not_cover_the_word);
    RUN_TEST(test_preview_clipper_contains_native_rasterizer_writes);
    RUN_TEST(test_startup_status_is_bounded_and_skips_unchanged_frames);
    RUN_TEST(test_startup_presentation_does_not_leak_into_runtime_status);
    RUN_TEST(test_startup_respects_custom_theme_and_other_display_sizes);
    return UNITY_END();
}
