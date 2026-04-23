#pragma once

#include <Arduino.h>
#include <vector>

struct ChapterMarker {
  String title;
  size_t wordIndex = 0;
};

struct BookContent {
  String title;
  String author;
  std::vector<String> words;
  std::vector<ChapterMarker> chapters;
  std::vector<size_t> paragraphStarts;

  void clear() {
    title = "";
    author = "";
    words.clear();
    chapters.clear();
    paragraphStarts.clear();
  }
};
