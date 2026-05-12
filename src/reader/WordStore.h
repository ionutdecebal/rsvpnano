#pragma once

#include <Arduino.h>
#include <cstdint>
#include <vector>

#include "storage/PsramAllocator.h"

class WordStore {
 public:
  void clear() {
    offsets_.clear();
    lengths_.clear();
    text_.clear();
  }

  void reserve(size_t wordCount, size_t textBytes = 0) {
    offsets_.reserve(wordCount);
    lengths_.reserve(wordCount);
    if (textBytes > 0) {
      text_.reserve(textBytes);
    }
  }

  bool empty() const { return offsets_.empty(); }
  size_t size() const { return offsets_.size(); }

  void add(String word) {
    if (word.isEmpty()) {
      return;
    }

    const size_t offset = text_.size();
    if (offset > UINT32_MAX) {
      return;
    }

    const size_t length = word.length();
    if (length > UINT16_MAX) {
      return;
    }

    offsets_.push_back(static_cast<uint32_t>(offset));
    lengths_.push_back(static_cast<uint16_t>(length));
    for (size_t i = 0; i < length; ++i) {
      text_.push_back(word[i]);
    }
  }

  String wordAt(size_t index) const {
    if (index >= offsets_.size()) {
      return "";
    }

    const uint32_t offset = offsets_[index];
    const uint16_t length = lengths_[index];
    String word;
    word.reserve(length);
    for (uint16_t i = 0; i < length; ++i) {
      word += text_[offset + i];
    }
    return word;
  }

 private:
  std::vector<uint32_t, PsramAllocator<uint32_t>> offsets_;
  std::vector<uint16_t, PsramAllocator<uint16_t>> lengths_;
  std::vector<char, PsramAllocator<char>> text_;
};
