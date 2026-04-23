#pragma once

#include <Arduino.h>
#include <vector>

class ReadingLoop {
 public:
  struct PacingConfig {
    uint8_t longWordScalePercent = 100;
    uint8_t complexWordScalePercent = 100;
    uint8_t punctuationScalePercent = 100;
  };

  void begin(uint32_t nowMs);
  void start(uint32_t nowMs);
  bool update(uint32_t nowMs);
  void setWords(std::vector<String> words, uint32_t nowMs);
  void scrub(int steps);
  void seekTo(size_t wordIndex);
  void seekRelative(size_t baseIndex, int steps);
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

 private:
  bool advance(size_t steps);
  void setCurrentWordFromIndex();
  bool usingLoadedBook() const;

  size_t currentIndex_ = 0;
  uint32_t lastAdvanceMs_ = 0;
  uint16_t wpm_ = 300;
  PacingConfig pacingConfig_;
  String currentWord_;
  std::vector<String> loadedWords_;
};
