#include "reader/ReadingLoop.h"
#include "library/IndexedBookStore.h"

#include <algorithm>
#include <string_view>

#include "text/AsciiText.h"
#include "text/UnicodeText.h"
#include "text/Utf8Text.h"

namespace {

    constexpr const char* kDemoWords[] = {
        "This",
        "is",
        "the",
        "minimal",
        "RSVP",
        "demo",
        "reader",
        "running",
        "on",
        "the",
        "Waveshare",
        "AMOLED",
        "board.",

        "Rapid",
        "Serial",
        "Visual",
        "Presentation,",
        "or",
        "RSVP,",
        "is",
        "a",
        "reading",
        "technique",
        "that",
        "displays",
        "text",
        "one",
        "word",
        "at",
        "a",
        "time",
        "in",
        "a",
        "fixed",
        "position",
        "on",
        "the",
        "screen.",
        "Instead",
        "of",
        "moving",
        "your",
        "eyes",
        "across",
        "lines",
        "and",
        "paragraphs,",
        "you",
        "keep",
        "your",
        "gaze",
        "locked",
        "on",
        "a",
        "single",
        "point",
        "while",
        "words",
        "flash",
        "in",
        "sequence.",
        "This",
        "eliminates",
        "saccades,",
        "the",
        "small",
        "rapid",
        "eye",
        "movements",
        "that",
        "consume",
        "a",
        "surprising",
        "amount",
        "of",
        "time",
        "during",
        "traditional",
        "reading.",

        "The",
        "concept",
        "emerged",
        "from",
        "cognitive",
        "psychology",
        "research",
        "in",
        "the",
        "1970s,",
        "when",
        "scientists",
        "began",
        "studying",
        "how",
        "quickly",
        "the",
        "human",
        "brain",
        "could",
        "process",
        "written",
        "language.",
        "They",
        "discovered",
        "that",
        "much",
        "of",
        "the",
        "time",
        "spent",
        "reading",
        "is",
        "not",
        "actually",
        "spent",
        "understanding",
        "words",
        "but",
        "rather",
        "physically",
        "relocating",
        "the",
        "eyes",
        "from",
        "one",
        "word",
        "to",
        "the",
        "next.",
        "By",
        "removing",
        "that",
        "mechanical",
        "overhead,",
        "readers",
        "could",
        "absorb",
        "text",
        "significantly",
        "faster",
        "without",
        "losing",
        "comprehension.",

        "A",
        "key",
        "element",
        "of",
        "modern",
        "RSVP",
        "readers",
        "is",
        "the",
        "Optimal",
        "Recognition",
        "Point,",
        "or",
        "ORP.",
        "Every",
        "word",
        "has",
        "a",
        "specific",
        "letter",
        "that",
        "your",
        "brain",
        "naturally",
        "fixates",
        "on",
        "first.",
        "For",
        "short",
        "words",
        "it",
        "tends",
        "to",
        "be",
        "near",
        "the",
        "beginning,",
        "for",
        "longer",
        "words",
        "it",
        "shifts",
        "slightly",
        "toward",
        "the",
        "center.",
        "By",
        "aligning",
        "this",
        "letter",
        "at",
        "a",
        "fixed",
        "position",
        "on",
        "screen,",
        "and",
        "highlighting",
        "it,",
        "the",
        "reader",
        "can",
        "recognize",
        "each",
        "word",
        "faster",
        "because",
        "the",
        "eye",
        "does",
        "not",
        "need",
        "to",
        "search",
        "for",
        "where",
        "to",
        "focus.",

        "The",
        "speed",
        "is",
        "measured",
        "in",
        "words",
        "per",
        "minute,",
        "or",
        "WPM.",
        "Average",
        "silent",
        "reading",
        "speed",
        "is",
        "around",
        "200",
        "to",
        "250",
        "WPM.",
        "With",
        "RSVP,",
        "many",
        "people",
        "comfortably",
        "reach",
        "300",
        "to",
        "500",
        "WPM",
        "after",
        "a",
        "short",
        "adjustment",
        "period.",
        "Some",
        "experienced",
        "users",
        "push",
        "beyond",
        "600",
        "WPM,",
        "though",
        "comprehension",
        "can",
        "start",
        "to",
        "decline",
        "at",
        "very",
        "high",
        "speeds",
        "depending",
        "on",
        "the",
        "complexity",
        "of",
        "the",
        "material.",

        "Timing",
        "is",
        "also",
        "adaptive.",
        "Longer",
        "words",
        "stay",
        "on",
        "screen",
        "slightly",
        "longer",
        "because",
        "they",
        "take",
        "more",
        "time",
        "to",
        "process.",
        "Words",
        "followed",
        "by",
        "punctuation",
        "like",
        "commas,",
        "periods,",
        "or",
        "question",
        "marks",
        "receive",
        "an",
        "extra",
        "pause",
        "to",
        "let",
        "the",
        "brain",
        "register",
        "the",
        "end",
        "of",
        "a",
        "phrase",
        "or",
        "sentence.",
        "This",
        "mimics",
        "the",
        "natural",
        "rhythm",
        "of",
        "reading,",
        "and",
        "prevents",
        "the",
        "experience",
        "from",
        "feeling",
        "robotic.",

        "RSVP",
        "is",
        "particularly",
        "effective",
        "on",
        "mobile",
        "devices",
        "where",
        "screen",
        "space",
        "is",
        "limited.",
        "A",
        "single",
        "word",
        "at",
        "a",
        "time",
        "needs",
        "almost",
        "no",
        "horizontal",
        "space,",
        "making",
        "it",
        "ideal",
        "for",
        "phones.",
        "There",
        "is",
        "no",
        "scrolling,",
        "no",
        "page",
        "turning,",
        "and",
        "no",
        "distraction",
        "from",
        "surrounding",
        "text.",
        "You",
        "simply",
        "hold,",
        "read,",
        "and",
        "let",
        "the",
        "words",
        "come",
        "to",
        "you.",
    };

    constexpr size_t kDemoWordCount = sizeof(kDemoWords) / sizeof(kDemoWords[0]);
    constexpr uint16_t kLowWpmMax = 100;
    constexpr uint16_t kLowWpmStep = 10;
    constexpr uint16_t kHighWpmStep = 25;
    constexpr uint8_t kLongWordAfterChars = 6;
    constexpr uint8_t kLongWordPercentPerChar = 6;
    constexpr uint8_t kVeryLongWordAfterChars = 10;
    constexpr uint8_t kVeryLongWordPercentPerChar = 9;
    constexpr uint8_t kUltraLongWordAfterChars = 14;
    constexpr uint8_t kUltraLongWordPercentPerChar = 12;
    constexpr uint8_t kLongWordMaxPercent = 170;
    constexpr uint8_t kCompoundJoinerPercent = 14;
    constexpr uint8_t kLongCompoundWordPercent = 18;
    constexpr uint8_t kTechnicalConnectorPercent = 8;
    constexpr uint8_t kSyllableBonusAfterCount = 2;
    constexpr uint8_t kSyllableBonusPercentPerGroup = 10;
    constexpr uint8_t kSyllableBonusMaxPercent = 50;
    constexpr uint8_t kAllCapsComplexityPercent = 14;
    constexpr uint8_t kMixedTokenComplexityPercent = 22;
    constexpr uint8_t kNumericTokenComplexityPercent = 10;
    constexpr uint8_t kDenseConnectorComplexityPercent = 12;
    constexpr uint8_t kComplexWordMaxPercent = 85;
    constexpr uint8_t kCommaPausePercent = 45;
    constexpr uint8_t kDashPausePercent = 60;
    constexpr uint8_t kClausePausePercent = 80;
    constexpr uint8_t kEllipsisPausePercent = 110;
    constexpr uint8_t kSentencePausePercent = 135;
    constexpr uint8_t kStrongSentencePausePercent = 150;
    constexpr uint8_t kMaxCatchUpWords = 4;

    template<typename Predicate>
    int codepointCount(std::string_view text, Predicate predicate) {
        int count = 0;
        uint32_t codepoint = 0;
        while (Utf8Text::next(text, codepoint)) {
            if (predicate(codepoint))
                ++count;
        }
        return count;
    }

    bool isSegmentSeparator(uint32_t codepoint) {
        switch (codepoint) {
        case '-':
        case '/':
        case '_':
            return true;
        default:
            return false;
        }
    }

    bool isTechnicalConnector(uint32_t codepoint) {
        switch (codepoint) {
        case '-':
        case '/':
        case '_':
        case '.':
        case '+':
        case '\\':
            return true;
        default:
            return false;
        }
    }

    bool isIgnoredTrailingChar(char c) {
        switch (c) {
        case '"':
        case '\'':
        case ')':
        case ']':
        case '}':
            return true;
        default:
            return false;
        }
    }

    bool isIgnoredTrailingCodepoint(uint32_t codepoint) {
        if (codepoint <= 0x7FU)
            return isIgnoredTrailingChar(static_cast<char>(codepoint));
        switch (codepoint) {
        case 0x2019U:
        case 0x201DU:
        case 0x3009U:
        case 0x300BU:
        case 0x300DU:
        case 0x300FU:
        case 0x3011U:
        case 0x3015U:
        case 0x3017U:
        case 0x3019U:
        case 0x301BU:
        case 0xFF09U:
        case 0xFF3DU:
        case 0xFF5DU:
            return true;
        default:
            return false;
        }
    }

    int approximateSyllableGroupCount(std::string_view word) {
        int groups = 0;
        int letterCount = 0;
        bool previousWasVowel = false;
        uint32_t previousLetter = 0;
        uint32_t lastLetter = 0;

        uint32_t codepoint = 0;
        while (Utf8Text::next(word, codepoint)) {
            if (!UnicodeText::isLetter(codepoint)) {
                previousWasVowel = false;
                continue;
            }

            ++letterCount;
            const uint32_t lowered = UnicodeText::toLowercase(codepoint);
            previousLetter = lastLetter;
            lastLetter = lowered;

            const bool vowel = UnicodeText::isVowel(lowered);
            if (vowel && !previousWasVowel) {
                ++groups;
            }
            previousWasVowel = vowel;
        }

        if (groups > 1 && letterCount > 3 && lastLetter == 'e' && previousLetter != 'l' && previousLetter != 'y') {
            --groups;
        }

        if (groups == 0 && letterCount > 0) {
            groups = 1;
        }

        return groups;
    }

    template<typename Predicate>
    int connectorCount(std::string_view word, Predicate predicate) {
        int count = 0;
        uint32_t previous = 0;
        uint32_t current = 0;
        if (!Utf8Text::next(word, previous) || !Utf8Text::next(word, current))
            return 0;

        uint32_t next = 0;
        while (Utf8Text::next(word, next)) {
            if (predicate(current) && UnicodeText::isWordCharacter(previous) && UnicodeText::isWordCharacter(next)) {
                ++count;
            }
            previous = current;
            current = next;
        }
        return count;
    }

    int lastMeaningfulCharIndex(std::string_view word) {
        for (int i = static_cast<int>(word.length()) - 1; i >= 0; --i) {
            if (!isIgnoredTrailingChar(word[static_cast<size_t>(i)])) {
                return i;
            }
        }
        return -1;
    }

    uint32_t trailingRhythmCodepoint(std::string_view word) {
        while (!word.empty()) {
            const size_t start = Utf8Text::lastCodepointStart(word);
            std::string_view trailing = word.substr(start);
            uint32_t codepoint = 0;
            Utf8Text::next(trailing, codepoint);
            word.remove_suffix(word.size() - start);
            if (isIgnoredTrailingCodepoint(codepoint))
                continue;
            switch (codepoint) {
            case 0x3001U:
            case 0xFF0CU:
                return ',';
            case 0x3002U:
                return '.';
            case 0xFF01U:
                return '!';
            case 0xFF1AU:
                return ':';
            case 0xFF1BU:
                return ';';
            case 0xFF1FU:
                return '?';
            default:
                return codepoint;
            }
        }
        return 0;
    }

    int trailingRepeatedCharCount(std::string_view word, char target) {
        int count = 0;
        for (int i = lastMeaningfulCharIndex(word); i >= 0; --i) {
            const char c = word[static_cast<size_t>(i)];
            if (c != target) {
                break;
            }
            ++count;
        }
        return count;
    }

    bool endsWithEllipsis(std::string_view word) {
        return trailingRhythmCodepoint(word) == 0x2026U || trailingRepeatedCharCount(word, '.') >= 3;
    }

    bool startsWithLowercaseLetter(std::string_view word) {
        uint32_t codepoint = 0;
        while (Utf8Text::next(word, codepoint)) {
            if (UnicodeText::isLowercaseLetter(codepoint)) {
                return true;
            }
            if (UnicodeText::isLetter(codepoint)) {
                return false;
            }
        }
        return false;
    }

    bool isDottedInitialism(std::string_view word) {
        const int end = lastMeaningfulCharIndex(word);
        if (end <= 0) {
            return false;
        }

        int letterCount = 0;
        bool expectLetter = true;
        std::string_view text = word.substr(0, static_cast<size_t>(end + 1));
        uint32_t codepoint = 0;
        while (Utf8Text::next(text, codepoint)) {
            if (expectLetter) {
                if (!UnicodeText::isLetter(codepoint)) {
                    return false;
                }
                ++letterCount;
                expectLetter = false;
            } else if (codepoint == '.') {
                expectLetter = true;
            } else {
                return false;
            }
        }

        return expectLetter && letterCount >= 2;
    }

    bool looksLikeAbbreviation(std::string_view word, bool nextWordStartsLowercase) {
        constexpr const char* kKnownAbbreviations[] = {
            "mr.",  "mrs.", "ms.", "dr.",  "prof.", "sr.",  "jr.",  "st.", "vs.",   "etc.", "e.g.",
            "i.e.", "cf.",  "no.", "fig.", "eq.",   "inc.", "ltd.", "co.", "dept.", "mt.",  "ft.",
        };

        if (std::ranges::any_of(kKnownAbbreviations, [word](const char* abbreviation) {
                return std::ranges::equal(word, std::string_view{abbreviation}, {}, AsciiText::toLower,
                                          AsciiText::toLower);
            }))
            return true;

        if (!word.ends_with(".")) {
            return false;
        }

        if (isDottedInitialism(word)) {
            return true;
        }

        if (codepointCount(word, UnicodeText::isWordCharacter) <= 2) {
            return true;
        }

        if (nextWordStartsLowercase && codepointCount(word, UnicodeText::isWordCharacter) <= 4) {
            return true;
        }

        return false;
    }

    uint32_t scaledDelayMs(uint16_t bonusPercent, uint16_t delayMs) {
        return (static_cast<uint32_t>(bonusPercent) * delayMs) / 100UL;
    }

    uint16_t lengthBonusPercentForWord(std::string_view word) {
        const int readableLength = codepointCount(word, UnicodeText::isWordCharacter);
        if (readableLength == 0) {
            return 0;
        }

        uint16_t bonusPercent = 0;
        if (readableLength > kLongWordAfterChars) {
            const int extraChars = readableLength - kLongWordAfterChars;
            bonusPercent += static_cast<uint16_t>(extraChars * static_cast<int>(kLongWordPercentPerChar));
        }

        if (readableLength > kVeryLongWordAfterChars) {
            const int extraChars = readableLength - kVeryLongWordAfterChars;
            bonusPercent += static_cast<uint16_t>(extraChars * static_cast<int>(kVeryLongWordPercentPerChar));
        }

        if (readableLength > kUltraLongWordAfterChars) {
            const int extraChars = readableLength - kUltraLongWordAfterChars;
            bonusPercent += static_cast<uint16_t>(extraChars * static_cast<int>(kUltraLongWordPercentPerChar));
        }

        const int joinerCount = connectorCount(word, isSegmentSeparator);
        if (joinerCount > 0) {
            bonusPercent += static_cast<uint16_t>(joinerCount * static_cast<int>(kCompoundJoinerPercent));
            if (readableLength >= kVeryLongWordAfterChars) {
                bonusPercent += kLongCompoundWordPercent;
            }
        }

        const int techConnectorCount = connectorCount(word, isTechnicalConnector);
        if (techConnectorCount > joinerCount) {
            bonusPercent += static_cast<uint16_t>((techConnectorCount - joinerCount)
                                                  * static_cast<int>(kTechnicalConnectorPercent));
        }

        return std::min<uint16_t>(kLongWordMaxPercent, bonusPercent);
    }

    uint16_t complexityBonusPercentForWord(std::string_view word) {
        uint16_t bonusPercent = 0;
        const int syllableGroups = approximateSyllableGroupCount(word);
        if (syllableGroups > kSyllableBonusAfterCount) {
            const int extraGroups = syllableGroups - kSyllableBonusAfterCount;
            bonusPercent +=
                static_cast<uint16_t>(std::min(static_cast<int>(kSyllableBonusMaxPercent),
                                               extraGroups * static_cast<int>(kSyllableBonusPercentPerGroup)));
        }

        const int letterCount = codepointCount(word, UnicodeText::isLetter);
        const int digitCount = codepointCount(word, UnicodeText::isDigit);
        const int uppercaseCount = codepointCount(word, UnicodeText::isUppercaseLetter);
        if (letterCount > 0 && digitCount > 0) {
            bonusPercent += kMixedTokenComplexityPercent;
        } else if (digitCount >= 3) {
            bonusPercent += kNumericTokenComplexityPercent;
        }

        if (uppercaseCount >= 2 && uppercaseCount == letterCount) {
            bonusPercent += kAllCapsComplexityPercent;
        }

        const int techConnectorCount = connectorCount(word, isTechnicalConnector);
        if (techConnectorCount >= 2) {
            bonusPercent +=
                static_cast<uint16_t>((techConnectorCount - 1) * static_cast<int>(kDenseConnectorComplexityPercent));
        }

        return std::min<uint16_t>(kComplexWordMaxPercent, bonusPercent);
    }

    uint16_t punctuationPausePercentForWord(std::string_view word, bool nextWordStartsLowercase) {
        if (endsWithEllipsis(word)) {
            return kEllipsisPausePercent;
        }

        switch (trailingRhythmCodepoint(word)) {
        case ',':
            return kCommaPausePercent;
        case '-':
        case 0x2014U:
            return kDashPausePercent;
        case ';':
        case ':':
            return kClausePausePercent;
        case '.':
            if (!looksLikeAbbreviation(word, nextWordStartsLowercase)) {
                return kSentencePausePercent;
            }
            return 0;
        case '!':
        case '?':
            return kStrongSentencePausePercent;
        default:
            return 0;
        }
    }

    uint32_t pacingBonusMsForWord(std::string_view word, bool nextWordStartsLowercase,
                                  const settings::PacingSettings& pacing) {
        if (word.empty()) {
            return 0;
        }

        uint32_t totalBonusMs = 0;
        totalBonusMs += scaledDelayMs(lengthBonusPercentForWord(word), pacing.longWordDelayMs);
        totalBonusMs += scaledDelayMs(complexityBonusPercentForWord(word), pacing.complexWordDelayMs);
        totalBonusMs +=
            scaledDelayMs(punctuationPausePercentForWord(word, nextWordStartsLowercase), pacing.punctuationDelayMs);
        return totalBonusMs;
    }

    uint32_t durationForWord(std::string_view word, bool nextWordStartsLowercase, uint32_t baseIntervalMs,
                             const settings::PacingSettings& pacing) {
        if (baseIntervalMs == 0) {
            return 0;
        }
        return baseIntervalMs + pacingBonusMsForWord(word, nextWordStartsLowercase, pacing);
    }

} // namespace

namespace ReadingLoop {
    namespace {

        bool usingLoadedBook(const ReadingSession& session) {
            return session.bookStore != nullptr || !session.words.empty();
        }

        bool nextWordStartsLowercaseAt(const ReadingSession& session, size_t wordIndex) {
            const size_t nextIndex = wordIndex + 1;
            return nextIndex < wordCount(session) && startsWithLowercaseLetter(wordAt(session, nextIndex));
        }

        bool wordEndsSentenceAt(const ReadingSession& session, size_t wordIndex) {
            if (wordIndex >= wordCount(session))
                return false;

            const std::string_view word = wordAt(session, wordIndex);
            if (word.empty())
                return false;

            switch (trailingRhythmCodepoint(word)) {
            case '!':
            case '?':
                return true;
            case '.':
                return !looksLikeAbbreviation(word, nextWordStartsLowercaseAt(session, wordIndex));
            default:
                return false;
            }
        }

        size_t sentenceStartAtOrBefore(const ReadingSession& session, size_t wordIndex) {
            const size_t count = wordCount(session);
            if (count == 0)
                return 0;

            wordIndex = std::min(wordIndex, count - 1);
            while (wordIndex > 0 && !wordEndsSentenceAt(session, wordIndex - 1))
                --wordIndex;
            return wordIndex;
        }

        void setCurrentWordFromIndex(ReadingSession& session) {
            if (wordCount(session) == 0) {
                session.currentWord.clear();
                return;
            }

            if (session.bookStore != nullptr)
                session.bookStore->prefetchAround(session.state.wordIndex);
            session.currentWord = wordAt(session, session.state.wordIndex);
        }

        bool advance(ReadingSession& session, size_t steps) {
            const size_t count = wordCount(session);
            if (count == 0) {
                session.currentWord.clear();
                return false;
            }

            const size_t previousIndex = session.state.wordIndex;
            if (usingLoadedBook(session)) {
                const size_t maxIndex = count - 1;
                if (session.state.wordIndex < maxIndex)
                    session.state.wordIndex +=
                        static_cast<uint32_t>(std::min<size_t>(steps, maxIndex - session.state.wordIndex));
            } else {
                session.state.wordIndex = (session.state.wordIndex + steps) % count;
            }

            if (session.state.wordIndex == previousIndex)
                return false;
            setCurrentWordFromIndex(session);
            return true;
        }

    } // namespace

    void begin(ReadingSession& session, uint32_t nowMs) {
        session.playing = false;
        session.state.wordIndex = 0;
        session.lastAdvanceMs = nowMs;
        setCurrentWordFromIndex(session);
    }

    void setWords(ReadingSession& session, std::span<const std::string> words, uint32_t nowMs) {
        session.state.wordIndex = 0;
        session.lastAdvanceMs = nowMs;
        session.words = words;
        session.bookStore = nullptr;
        session.playing = false;
        setCurrentWordFromIndex(session);
    }

    void setBookStore(ReadingSession& session, const IndexedBookStore& store, uint32_t nowMs) {
        session.state.wordIndex = 0;
        session.lastAdvanceMs = nowMs;
        session.words = {};
        session.bookStore = &store;
        session.playing = false;
        setCurrentWordFromIndex(session);
    }

    void start(ReadingSession& session, uint32_t nowMs) {
        session.lastAdvanceMs = nowMs;
        session.playing = true;
    }

    void pause(ReadingSession& session) {
        session.playing = false;
    }

    bool update(ReadingSession& session, const settings::ReadingSettings& settings, uint32_t nowMs, bool allowCatchUp) {
        bool changed = false;
        const uint8_t maxCatchUpWords = allowCatchUp ? kMaxCatchUpWords : 1;
        for (uint8_t catchUp = 0; catchUp < maxCatchUpWords; ++catchUp) {
            const uint32_t durationMs = currentWordDurationMs(session, settings);
            if (durationMs == 0 || nowMs - session.lastAdvanceMs < durationMs)
                break;
            session.lastAdvanceMs += durationMs;
            if (!advance(session, 1))
                break;
            changed = true;
        }
        return changed;
    }

    settings::ReadingPacing pacingMode(const ReadingSession& session) {
        if (session.state.overrides.pacing)
            return *session.state.overrides.pacing;
        return UnicodeText::isCjkText(session.currentWord) ? settings::ReadingPacing::cjkPhrase
                                                           : settings::ReadingPacing::words;
    }

    uint32_t currentWordDurationMs(const ReadingSession& session, const settings::ReadingSettings& settings) {
        const size_t nextIndex = session.state.wordIndex + 1;
        const bool nextWordStartsLowercase =
            nextIndex < wordCount(session) && startsWithLowercaseLetter(wordAt(session, nextIndex));

        if (pacingMode(session) == settings::ReadingPacing::cjkPhrase) {
            const uint32_t characters =
                static_cast<uint32_t>(std::max(1, codepointCount(session.currentWord, UnicodeText::isWordCharacter)));
            const uint32_t base = (60'000UL * characters + settings.wpm - 1) / settings.wpm;
            return base
                 + scaledDelayMs(punctuationPausePercentForWord(session.currentWord, false),
                                 settings.pacing.punctuationDelayMs);
        }
        return durationForWord(session.currentWord, nextWordStartsLowercase, 60000UL / settings.wpm, settings.pacing);
    }

    uint32_t elapsedInCurrentWordMs(const ReadingSession& session, uint32_t nowMs) {
        return nowMs <= session.lastAdvanceMs ? 0 : nowMs - session.lastAdvanceMs;
    }

    bool currentWordEndsSentence(const ReadingSession& session) {
        return wordEndsSentenceAt(session, session.state.wordIndex);
    }

    bool atEnd(const ReadingSession& session) {
        const size_t count = wordCount(session);
        return count == 0 || session.state.wordIndex + 1 >= count;
    }

    void seekTo(ReadingSession& session, size_t wordIndex) {
        const size_t count = wordCount(session);
        if (count == 0) {
            session.currentWord.clear();
            return;
        }
        session.state.wordIndex = static_cast<uint32_t>(std::min(wordIndex, count - 1));
        setCurrentWordFromIndex(session);
    }

    void seekRelative(ReadingSession& session, size_t baseIndex, int steps) {
        const size_t count = wordCount(session);
        if (count == 0)
            return;

        baseIndex = std::min(baseIndex, count - 1);
        int nextIndex = static_cast<int>(baseIndex) + steps;
        if (usingLoadedBook(session)) {
            nextIndex = std::clamp(nextIndex, 0, static_cast<int>(count) - 1);
        } else {
            nextIndex %= static_cast<int>(count);
            if (nextIndex < 0)
                nextIndex += static_cast<int>(count);
        }
        session.state.wordIndex = static_cast<size_t>(nextIndex);
        setCurrentWordFromIndex(session);
    }

    bool seekParagraph(ReadingSession& session, int steps) {
        const auto& paragraphStarts = session.metadata.paragraphStarts;
        if (paragraphStarts.empty() || steps == 0)
            return false;
        const auto next = std::ranges::upper_bound(paragraphStarts, session.state.wordIndex);
        const size_t current =
            next == paragraphStarts.begin() ? 0 : static_cast<size_t>(next - paragraphStarts.begin() - 1);
        const int64_t target = std::clamp<int64_t>(static_cast<int64_t>(current) + steps, 0,
                                                   static_cast<int64_t>(paragraphStarts.size() - 1));
        if (target == static_cast<int64_t>(current))
            return false;
        const size_t wordIndex = paragraphStarts[static_cast<size_t>(target)];
        seekTo(session, wordIndex);
        return true;
    }

    void rewindSentence(ReadingSession& session) {
        if (wordCount(session) == 0)
            return;

        const size_t currentSentenceStart = sentenceStartAtOrBefore(session, session.state.wordIndex);
        if (currentSentenceStart == session.state.wordIndex && session.state.wordIndex > 0) {
            seekTo(session, sentenceStartAtOrBefore(session, session.state.wordIndex - 1));
            return;
        }
        seekTo(session, currentSentenceStart);
    }

    void adjustWpm(settings::ReadingSettings& settings, int delta) {
        if (delta == 0)
            return;

        int nextWpm = settings.wpm;
        if (delta > 0) {
            nextWpm += nextWpm < kLowWpmMax ? kLowWpmStep : kHighWpmStep;
            if (nextWpm > kLowWpmMax && settings.wpm < kLowWpmMax)
                nextWpm = kLowWpmMax;
        } else {
            nextWpm -= nextWpm <= kLowWpmMax ? kLowWpmStep : kHighWpmStep;
            if (nextWpm < kLowWpmMax && settings.wpm > kLowWpmMax)
                nextWpm = kLowWpmMax;
        }
        settings.wpm = nextWpm;
    }

    size_t wordCount(const ReadingSession& session) {
        if (session.bookStore != nullptr)
            return session.bookStore->wordCount();
        if (!session.words.empty())
            return session.words.size();
        return kDemoWordCount;
    }

    std::string_view wordAt(const ReadingSession& session, size_t index) {
        if (session.bookStore != nullptr)
            return session.bookStore->wordAt(index);
        if (!session.words.empty())
            return session.words[index];
        return kDemoWords[index];
    }

    std::pair<size_t, size_t> paragraphBoundsAt(const ReadingSession& session, size_t wordIndex) {
        const size_t count = wordCount(session);
        if (count == 0)
            return {};

        wordIndex = std::min(wordIndex, count - 1);
        const auto next = std::ranges::upper_bound(session.metadata.paragraphStarts, wordIndex);
        const size_t firstWord = next == session.metadata.paragraphStarts.begin() ? 0 : *std::prev(next);
        const size_t lastWord = next == session.metadata.paragraphStarts.end() ? count : std::min(*next, count);
        return {firstWord, lastWord};
    }

    TextParagraph paragraphAt(const ReadingSession& session, size_t wordIndex) {
        TextParagraph result;
        const auto [firstWord, lastWord] = paragraphBoundsAt(session, wordIndex);
        result.firstWord = firstWord;
        result.lastWord = lastWord;
        result.text.reserve((result.lastWord - result.firstWord) * 8);
        result.wordOffsets.reserve(result.lastWord - result.firstWord + 1);
        bool previousCjk = false;
        for (size_t index = result.firstWord; index < result.lastWord; ++index) {
            const std::string_view word = wordAt(session, index);
            const bool cjk = UnicodeText::isCjkText(word);
            if (index > result.firstWord && !(previousCjk && cjk))
                result.text += ' ';
            result.wordOffsets.push_back(result.text.size());
            result.text += word;
            previousCjk = cjk;
        }
        result.wordOffsets.push_back(result.text.size());
        return result;
    }

} // namespace ReadingLoop
