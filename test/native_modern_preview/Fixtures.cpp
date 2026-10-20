#include "StorageFixture.h"
// Host-only hardware/catalog fixtures. Screen layouts and controls come from
// production sources. Reader words here are static raster specimens: this does
// not test the live reader's scheduling, shaping or phantom-word pipeline.
#include "ui/screens/ReaderScreen.h"
#include "ui/screens/ReaderLayout.h"
#include "fonts/LiterataFallbackAlpha4.h"
#include "text/Utf8Text.h"
#include "network/WifiConnection.h"
#include <freertos/task.h>

namespace net {
std::expected<void,std::error_code> connectStation(const char*,const char*,const WifiProgress&,uint32_t) {
    return std::unexpected(std::make_error_code(std::errc::timed_out));
}
void disconnect() {}
}

FontCatalog::FontCatalog() { families_.push_back({.id="literata", .label="Literata", .builtIn=true, .scriptMask=UnicodeText::ScriptLatin}); }
// Match the SD catalog's mutable per-family strike, so page/RSVP aliasing
// is exercised instead of hidden by immutable built-in font pointers.
ui::fonts::AlphaFont previewRuntimeFont;
size_t previewRuntimeSize = SIZE_MAX;
FontCatalog::Face FontCatalog::loadFace(size_t, size_t size) {
    size = std::min<size_t>(size, 3);
    if (previewRuntimeSize != size) {
        const auto generation = previewRuntimeFont.generation + 1;
        previewRuntimeFont = *ui::fonts::LiterataFallbackAlpha4_Sizes[size];
        previewRuntimeFont.generation = generation;
        previewRuntimeSize = size;
    }
    return {previewRuntimeFont};
}
bool focus::OrientationReader::begin() { available_ = true; return true; }
int previewSaveCalls = 0, previewPrefetchDelay = 0;
settings::SettingsStore::~SettingsStore() = default;
void settings::SettingsStore::acceptChanges() { ++previewSaveCalls; }
void settings::SettingsStore::acceptSecretChanges() {}

namespace screens {
    ReaderScreen::ReaderScreen(Arduino_GFX& gfx, settings::ReadingSettings& config)
        : width_(gfx.width()), height_(gfx.height()), text_(gfx), settings_(config), face_(fonts.loadFace(0, 0)) {
        text_.begin();
    }
    void ReaderScreen::resetTouch() { touching_ = false; touchIntent_ = TouchIntent::None; }
    bool ReaderScreen::appearance(ui::Context& ui, Screen& screen) {
        typography_ = settings_.typography;
        face_ = fonts.loadFace(0, typography_.fontSizeIndex);
        text_.setFont(face_.raster.get());
        return modernAppearance(ui, screen);
    }
    void FocusScreen::begin() { timers_ = focus::defaultTimers(); writable_ = true; orientation_.begin(); }
    bool FocusScreen::persist(const focus::Timers& timers) { timers_ = timers; return true; }
    void FocusScreen::edit(size_t index, bool creating, Screen& screen) {
        editIndex_ = index; creating_ = creating;
        draft_ = creating ? focus::defaultTimer() : timers_.timers[index];
        screen = Screen::FocusEditor;
    }
    Action FocusScreen::draw(ui::Context& ui, uint32_t now, Screen& screen) {
        if (ui.modernPresentation()) return drawModern(ui,now,screen);
        if (screen == Screen::FocusTimers) return drawTimers(ui, screen);
        if (screen == Screen::FocusEditor) drawEditor(ui, screen);
        if (screen == Screen::FocusSession && drawSession(ui, now)) screen = Screen::FocusTimers;
        return Action::None;
    }
}

namespace screens {
void FocusScreen::close() { session_.stop();manualActive_=false; }
void FocusScreen::drawNameEditor(ui::Context& ui,Screen& screen) {
    const auto action=ui.keyboard({0,0,640,172},draft_.name,focus::kMaxTimerNameBytes,keyboard_,ui.text(UiText::TimerName));
    if(action!=ui::KeyboardAction::None) screen=Screen::FocusEditor;
}
}

namespace screens {
bool FocusScreen::update(uint32_t now) {session_.update(now,focus::Orientation::Unknown);return session_.consumeCompletionCue();}
}


namespace ReadingProgress {
void save(ReadingSession&, Preferences&, bool, uint32_t) { ++previewSaveCalls; }
bool saveChapterTransition(ReadingSession&, Preferences&, const IndexedBookStore&,size_t,size_t,uint32_t) { return false; }
uint8_t percent(uint32_t index,uint32_t count) { return count<=1?0:static_cast<uint8_t>(std::min<uint64_t>(uint64_t(index)*100/(count-1),100)); }
}

namespace screens {
size_t ReaderScreen::fontChoice(size_t) const {return 0;}
void ReaderScreen::prefetchNextWord(uint32_t) { FakeTask::now += previewPrefetchDelay; }
uint32_t ReaderScreen::frameSignature(std::string_view word,bool,bool,const settings::ReadingSettings& config) const {
    return ui::Context::signature(word,ui::Context::combine(ui::Context::combine(config.typography.fontSizeIndex,config.typography.tracking),config.typography.anchor));
}
void ReaderScreen::draw(ui::Context& ui,const StorageManager&,const Board::Power::BatteryState&,uint32_t) {
    if(pageBrowsing_){drawPage(ui);return;}
    typography_=settings_.typography;refreshTypeface();
    prepareChunk();if(chunkLayoutReady_)drawChunk(ui,{0,36,640,100});
}
}
