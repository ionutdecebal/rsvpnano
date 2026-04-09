#pragma once

#include <Arduino.h>
#include <vector>

enum class AppState {
  LOADING,
  PAUSED,
  PLAYING,
  SCRUBBING,
  MENU,
  SLEEPING
};

struct WordToken {
  String text;
  bool sentenceEnd = false;
  bool paragraphBreak = false;
};

struct ChapterAnchor {
  String title;
  size_t wordIndex = 0;
};

struct BookData {
  String id;
  String title;
  std::vector<WordToken> words;
  std::vector<ChapterAnchor> chapters;
};

struct BookProgress {
  size_t wordIndex = 0;
  int wpm = 280;
};

struct TouchEvent {
  bool valid = false;
  bool pressed = false;
  int16_t x = 0;
  int16_t y = 0;
  uint32_t tsMs = 0;
};

struct GestureResult {
  bool horizontalScrub = false;
  int scrubDelta = 0;
  bool verticalWpm = false;
  int wpmDelta = 0;
  bool longPress = false;
};
