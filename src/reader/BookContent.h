#pragma once

#include <Arduino.h>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

// Compact, byte-oriented store for book words.
//
// Words are concatenated into one flat byte buffer (`text_`) with a single
// null terminator between each pair. A parallel `offsets_` table records the
// start byte of each word; the last entry is a sentinel equal to text_.size().
// This avoids the two memory pathologies of `std::vector<String>`:
//  - the vector's backing array no longer needs ~12 bytes per word in one
//    contiguous block,
//  - and there is no per-word malloc, so the heap can't fragment from
//    thousands of tiny String buffers.
//
// Memory usage drops roughly 2-3x for typical Latin text (~10 bytes/word vs
// ~30 bytes/word).
//
// The store is byte-oriented and Unicode-agnostic: callers continue to put
// UTF-8 (or any other) bytes in, and `word(i)` reconstructs an Arduino String
// of the same bytes. ASCII whitespace bytes never appear inside multi-byte
// UTF-8 sequences, so byte-level word splitting in the parser stays correct.
class WordTable {
 public:
  WordTable() = default;
  WordTable(WordTable &&) = default;
  WordTable &operator=(WordTable &&) = default;
  WordTable(const WordTable &) = delete;
  WordTable &operator=(const WordTable &) = delete;

  void clear() {
    std::string().swap(text_);
    std::vector<uint32_t>().swap(offsets_);
  }

  // Pre-reserve to avoid mid-parse re-allocations, which need single
  // contiguous blocks and fail well before total free memory is exhausted.
  void reserveWords(size_t n) { offsets_.reserve(n + 1); }
  void reserveBytes(size_t n) { text_.reserve(n); }

  size_t size() const { return offsets_.empty() ? 0 : offsets_.size() - 1; }
  bool empty() const { return size() == 0; }
  size_t byteCount() const { return text_.size(); }

  // Append a word. May throw std::bad_alloc on memory exhaustion.
  void append(const char *data, size_t len) {
    if (offsets_.empty()) {
      offsets_.push_back(0);
    }
    if (len > 0) {
      text_.append(data, len);
    }
    text_.push_back('\0');
    offsets_.push_back(static_cast<uint32_t>(text_.size()));
  }
  void append(const String &word) { append(word.c_str(), word.length()); }

  // wordLen(i) excludes the implicit null terminator.
  size_t wordLen(size_t i) const { return offsets_[i + 1] - offsets_[i] - 1; }
  // Null-terminated pointer to word i. Stable as long as no further appends
  // happen (we only append during parse, then read).
  const char *wordData(size_t i) const { return text_.data() + offsets_[i]; }
  String word(size_t i) const { return empty() ? String() : String(wordData(i)); }

 private:
  std::string text_;
  std::vector<uint32_t> offsets_;  // size = 0 or words+1; last is sentinel
};

struct ChapterMarker {
  String title;
  size_t wordIndex = 0;
};

struct BookContent {
  String title;
  String author;
  WordTable words;
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
