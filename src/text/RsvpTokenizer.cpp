#include "text/RsvpTokenizer.h"

#include <algorithm>

#include "text/LatinText.h"

namespace RsvpText {

namespace Detail {

bool isWordBoundary(char c) {
  const uint8_t value = LatinText::byteValue(c);
  return value <= ' ' && !LatinText::isWordCharacter(value) &&
         !LatinText::isLowCustomSlotByte(value);
}

bool isInlineWordHyphen(const String &text, size_t index) {
  if (index == 0 || index + 1 >= text.length() || text[index] != '-') {
    return false;
  }
  if (text[index - 1] == '-' || text[index + 1] == '-') {
    return false;
  }
  return isReadableTokenChar(text[index - 1]) &&
         isReadableTokenChar(text[index + 1]);
}

bool isHyphenToken(const String &token) {
  if (token.isEmpty()) {
    return false;
  }
  const char *text = token.c_str();
  return std::all_of(text, text + token.length(),
                     [](char c) { return c == '-'; });
}

bool isRhythmToken(const String &token) { return isHyphenToken(token); }

bool isEllipsisToken(const String &token) {
  if (token.length() < 3) {
    return false;
  }
  const char *text = token.c_str();
  return std::all_of(text, text + token.length(),
                     [](char c) { return c == '.'; });
}

} // namespace Detail

bool isReadableTokenChar(char c) {
  return LatinText::isWordCharacter(LatinText::byteValue(c));
}

bool isRhythmToken(const String &token) { return Detail::isHyphenToken(token); }

} // namespace RsvpText
