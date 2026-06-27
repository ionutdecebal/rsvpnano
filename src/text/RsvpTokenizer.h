#pragma once

#include <Arduino.h>
#include <type_traits>

#ifndef RSVP_MAX_BOOK_WORDS
#define RSVP_MAX_BOOK_WORDS 0
#endif

#include "text/TextNormalizer.h"

namespace RsvpText {

constexpr size_t kMaxBookWords = static_cast<size_t>(RSVP_MAX_BOOK_WORDS);
constexpr size_t kMaxBookLineChars = 4096;

bool isReadableTokenChar(char c);
bool isRhythmToken(const String &token);

namespace Detail {

bool isWordBoundary(char c);
bool isInlineWordHyphen(const String &text, size_t index);
bool isHyphenToken(const String &token);
bool isEllipsisToken(const String &token);

} // namespace Detail

template <typename TokenConsumer, typename WordCount>
bool appendNormalizedLineWords(const String &normalizedLine,
                               TokenConsumer consumeToken,
                               const WordCount &wordCount, size_t maxWords) {
  using TokenResult = typename std::result_of<TokenConsumer(const String &)>::type;
  static_assert(std::is_convertible<TokenResult, bool>::value,
                "TokenConsumer must be callable as bool(const String&)");
  static_assert(std::is_integral<WordCount>::value,
                "WordCount must be an integral counter type");

  String currentWord;
  String pendingToken;
  currentWord.reserve(32);
  pendingToken.reserve(32);

  auto withinWordLimit = [&]() {
    return maxWords == 0 || wordCount < maxWords;
  };

  auto flushPending = [&]() -> bool {
    if (pendingToken.isEmpty()) {
      return true;
    }
    if (!withinWordLimit()) {
      return false;
    }
    if (!consumeToken(pendingToken)) {
      return false;
    }
    pendingToken = "";
    return withinWordLimit();
  };

  auto finishToken = [&](String token) -> bool {
    trimAsciiWhitespace(token);
    if (token.isEmpty()) {
      return true;
    }

    if (Detail::isEllipsisToken(token)) {
      if (!pendingToken.isEmpty()) {
        pendingToken += "...";
      }
      return true;
    }

    if (Detail::isHyphenToken(token)) {
      if (!flushPending()) {
        return false;
      }
      if (!consumeToken("-")) {
        return false;
      }
      return withinWordLimit();
    }

    if (!flushPending()) {
      return false;
    }
    pendingToken = token;
    return true;
  };

  auto flushCurrent = [&]() -> bool {
    if (currentWord.isEmpty()) {
      return true;
    }
    const bool ok = finishToken(currentWord);
    currentWord = "";
    return ok;
  };

  for (size_t i = 0; i < normalizedLine.length(); ++i) {
    if ((i & 0x7F) == 0) {
      yield();
      delay(0);
    }

    const char c = normalizedLine[i];
    if (Detail::isWordBoundary(c)) {
      if (!flushCurrent()) {
        return false;
      }
      continue;
    }

    if (c == '-') {
      if (Detail::isInlineWordHyphen(normalizedLine, i)) {
        currentWord += c;
        continue;
      }
      if (!flushCurrent() || !finishToken("-")) {
        return false;
      }
      while (i + 1 < normalizedLine.length() && normalizedLine[i + 1] == '-') {
        ++i;
      }
      continue;
    }

    if (c == '.' && i + 2 < normalizedLine.length() &&
        normalizedLine[i + 1] == '.' && normalizedLine[i + 2] == '.') {
      currentWord += "...";
      i += 2;
      while (i + 1 < normalizedLine.length() && normalizedLine[i + 1] == '.') {
        ++i;
      }
      if (!flushCurrent()) {
        return false;
      }
      continue;
    }

    currentWord += c;
  }

  if (!flushCurrent()) {
    return false;
  }

  return flushPending();
}

template <typename TokenConsumer, typename WordCount>
bool appendLineWords(const String &line, TokenConsumer consumeToken,
                     const WordCount &wordCount, ParseStats *stats) {
  const String normalizedLine = normalizeDisplayText(line, stats);
  return appendNormalizedLineWords(normalizedLine, consumeToken, wordCount,
                                   kMaxBookWords);
}

} // namespace RsvpText
