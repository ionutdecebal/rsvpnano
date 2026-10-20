#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "reader/ReadingSession.h"
#include "settings/SettingsModel.h"

namespace ReadingLoop {

    struct TextParagraph {
        size_t firstWord = 0;
        size_t lastWord = 0;
        std::string text;
        std::vector<size_t> wordOffsets;
    };

    void begin(ReadingSession& session, uint32_t nowMs);
    void start(ReadingSession& session, uint32_t nowMs);
    void pause(ReadingSession& session);
    bool update(ReadingSession& session, const settings::ReadingSettings& settings, uint32_t nowMs,
                bool allowCatchUp = true);
    void setWords(ReadingSession& session, std::span<const std::string> words, uint32_t nowMs);
    void setBookStore(ReadingSession& session, const IndexedBookStore& store, uint32_t nowMs);
    void seekTo(ReadingSession& session, size_t wordIndex);
    void seekRelative(ReadingSession& session, size_t baseIndex, int steps);
    bool seekParagraph(ReadingSession& session, int steps);
    void rewindSentence(ReadingSession& session);
    void adjustWpm(settings::ReadingSettings& settings, int delta);

    std::string_view wordAt(const ReadingSession& session, size_t index);
    size_t wordCount(const ReadingSession& session);
    size_t displayWordCount(const ReadingSession& session);
    uint32_t wordDurationMs(const ReadingSession& session, const settings::ReadingSettings& settings, size_t index);
    std::pair<size_t, size_t> paragraphBoundsAt(const ReadingSession& session, size_t wordIndex);
    TextParagraph paragraphAt(const ReadingSession& session, size_t wordIndex);
    settings::ReadingPacing pacingMode(const ReadingSession& session);
    uint32_t currentWordDurationMs(const ReadingSession& session, const settings::ReadingSettings& settings);
    uint32_t elapsedInCurrentWordMs(const ReadingSession& session, uint32_t nowMs);
    bool currentWordEndsSentence(const ReadingSession& session);
    bool atEnd(const ReadingSession& session);

} // namespace ReadingLoop
