#pragma once

#include <Arduino.h>
#include <vector>

struct ChapterMarker {
  String title;
  size_t wordIndex = 0;
};

struct BookPartInfo {
  String filename;
  String title;
  size_t wordCount = 0;
};

struct BookManifest {
  String title;
  String author;
  String version;
  std::vector<BookPartInfo> parts;

  size_t totalWords() const {
    size_t total = 0;
    for (const auto &part : parts) {
      total += part.wordCount;
    }
    return total;
  }

  size_t partIndexForGlobalWord(size_t globalWordIndex) const {
    size_t cumulative = 0;
    for (size_t i = 0; i < parts.size(); ++i) {
      cumulative += parts[i].wordCount;
      if (globalWordIndex < cumulative) {
        return i;
      }
    }
    return parts.empty() ? 0 : parts.size() - 1;
  }

  size_t globalWordIndexForPart(size_t partIndex) const {
    size_t cumulative = 0;
    for (size_t i = 0; i < partIndex && i < parts.size(); ++i) {
      cumulative += parts[i].wordCount;
    }
    return cumulative;
  }

  void clear() {
    title = "";
    author = "";
    version = "";
    parts.clear();
  }
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
