#include "reader/ReadingLoop.h"

#include <algorithm>
#include <cctype>
#include <utility>

#include "text/LatinText.h"

namespace {

constexpr const char *kDemoWords[] = {
    "This",        "is",         "the",         "minimal",     "RSVP",
    "demo",        "reader",     "running",     "on",          "the",
    "Waveshare",   "AMOLED",     "board.",

    "Rapid",       "Serial",     "Visual",      "Presentation,", "or",
    "RSVP,",       "is",         "a",           "reading",     "technique",
    "that",        "displays",   "text",        "one",         "word",
    "at",          "a",          "time",        "in",          "a",
    "fixed",       "position",   "on",          "the",         "screen.",
    "Instead",     "of",         "moving",      "your",        "eyes",
    "across",      "lines",      "and",         "paragraphs,", "you",
    "keep",        "your",       "gaze",        "locked",      "on",
    "a",           "single",     "point",       "while",       "words",
    "flash",       "in",         "sequence.",   "This",        "eliminates",
    "saccades,",   "the",        "small",       "rapid",       "eye",
    "movements",   "that",       "consume",     "a",           "surprising",
    "amount",      "of",         "time",        "during",      "traditional",
    "reading.",

    "The",         "concept",    "emerged",     "from",        "cognitive",
    "psychology",  "research",   "in",          "the",         "1970s,",
    "when",        "scientists", "began",       "studying",    "how",
    "quickly",     "the",        "human",       "brain",       "could",
    "process",     "written",    "language.",   "They",        "discovered",
    "that",        "much",       "of",          "the",         "time",
    "spent",       "reading",    "is",          "not",         "actually",
    "spent",       "understanding", "words",    "but",         "rather",
    "physically",  "relocating", "the",         "eyes",        "from",
    "one",         "word",       "to",          "the",         "next.",
    "By",          "removing",   "that",        "mechanical",  "overhead,",
    "readers",     "could",      "absorb",      "text",        "significantly",
    "faster",      "without",    "losing",      "comprehension.",

    "A",           "key",        "element",     "of",          "modern",
    "RSVP",        "readers",    "is",          "the",         "Optimal",
    "Recognition", "Point,",     "or",          "ORP.",        "Every",
    "word",        "has",        "a",           "specific",    "letter",
    "that",        "your",       "brain",       "naturally",   "fixates",
    "on",          "first.",     "For",         "short",       "words",
    "it",          "tends",      "to",          "be",          "near",
    "the",         "beginning,", "for",         "longer",      "words",
    "it",          "shifts",     "slightly",    "toward",      "the",
    "center.",     "By",         "aligning",    "this",        "letter",
    "at",          "a",          "fixed",       "position",    "on",
    "screen,",     "and",        "highlighting", "it,",        "the",
    "reader",      "can",        "recognize",   "each",        "word",
    "faster",      "because",    "the",         "eye",         "does",
    "not",         "need",       "to",          "search",      "for",
    "where",       "to",         "focus.",

    "The",         "speed",      "is",          "measured",    "in",
    "words",       "per",        "minute,",     "or",          "WPM.",
    "Average",     "silent",     "reading",     "speed",       "is",
    "around",      "200",        "to",          "250",         "WPM.",
    "With",        "RSVP,",      "many",        "people",      "comfortably",
    "reach",       "300",        "to",          "500",         "WPM",
    "after",       "a",          "short",       "adjustment",  "period.",
    "Some",        "experienced", "users",      "push",        "beyond",
    "600",         "WPM,",       "though",      "comprehension", "can",
    "start",       "to",         "decline",     "at",          "very",
    "high",        "speeds",     "depending",   "on",          "the",
    "complexity",  "of",         "the",         "material.",

    "Timing",      "is",         "also",        "adaptive.",   "Longer",
    "words",       "stay",       "on",          "screen",      "slightly",
    "longer",      "because",    "they",        "take",        "more",
    "time",        "to",         "process.",    "Words",       "followed",
    "by",          "punctuation", "like",       "commas,",     "periods,",
    "or",          "question",   "marks",       "receive",     "an",
    "extra",       "pause",      "to",          "let",         "the",
    "brain",       "register",   "the",         "end",         "of",
    "a",           "phrase",     "or",          "sentence.",   "This",
    "mimics",      "the",        "natural",     "rhythm",      "of",
    "reading,",    "and",        "prevents",    "the",         "experience",
    "from",        "feeling",    "robotic.",

    "RSVP",        "is",         "particularly", "effective",  "on",
    "mobile",      "devices",    "where",       "screen",      "space",
    "is",          "limited.",   "A",           "single",      "word",
    "at",          "a",          "time",        "needs",       "almost",
    "no",          "horizontal", "space,",      "making",      "it",
    "ideal",       "for",        "phones.",     "There",       "is",
    "no",          "scrolling,", "no",          "page",        "turning,",
    "and",         "no",         "distraction", "from",        "surrounding",
    "text.",       "You",        "simply",      "hold,",       "read,",
    "and",         "let",        "the",         "words",       "come",
    "to",          "you.",
};

constexpr size_t kDemoWordCount = sizeof(kDemoWords) / sizeof(kDemoWords[0]);
constexpr uint16_t kMinWpm = 100;
constexpr uint16_t kMaxWpm = 1000;
constexpr uint16_t kWpmStep = 25;
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
constexpr uint16_t kMaxPacingDelayMs = 600;

bool isWordCharacter(char c) {
  return LatinText::isWordCharacter(static_cast<uint8_t>(c));
}

bool isLetterCharacter(char c) {
  return LatinText::isLetter(static_cast<uint8_t>(c));
}

bool isDigitCharacter(char c) {
  return LatinText::isDigit(static_cast<uint8_t>(c));
}

bool isLowercaseLetter(char c) {
  return LatinText::isLowercaseLetter(static_cast<uint8_t>(c));
}

bool isUppercaseLetter(char c) {
  return LatinText::isUppercaseLetter(static_cast<uint8_t>(c));
}

bool isVowelCharacter(char c) {
  return LatinText::isVowel(static_cast<uint8_t>(c));
}

bool isSegmentSeparator(char c) {
  switch (c) {
    case '-':
    case '/':
    case '_':
      return true;
    default:
      return false;
  }
}

bool isTechnicalConnector(char c) {
  switch (c) {
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

int letterCharacterCount(const String &word) {
  int count = 0;
  for (size_t i = 0; i < word.length(); ++i) {
    if (isLetterCharacter(word[i])) {
      ++count;
    }
  }
  return count;
}

int digitCharacterCount(const String &word) {
  int count = 0;
  for (size_t i = 0; i < word.length(); ++i) {
    if (isDigitCharacter(word[i])) {
      ++count;
    }
  }
  return count;
}

int uppercaseLetterCount(const String &word) {
  int count = 0;
  for (size_t i = 0; i < word.length(); ++i) {
    if (isUppercaseLetter(word[i])) {
      ++count;
    }
  }
  return count;
}

int readableCharacterCount(const String &word) {
  int count = 0;
  for (size_t i = 0; i < word.length(); ++i) {
    if (isWordCharacter(word[i])) {
      ++count;
    }
  }
  return count;
}

int approximateSyllableGroupCount(const String &word) {
  int groups = 0;
  int letterCount = 0;
  bool previousWasVowel = false;
  String lettersOnly;
  lettersOnly.reserve(word.length());

  for (size_t i = 0; i < word.length(); ++i) {
    const char c = word[i];
    if (!isLetterCharacter(c)) {
      previousWasVowel = false;
      continue;
    }

    ++letterCount;
    const char lowered = static_cast<char>(LatinText::toLowercaseByte(static_cast<uint8_t>(c)));
    lettersOnly += lowered;

    const bool vowel = LatinText::isVowel(static_cast<uint8_t>(lowered));
    if (vowel && !previousWasVowel) {
      ++groups;
    }
    previousWasVowel = vowel;
  }

  if (groups > 1 && letterCount > 3 && lettersOnly.endsWith("e") && !lettersOnly.endsWith("le") &&
      !lettersOnly.endsWith("ye")) {
    --groups;
  }

  if (groups == 0 && letterCount > 0) {
    groups = 1;
  }

  return groups;
}

int compoundJoinerCount(const String &word) {
  int count = 0;
  for (size_t i = 1; i + 1 < word.length(); ++i) {
    if (!isSegmentSeparator(word[i])) {
      continue;
    }
    if (!isWordCharacter(word[i - 1]) || !isWordCharacter(word[i + 1])) {
      continue;
    }
    ++count;
  }
  return count;
}

int technicalConnectorCount(const String &word) {
  int count = 0;
  for (size_t i = 1; i + 1 < word.length(); ++i) {
    if (!isTechnicalConnector(word[i])) {
      continue;
    }
    if (!isWordCharacter(word[i - 1]) || !isWordCharacter(word[i + 1])) {
      continue;
    }
    ++count;
  }
  return count;
}

int lastMeaningfulCharIndex(const String &word) {
  for (int i = static_cast<int>(word.length()) - 1; i >= 0; --i) {
    if (!isIgnoredTrailingChar(word[static_cast<size_t>(i)])) {
      return i;
    }
  }
  return -1;
}

char trailingRhythmChar(const String &word) {
  const int index = lastMeaningfulCharIndex(word);
  if (index >= 0) {
    return word[static_cast<size_t>(index)];
  }
  return '\0';
}

int trailingRepeatedCharCount(const String &word, char target) {
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

bool endsWithEllipsis(const String &word) {
  return trailingRepeatedCharCount(word, '.') >= 3;
}

bool startsWithLowercaseLetter(const String &word) {
  for (size_t i = 0; i < word.length(); ++i) {
    if (isLowercaseLetter(word[i])) {
      return true;
    }
    if (isLetterCharacter(word[i])) {
      return false;
    }
  }
  return false;
}

bool isDottedInitialism(const String &word) {
  const int end = lastMeaningfulCharIndex(word);
  if (end <= 0) {
    return false;
  }

  int letterCount = 0;
  bool expectLetter = true;
  for (int i = 0; i <= end; ++i) {
    const char c = word[static_cast<size_t>(i)];
    if (expectLetter) {
      if (!isLetterCharacter(c)) {
        return false;
      }
      ++letterCount;
      expectLetter = false;
    } else if (c == '.') {
      expectLetter = true;
    } else {
      return false;
    }
  }

  return expectLetter && letterCount >= 2;
}

bool looksLikeAbbreviation(const String &word, bool nextWordStartsLowercase) {
  String lowered = word;
  lowered.toLowerCase();

  constexpr const char *kKnownAbbreviations[] = {
      "mr.",  "mrs.",  "ms.",   "dr.",   "prof.", "sr.",   "jr.",  "st.",
      "vs.",  "etc.",  "e.g.",  "i.e.",  "cf.",   "no.",   "fig.", "eq.",
      "inc.", "ltd.",  "co.",   "dept.", "mt.",   "ft.",
  };

  for (const char *abbreviation : kKnownAbbreviations) {
    if (lowered == abbreviation) {
      return true;
    }
  }

  if (!lowered.endsWith(".")) {
    return false;
  }

  if (isDottedInitialism(word)) {
    return true;
  }

  if (readableCharacterCount(lowered) <= 2) {
    return true;
  }

  if (nextWordStartsLowercase && readableCharacterCount(lowered) <= 4) {
    return true;
  }

  return false;
}

uint16_t clampPacingDelayMs(uint16_t delayMs) {
  if (delayMs > kMaxPacingDelayMs) {
    return kMaxPacingDelayMs;
  }
  return delayMs;
}

uint8_t clampScalePercent(uint8_t percent) {
  if (percent < 25) {
    return 25;
  }
  return percent;
}

uint16_t scaledPercent(uint16_t basePercent, uint8_t scalePercent) {
  return static_cast<uint16_t>((static_cast<uint32_t>(basePercent) *
                                static_cast<uint32_t>(clampScalePercent(scalePercent))) /
                               100UL);
}

uint32_t scaledDelayMs(uint16_t bonusPercent, uint16_t delayMs) {
  return (static_cast<uint32_t>(bonusPercent) *
          static_cast<uint32_t>(clampPacingDelayMs(delayMs))) /
         100UL;
}

uint16_t lengthBonusPercentForWord(const String &word) {
  const int readableLength = readableCharacterCount(word);
  if (readableLength == 0) {
    return 0;
  }

  uint16_t bonusPercent = 0;
  if (readableLength > kLongWordAfterChars) {
    const int extraChars = readableLength - kLongWordAfterChars;
    bonusPercent +=
        static_cast<uint16_t>(extraChars * static_cast<int>(kLongWordPercentPerChar));
  }

  if (readableLength > kVeryLongWordAfterChars) {
    const int extraChars = readableLength - kVeryLongWordAfterChars;
    bonusPercent +=
        static_cast<uint16_t>(extraChars * static_cast<int>(kVeryLongWordPercentPerChar));
  }

  if (readableLength > kUltraLongWordAfterChars) {
    const int extraChars = readableLength - kUltraLongWordAfterChars;
    bonusPercent +=
        static_cast<uint16_t>(extraChars * static_cast<int>(kUltraLongWordPercentPerChar));
  }

  const int joinerCount = compoundJoinerCount(word);
  if (joinerCount > 0) {
    bonusPercent +=
        static_cast<uint16_t>(joinerCount * static_cast<int>(kCompoundJoinerPercent));
    if (readableLength >= kVeryLongWordAfterChars) {
      bonusPercent += kLongCompoundWordPercent;
    }
  }

  const int techConnectorCount = technicalConnectorCount(word);
  if (techConnectorCount > joinerCount) {
    bonusPercent += static_cast<uint16_t>((techConnectorCount - joinerCount) *
                                          static_cast<int>(kTechnicalConnectorPercent));
  }

  return std::min<uint16_t>(kLongWordMaxPercent, bonusPercent);
}

uint16_t complexityBonusPercentForWord(const String &word) {
  uint16_t bonusPercent = 0;
  const int syllableGroups = approximateSyllableGroupCount(word);
  if (syllableGroups > kSyllableBonusAfterCount) {
    const int extraGroups = syllableGroups - kSyllableBonusAfterCount;
    bonusPercent += static_cast<uint16_t>(std::min(
        static_cast<int>(kSyllableBonusMaxPercent),
        extraGroups * static_cast<int>(kSyllableBonusPercentPerGroup)));
  }

  const int letterCount = letterCharacterCount(word);
  const int digitCount = digitCharacterCount(word);
  const int uppercaseCount = uppercaseLetterCount(word);
  if (letterCount > 0 && digitCount > 0) {
    bonusPercent += kMixedTokenComplexityPercent;
  } else if (digitCount >= 3) {
    bonusPercent += kNumericTokenComplexityPercent;
  }

  if (uppercaseCount >= 2 && uppercaseCount == letterCount) {
    bonusPercent += kAllCapsComplexityPercent;
  }

  const int techConnectorCount = technicalConnectorCount(word);
  if (techConnectorCount >= 2) {
    bonusPercent += static_cast<uint16_t>((techConnectorCount - 1) *
                                          static_cast<int>(kDenseConnectorComplexityPercent));
  }

  return std::min<uint16_t>(kComplexWordMaxPercent, bonusPercent);
}

uint16_t punctuationPausePercentForWord(const String &word, bool nextWordStartsLowercase) {
  if (endsWithEllipsis(word)) {
    return kEllipsisPausePercent;
  }

  switch (trailingRhythmChar(word)) {
    case ',':
      return kCommaPausePercent;
    case '-':
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

uint32_t pacingBonusMsForWord(const String &word, bool nextWordStartsLowercase,
                              const ReadingLoop::PacingConfig &config) {
  if (word.isEmpty()) {
    return 0;
  }

  uint32_t totalBonusMs = 0;
  totalBonusMs += scaledDelayMs(
      scaledPercent(lengthBonusPercentForWord(word), config.longWordScalePercent),
      config.longWordDelayMs);
  totalBonusMs += scaledDelayMs(
      scaledPercent(complexityBonusPercentForWord(word), config.complexWordScalePercent),
      config.complexWordDelayMs);
  totalBonusMs +=
      scaledDelayMs(scaledPercent(punctuationPausePercentForWord(word, nextWordStartsLowercase),
                                  config.punctuationScalePercent),
                    config.punctuationDelayMs);
  return totalBonusMs;
}

uint32_t durationForWord(const String &word, bool nextWordStartsLowercase, uint32_t baseIntervalMs,
                         const ReadingLoop::PacingConfig &config) {
  if (baseIntervalMs == 0) {
    return 0;
  }
  return baseIntervalMs + pacingBonusMsForWord(word, nextWordStartsLowercase, config);
}

}  // namespace

void ReadingLoop::begin(uint32_t nowMs) {
  currentIndex_ = 0;
  lastAdvanceMs_ = nowMs;
  setCurrentWordFromIndex();
}

void ReadingLoop::setWords(WordTable words, uint32_t nowMs) {
  loadedWords_ = std::move(words);
  currentIndex_ = 0;
  lastAdvanceMs_ = nowMs;
  setCurrentWordFromIndex();
}

void ReadingLoop::start(uint32_t nowMs) { lastAdvanceMs_ = nowMs; }

bool ReadingLoop::update(uint32_t nowMs, bool allowCatchUp) {
  bool changed = false;
  const uint8_t maxCatchUpWords = allowCatchUp ? kMaxCatchUpWords : 1;

  for (uint8_t catchUp = 0; catchUp < maxCatchUpWords; ++catchUp) {
    const uint32_t durationMs = currentWordDurationMs();
    if (durationMs == 0 || nowMs - lastAdvanceMs_ < durationMs) {
      break;
    }

    lastAdvanceMs_ += durationMs;
    if (!advance(1)) {
      break;
    }
    changed = true;
  }

  return changed;
}

const String &ReadingLoop::currentWord() const { return currentWord_; }

size_t ReadingLoop::currentIndex() const { return currentIndex_; }

uint16_t ReadingLoop::wpm() const { return wpm_; }

uint32_t ReadingLoop::wordIntervalMs() const { return 60000UL / wpm_; }

uint32_t ReadingLoop::currentWordDurationMs() const {
  bool nextWordStartsLowercase = false;
  const size_t nextIndex = currentIndex_ + 1;
  if (!loadedWords_.empty()) {
    if (nextIndex < loadedWords_.size()) {
      nextWordStartsLowercase = startsWithLowercaseLetter(loadedWords_.word(nextIndex));
    }
  } else if (nextIndex < kDemoWordCount) {
    nextWordStartsLowercase = startsWithLowercaseLetter(String(kDemoWords[nextIndex]));
  }

  return durationForWord(currentWord_, nextWordStartsLowercase, wordIntervalMs(), pacingConfig_);
}

uint32_t ReadingLoop::wordPacingBonusMsAt(size_t index) const {
  const size_t count = wordCount();
  if (count == 0 || index >= count) {
    return 0;
  }

  const String word = wordAt(index);
  const bool nextLowercase = nextWordStartsLowercaseAt(index);
  return pacingBonusMsForWord(word, nextLowercase, pacingConfig_);
}

uint32_t ReadingLoop::elapsedInCurrentWordMs(uint32_t nowMs) const {
  if (nowMs <= lastAdvanceMs_) {
    return 0;
  }
  return nowMs - lastAdvanceMs_;
}

bool ReadingLoop::currentWordEndsSentence() const {
  return wordEndsSentenceAt(currentIndex_);
}

bool ReadingLoop::atEnd() const {
  const size_t count = wordCount();
  return count == 0 || currentIndex_ + 1 >= count;
}

void ReadingLoop::scrub(int steps) {
  seekRelative(currentIndex_, steps);
}

void ReadingLoop::seekTo(size_t wordIndex) {
  const size_t count = wordCount();
  if (count == 0) {
    currentWord_ = "";
    return;
  }

  if (wordIndex >= count) {
    wordIndex = count - 1;
  }

  currentIndex_ = wordIndex;
  setCurrentWordFromIndex();
}

void ReadingLoop::seekRelative(size_t baseIndex, int steps) {
  const size_t count = wordCount();
  if (count == 0) {
    return;
  }

  if (baseIndex >= count) {
    baseIndex = count - 1;
  }

  int nextIndex = static_cast<int>(baseIndex) + steps;
  if (usingLoadedBook()) {
    if (nextIndex < 0) {
      nextIndex = 0;
    }
    if (nextIndex >= static_cast<int>(count)) {
      nextIndex = static_cast<int>(count) - 1;
    }
  } else {
    nextIndex %= static_cast<int>(count);
    if (nextIndex < 0) {
      nextIndex += static_cast<int>(count);
    }
  }

  currentIndex_ = static_cast<size_t>(nextIndex);
  setCurrentWordFromIndex();
}

void ReadingLoop::rewindSentence() {
  const size_t count = wordCount();
  if (count == 0) {
    return;
  }

  const size_t currentSentenceStart = sentenceStartAtOrBefore(currentIndex_);
  if (currentSentenceStart == currentIndex_ && currentIndex_ > 0) {
    seekTo(sentenceStartAtOrBefore(currentIndex_ - 1));
    return;
  }

  seekTo(currentSentenceStart);
}

void ReadingLoop::adjustWpm(int delta) {
  if (delta == 0) {
    return;
  }

  int nextWpm = static_cast<int>(wpm_);
  nextWpm += (delta > 0) ? kWpmStep : -static_cast<int>(kWpmStep);
  if (nextWpm < static_cast<int>(kMinWpm)) {
    nextWpm = kMinWpm;
  }
  if (nextWpm > static_cast<int>(kMaxWpm)) {
    nextWpm = kMaxWpm;
  }
  wpm_ = static_cast<uint16_t>(nextWpm);
}

void ReadingLoop::setWpm(uint16_t wpm) {
  if (wpm < kMinWpm) {
    wpm = kMinWpm;
  }
  if (wpm > kMaxWpm) {
    wpm = kMaxWpm;
  }
  wpm_ = wpm;
}

void ReadingLoop::setPacingConfig(const PacingConfig &config) {
  pacingConfig_.longWordDelayMs = clampPacingDelayMs(config.longWordDelayMs);
  pacingConfig_.complexWordDelayMs = clampPacingDelayMs(config.complexWordDelayMs);
  pacingConfig_.punctuationDelayMs = clampPacingDelayMs(config.punctuationDelayMs);
  pacingConfig_.longWordScalePercent = clampScalePercent(config.longWordScalePercent);
  pacingConfig_.complexWordScalePercent = clampScalePercent(config.complexWordScalePercent);
  pacingConfig_.punctuationScalePercent = clampScalePercent(config.punctuationScalePercent);
}

const ReadingLoop::PacingConfig &ReadingLoop::pacingConfig() const { return pacingConfig_; }

bool ReadingLoop::advance(size_t steps) {
  const size_t count = wordCount();
  if (count == 0) {
    currentWord_ = "";
    return false;
  }

  const size_t previousIndex = currentIndex_;
  if (usingLoadedBook()) {
    const size_t maxIndex = count - 1;
    if (currentIndex_ < maxIndex) {
      const size_t remaining = maxIndex - currentIndex_;
      currentIndex_ += (steps < remaining) ? steps : remaining;
    }
  } else {
    currentIndex_ = (currentIndex_ + steps) % count;
  }

  if (currentIndex_ == previousIndex) {
    return false;
  }

  setCurrentWordFromIndex();
  return true;
}

void ReadingLoop::setCurrentWordFromIndex() {
  if (wordCount() == 0) {
    currentWord_ = "";
    return;
  }

  currentWord_ = wordAt(currentIndex_);
}

size_t ReadingLoop::wordCount() const {
  if (!loadedWords_.empty()) {
    return loadedWords_.size();
  }
  return kDemoWordCount;
}

String ReadingLoop::wordAt(size_t index) const {
  if (!loadedWords_.empty()) {
    return loadedWords_.word(index);
  }
  return String(kDemoWords[index]);
}

bool ReadingLoop::usingLoadedBook() const { return !loadedWords_.empty(); }

bool ReadingLoop::nextWordStartsLowercaseAt(size_t wordIndex) const {
  const size_t nextIndex = wordIndex + 1;
  if (nextIndex >= wordCount()) {
    return false;
  }

  return startsWithLowercaseLetter(wordAt(nextIndex));
}

bool ReadingLoop::wordEndsSentenceAt(size_t wordIndex) const {
  if (wordIndex >= wordCount()) {
    return false;
  }

  const String word = wordAt(wordIndex);
  if (word.isEmpty()) {
    return false;
  }

  switch (trailingRhythmChar(word)) {
    case '!':
    case '?':
      return true;
    case '.':
      return !looksLikeAbbreviation(word, nextWordStartsLowercaseAt(wordIndex));
    default:
      return false;
  }
}

size_t ReadingLoop::sentenceStartAtOrBefore(size_t wordIndex) const {
  const size_t count = wordCount();
  if (count == 0) {
    return 0;
  }

  if (wordIndex >= count) {
    wordIndex = count - 1;
  }

  while (wordIndex > 0) {
    if (wordEndsSentenceAt(wordIndex - 1)) {
      break;
    }
    --wordIndex;
  }

  return wordIndex;
}
