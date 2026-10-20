#include "ui/screens/ReaderScreen.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace screens {
    void ReaderScreen::toggle(Preferences& preferences, uint32_t nowMs) {
        if (pageBrowsing_) return;
        if (session.playing)
            requestPause(preferences, nowMs);
        else
            start(nowMs, true);
    }

    void ReaderScreen::update(Preferences& preferences, uint32_t nowMs) {
        // Browsing owns the page font. Preparing a chunk here would replace its
        // cached strike with the RSVP size on every paused browser frame.
        if (pageBrowsing_) return;
        if (wpmFeedbackUntilMs_ > 0 && nowMs >= wpmFeedbackUntilMs_) {
            wpmFeedbackUntilMs_ = 0;
        }
        prepareChunk();
        if (shouldFinishPause(nowMs)) {
            finishPause(preferences, nowMs);
            return;
        }
        prefetchNextWord(nowMs);
        nowMs = millis();
        if (!session.playing)
            return;
#if defined(RSVP_BENCHMARK_MODE)
        // Reading benchmarks load indexed fixtures but intentionally exclude persistence I/O.
        ReadingLoop::update(session, settings_, nowMs, true, pauseAtSentenceEndRequested_);
#else
        const size_t previousIndex = session.state.wordIndex;
        if (ReadingLoop::update(session, settings_, nowMs, true, pauseAtSentenceEndRequested_)) {
            ReadingProgress::saveChapterTransition(session, preferences, store, previousIndex, session.state.wordIndex,
                                                   nowMs);
        }
#endif
        if (!session.playing && pauseAtSentenceEndRequested_)
            finishPause(preferences, nowMs);
    }

    void ReaderScreen::start(uint32_t nowMs, bool locked) {
        pagePreview_ = false;
        pageBrowsing_ = false;
        playLocked_ = locked;
        pauseAtSentenceEndRequested_ = false;
        wpmFeedbackUntilMs_ = 0;
        prepareChunk();
        ReadingLoop::start(session, nowMs);
    }

    void ReaderScreen::requestPause(Preferences& preferences, uint32_t nowMs) {
        if (!session.playing)
            return;
        playLocked_ = false;
        if (settings_.pauseMode == settings::PauseMode::instant) {
            finishPause(preferences, nowMs);
            return;
        }
        pauseAtSentenceEndRequested_ = true;
        prepareChunk();
        if (shouldFinishPause(nowMs)) {
            finishPause(preferences, nowMs);
        }
    }

    bool ReaderScreen::shouldFinishPause(uint32_t nowMs) const {
        if (!session.playing || !pauseAtSentenceEndRequested_)
            return false;
        const uint32_t durationMs = ReadingLoop::currentWordDurationMs(session, settings_);
        return durationMs > 0 && ReadingLoop::elapsedInCurrentWordMs(session, nowMs) >= durationMs
            && (ReadingLoop::currentWordEndsSentence(session) || ReadingLoop::atEnd(session));
    }

    void ReaderScreen::finishPause(Preferences& preferences, uint32_t nowMs) {
        ReadingLoop::pause(session);
        pauseAtSentenceEndRequested_ = false;
        playLocked_ = false;
        ReadingProgress::save(session, preferences, true, nowMs);
    }

} // namespace screens
