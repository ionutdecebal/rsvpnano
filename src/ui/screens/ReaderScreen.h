#pragma once

#include <Preferences.h>
#include <functional>
#include <optional>
#include <span>
#include <utility>
#include "board/BoardPower.h"
#include "fonts/AlphaFont.h"
#include "fonts/FontCatalog.h"
#include "reader/ReadingLoop.h"
#include "settings/SettingsModel.h"
#include "settings/SettingsStore.h"
#include "library/IndexedBookStore.h"
#include "library/ReadingProgress.h"
#include "text/BidiText.h"
#include "ui/Ui.h"
#include "ui/screens/PageReaderScreen.h"

namespace screens {

    class ReaderScreen {
    public:
        ReaderScreen(Arduino_GFX& gfx, settings::ReadingSettings& settings);

        ReadingSession session;
        IndexedBookStore store;
        FontCatalog fonts;
        void begin(const ui::themes::Theme& theme);
        void applyTheme(const ui::themes::Theme& theme);
        void releaseRuntimeCaches();
        void refreshTypography();
        void refreshTypography(const settings::ReadingSettings& settings, const settings::ReadingOverrides& overrides);
        bool openBook(ui::Context& ui, StorageManager& storage, Preferences& preferences, size_t index, uint32_t nowMs);
        void prepareBookOpen(Preferences& preferences, uint32_t nowMs);
        void finishBookOpen(Preferences& preferences, uint32_t nowMs);
        void loadInitialBook(ui::Context& ui, StorageManager& storage, Preferences& preferences, uint32_t nowMs);
        void draw(ui::Context& ui, const StorageManager& storage, const Board::Power::BatteryState& battery,
                  uint32_t nowMs);
        bool batteryTapped(const ui::Touch& touch) const;
        bool batteryLongPressed(const ui::Touch& touch) const;
        bool batteryTouched(const ui::Touch& touch) const;
        bool previousSentenceTapped(uint16_t x, uint16_t y) const;
        void handleTouch(ui::Context& ui, uint32_t nowMs, Preferences& preferences,
                         settings::SettingsStore& settingsStore);
        void toggle(Preferences& preferences, uint32_t nowMs);
        void update(Preferences& preferences, uint32_t nowMs);
        // True once after playback paused on a chapter card, so the app can treat the
        // card like user activity and restart its standby timer.
        bool takeChapterCueRaised();

    private:
        void dismissChapterCue(uint32_t nowMs);
        void drawChapterCue(ui::Context& ui);
        int focusOffset(std::string_view word) const;
        int16_t wordAdvance(std::span<const BidiText::Codepoint> word) const;
        void drawPhantom(std::string_view value, bool rightToLeft, int16_t edge, bool extendsLeft, int16_t baseline,
                         bool vertical, ui::Context& ui);
        void drawWord(std::string_view word, int16_t x, int16_t baseline, int focus, bool vertical, ui::Context& ui);
        void drawWord(std::span<const BidiText::Codepoint> word, int16_t x, int16_t baseline, size_t wordOffset,
                      int focus, bool vertical, ui::Context& ui);
        std::string phantomBefore(const ReadingSession& reader, uint8_t sizeIndex) const;
        std::string phantomAfter(const ReadingSession& reader, uint8_t sizeIndex) const;
        uint32_t frameSignature(std::string_view word, bool overlayVisible, bool cjkPacing,
                                const settings::ReadingSettings& settings) const;

        enum class TouchIntent : uint8_t {
            None,
            PlayHold,
            Scrub,
            Wpm,
            Paragraph
        };
        void browseParagraphs(uint16_t y, uint32_t nowMs);
        bool doubleTap(uint16_t x, uint16_t y, uint32_t nowMs);
        void resetTouch();
        int scrubSteps(int deltaX) const;
        void start(uint32_t nowMs, bool locked);
        void requestPause(Preferences& preferences, uint32_t nowMs);
        bool shouldFinishPause(uint32_t nowMs) const;
        void finishPause(Preferences& preferences, uint32_t nowMs);
        size_t fontChoice(size_t wordIndex) const;
        size_t fontChoice(size_t wordIndex, const settings::ReadingSettings& settings,
                          const settings::ReadingOverrides& overrides) const;
        void activateFace(const FontCatalog::Face& face);
        void refreshTypeface();
        void prefetchUpcomingFont(uint32_t nowMs);
        void prefetchNextWord(uint32_t nowMs);
        FontCatalog::Face pageTypeface(size_t wordIndex);

        Arduino_GFX& gfx_;
        // Playback crossed a chapter marker: the reader pauses on a full-screen
        // chapter card until the reader taps to continue.
        bool chapterCue_ = false;
        bool chapterCuePainted_ = false;
        bool chapterCueRaised_ = false;
        size_t chapterCueWordIndex_ = SIZE_MAX;
        mutable ui::fonts::AlphaTextRenderer<640> text_;
        settings::ReadingSettings& settings_;
        FontCatalog::Face face_;
        size_t loadedWordIndex_ = SIZE_MAX;
        size_t loadedFamilyIndex_ = SIZE_MAX;
        size_t renderedWordIndex_ = SIZE_MAX;
        size_t prefetchedWordIndex_ = SIZE_MAX;
        size_t readAheadWordIndex_ = SIZE_MAX;
        size_t readAheadFamilyIndex_ = SIZE_MAX;
        size_t readAheadBlockCount_ = 0;
        uint8_t loadedFontSizeIndex_ = 0xFF;
        uint32_t fontRevision_ = 0;
        uint32_t typographyRevision_ = 0;
        settings::TypographySettings typography_;
        uint16_t background_ = 0;
        bool touching_ = false;
        uint16_t touchStartX_ = 0;
        uint16_t touchStartY_ = 0;
        size_t touchStartWord_ = 0;
        int scrubSteps_ = 0;
        TouchIntent touchIntent_ = TouchIntent::None;
        uint32_t lastTapMs_ = 0;
        uint16_t lastTapX_ = 0;
        uint16_t lastTapY_ = 0;
        bool lastTapValid_ = false;
        uint32_t wpmFeedbackUntilMs_ = 0;
        bool playLocked_ = false;
        bool pauseAtSentenceEndRequested_ = false;
        PageReader::State pageState_;
        ReadingLoop::TextParagraph rsvpParagraph_;
        BidiText::Analysis rsvpBidi_;
        BidiText::Line rsvpLine_;
        std::vector<BidiText::Codepoint> rsvpVisual_;
        std::vector<ui::fonts::PositionedGlyph> rsvpGlyphs_;
        BidiText::Analysis phantomBidi_;
        BidiText::Line phantomLine_;
        std::vector<BidiText::Codepoint> phantomVisual_;
        std::vector<ui::fonts::PositionedGlyph> phantomGlyphs_;
        bool pagePreview_ = false;
        uint32_t paragraphTickMs_ = 0;
        int32_t paragraphRemainder_ = 0;
    };

} // namespace screens
