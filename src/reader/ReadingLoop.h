#pragma once

#include <Arduino.h>
#include <vector>

class ReadingLoop {
 public:
  struct PacingConfig {
    uint16_t longWordDelayMs = 200;
    uint16_t complexWordDelayMs = 200;
    uint16_t punctuationDelayMs = 200;
    uint8_t longWordScalePercent = 100;
    uint8_t complexWordScalePercent = 100;
    uint8_t punctuationScalePercent = 100;
  };

  void begin(uint32_t nowMs);
  void start(uint32_t nowMs);
  bool update(uint32_t nowMs, bool allowCatchUp = true);
  void setWords(std::vector<String> words, uint32_t nowMs);
  void scrub(int steps);
  void seekTo(size_t wordIndex);
  void seekRelative(size_t baseIndex, int steps);
  void rewindSentence();
  void adjustWpm(int delta);
  void setWpm(uint16_t wpm);
  void setPacingConfig(const PacingConfig &config);
  const PacingConfig &pacingConfig() const;

  const String &currentWord() const;
  String wordAt(size_t index) const;
  size_t currentIndex() const;
  size_t wordCount() const;
  uint16_t wpm() const;
  uint32_t wordIntervalMs() const;
  uint32_t currentWordDurationMs() const;
  uint32_t wordPacingBonusMsAt(size_t index) const;
  uint32_t elapsedInCurrentWordMs(uint32_t nowMs) const;
  bool currentWordEndsSentence() const;
  bool atEnd() const;

 private:
  bool advance(size_t steps);
  void setCurrentWordFromIndex();
  bool usingLoadedBook() const;
  bool nextWordStartsLowercaseAt(size_t wordIndex) const;
  bool wordEndsSentenceAt(size_t wordIndex) const;
  size_t sentenceStartAtOrBefore(size_t wordIndex) const;

  size_t currentIndex_ = 0;
  uint32_t lastAdvanceMs_ = 0;
  uint16_t wpm_ = 300;
  PacingConfig pacingConfig_;
  String currentWord_;
  std::vector<String> loadedWords_;
};
