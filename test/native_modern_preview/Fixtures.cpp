#include "StorageFixture.h"
// Host-only hardware/catalog fixtures. Screen layouts and controls come from
// production sources. Reader words here are static raster specimens: this does
// not test the live reader's scheduling, shaping or phantom-word pipeline.
#include "ui/screens/ReaderScreen.h"
#include "ui/screens/ReaderLayout.h"
#include "fonts/LiterataFallbackAlpha4.h"
#include "text/Utf8Text.h"
#include "network/WifiConnection.h"

namespace net {
std::expected<void,std::error_code> connectStation(const char*,const char*,const WifiProgress&,uint32_t) {
    return std::unexpected(std::make_error_code(std::errc::timed_out));
}
void disconnect() {}
}

FontCatalog::FontCatalog() { families_.push_back({.id="literata", .label="Literata", .builtIn=true, .scriptMask=UnicodeText::ScriptLatin}); }
FontCatalog::Face FontCatalog::loadFace(size_t, size_t size) {
    return {*ui::fonts::LiterataFallbackAlpha4_Sizes[std::min<size_t>(size, 3)]};
}
bool focus::OrientationReader::begin() { available_ = true; return true; }
int previewSaveCalls = 0, previewPauseCalls = 0, previewToggleCalls = 0;
settings::SettingsStore::~SettingsStore() = default;
void settings::SettingsStore::acceptChanges() { ++previewSaveCalls; }
void settings::SettingsStore::acceptSecretChanges() {}

namespace screens {
    ReaderScreen::ReaderScreen(Arduino_GFX& gfx, settings::ReadingSettings& config)
        : width_(gfx.width()), height_(gfx.height()), text_(gfx), settings_(config), face_(fonts.loadFace(0, 0)) {
        text_.begin();
    }
    void ReaderScreen::start(uint32_t now, bool) {ReadingLoop::start(session,now);}
    void ReaderScreen::resetTouch() { touching_ = false; touchIntent_ = TouchIntent::None; }
    void ReaderScreen::finishPause(Preferences&, uint32_t) { ++previewPauseCalls; ReadingLoop::pause(session); }
    void ReaderScreen::toggle(Preferences&, uint32_t) { ++previewToggleCalls; session.playing = !session.playing; }
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
FontCatalog::Face ReaderScreen::pageTypeface(size_t index) { return fonts.loadFace(0,readerLayout::pageStrikeIndex()); }
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
uint8_t percent(uint32_t index,uint32_t count) { return count<=1?0:static_cast<uint8_t>(std::min<uint64_t>(uint64_t(index)*100/(count-1),100)); }
}

namespace screens {
size_t ReaderScreen::fontChoice(size_t) const {return 0;}
void ReaderScreen::activateFace(const FontCatalog::Face& face) {text_.setFont(face.raster.get());}
void ReaderScreen::refreshTypeface() {typography_=settings_.typography;face_=fonts.loadFace(0,typography_.fontSizeIndex);activateFace(face_);}
uint32_t ReaderScreen::frameSignature(std::string_view word,bool,bool,const settings::ReadingSettings& config) const {
    return ui::Context::signature(word,ui::Context::combine(ui::Context::combine(config.typography.fontSizeIndex,config.typography.tracking),config.typography.anchor));
}
void ReaderScreen::draw(ui::Context& ui,const StorageManager&,const Board::Power::BatteryState&,uint32_t) {
    prepareChunk();if(pageBrowsing_)drawPage(ui);else if(chunkLayoutReady_)drawChunk(ui,{0,36,640,100});
}
}
