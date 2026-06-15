#pragma once

#include <Arduino.h>
#include <vector>

struct ChapterMarker {
  String title;
  size_t wordIndex = 0;
};

struct BookMetadata {
  String title;
  String author;
  size_t wordCount = 0;
  std::vector<ChapterMarker> chapters;
  std::vector<size_t> paragraphStarts;

  void clear() {
    title = "";
    author = "";
    wordCount = 0;
    chapters.clear();
    paragraphStarts.clear();
  }
};
