#pragma once

#include <Arduino.h>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <utility>
#include <vector>

struct ChapterMarker {
  String title;
  size_t wordIndex = 0;
};

// Compact word storage: all words are concatenated into a single NUL-terminated
// blob, with a parallel offsets table giving each word's start position.
//
// Memory cost per word ≈ 4 B (offset) + word length + 1 B NUL, vs.
// `std::vector<String>` which spends ~30-40 B per word (Arduino String object,
// heap-block overhead, padding, and dynamic allocation rounding). On the
// PSRAM-less ESP32-C6 this is the difference between fitting a novel and
// crashing on out-of-memory while loading.
class WordBlob {
 public:
  WordBlob() { offsets_.push_back(0); }

  void clear() {
    blob_.clear();
    offsets_.clear();
    offsets_.push_back(0);
  }

  bool empty() const { return offsets_.size() <= 1; }
  size_t size() const { return offsets_.size() - 1; }

  void reserve(size_t wordCount, size_t totalChars) {
    offsets_.reserve(wordCount + 1);
    blob_.reserve(totalChars);
  }

  void push_back(const String &word) { push_back(word.c_str(), word.length()); }

  void push_back(const char *word, size_t len) {
    blob_.insert(blob_.end(), word, word + len);
    blob_.push_back('\0');
    offsets_.push_back(static_cast<uint32_t>(blob_.size()));
  }

  const char *cstr(size_t index) const { return blob_.data() + offsets_[index]; }

  size_t length(size_t index) const {
    // -1 to exclude the terminating NUL we appended in push_back.
    return offsets_[index + 1] - offsets_[index] - 1;
  }

  String at(size_t index) const { return String(cstr(index)); }

  void shrinkToFit() {
    blob_.shrink_to_fit();
    offsets_.shrink_to_fit();
  }

  // Approximate footprint in bytes; useful for diagnostics.
  size_t byteSize() const {
    return blob_.capacity() + offsets_.capacity() * sizeof(uint32_t);
  }

 private:
  std::vector<char> blob_;
  std::vector<uint32_t> offsets_;
};

struct BookContent {
  String title;
  String author;
  WordBlob words;
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
