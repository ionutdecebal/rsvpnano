#pragma once

#include <Arduino.h>
#include <stdint.h>

namespace CyrillicText {

constexpr uint8_t kLeadByte = 0x18;
constexpr uint8_t kTrailByteFirst = 0x40;
constexpr uint8_t kLetterCount = 66;
constexpr uint8_t kTrailByteLast = kTrailByteFirst + kLetterCount - 1;

constexpr uint32_t kCodepoints[kLetterCount] = {
    0x0410, 0x0411, 0x0412, 0x0413, 0x0414, 0x0415, 0x0401, 0x0416, 0x0417, 0x0418,
    0x0419, 0x041A, 0x041B, 0x041C, 0x041D, 0x041E, 0x041F, 0x0420, 0x0421, 0x0422,
    0x0423, 0x0424, 0x0425, 0x0426, 0x0427, 0x0428, 0x0429, 0x042A, 0x042B, 0x042C,
    0x042D, 0x042E, 0x042F, 0x0430, 0x0431, 0x0432, 0x0433, 0x0434, 0x0435, 0x0451,
    0x0436, 0x0437, 0x0438, 0x0439, 0x043A, 0x043B, 0x043C, 0x043D, 0x043E, 0x043F,
    0x0440, 0x0441, 0x0442, 0x0443, 0x0444, 0x0445, 0x0446, 0x0447, 0x0448, 0x0449,
    0x044A, 0x044B, 0x044C, 0x044D, 0x044E, 0x044F,
};

inline bool isLeadByte(uint8_t value) { return value == kLeadByte; }

inline bool isTrailByte(uint8_t value) {
  return value >= kTrailByteFirst && value <= kTrailByteLast;
}

inline bool isEncodedPair(uint8_t lead, uint8_t trail) {
  return isLeadByte(lead) && isTrailByte(trail);
}

inline uint8_t trailByteForIndex(uint8_t index) {
  return static_cast<uint8_t>(kTrailByteFirst + index);
}

inline uint8_t indexForTrailByte(uint8_t trail) {
  return static_cast<uint8_t>(trail - kTrailByteFirst);
}

inline bool storageBytesForCodepoint(uint32_t codepoint, uint8_t &lead, uint8_t &trail) {
  for (uint8_t index = 0; index < kLetterCount; ++index) {
    if (kCodepoints[index] == codepoint) {
      lead = kLeadByte;
      trail = trailByteForIndex(index);
      return true;
    }
  }
  return false;
}

inline bool isWordCharacterAt(const String &text, size_t index) {
  if (index >= text.length()) {
    return false;
  }

  const uint8_t value = static_cast<uint8_t>(text[index]);
  if (isLeadByte(value)) {
    return index + 1 < text.length() && isTrailByte(static_cast<uint8_t>(text[index + 1]));
  }
  if (index > 0 && isLeadByte(static_cast<uint8_t>(text[index - 1])) && isTrailByte(value)) {
    return true;
  }
  return false;
}

inline bool isWordCharacterByte(uint8_t value) {
  return isLeadByte(value) || isTrailByte(value);
}

inline bool isUppercaseIndex(uint8_t index) { return index < 33; }

inline bool isLowercaseIndex(uint8_t index) { return index >= 33; }

inline bool isUppercaseAt(const String &text, size_t index) {
  if (!isLeadByte(static_cast<uint8_t>(text[index])) || index + 1 >= text.length()) {
    return false;
  }
  return isUppercaseIndex(indexForTrailByte(static_cast<uint8_t>(text[index + 1])));
}

inline bool isLowercaseAt(const String &text, size_t index) {
  if (!isLeadByte(static_cast<uint8_t>(text[index])) || index + 1 >= text.length()) {
    return false;
  }
  return isLowercaseIndex(indexForTrailByte(static_cast<uint8_t>(text[index + 1])));
}

inline bool isLetterAt(const String &text, size_t index) {
  return isWordCharacterAt(text, index);
}

inline bool isVowelIndex(uint8_t index) {
  switch (index) {
    case 0:   // А
    case 5:   // Е
    case 6:   // Ё
    case 9:   // И
    case 14:  // О
    case 20:  // У
    case 28:  // Ы
    case 30:  // Э
    case 31:  // Ю
    case 32:  // Я
    case 33:  // а
    case 38:  // е
    case 39:  // ё
    case 42:  // и
    case 47:  // о
    case 53:  // у
    case 61:  // ы
    case 63:  // э
    case 64:  // ю
    case 65:  // я
      return true;
    default:
      return false;
  }
}

inline bool isVowelAt(const String &text, size_t index) {
  if (isLeadByte(static_cast<uint8_t>(text[index])) && index + 1 < text.length()) {
    return isVowelIndex(indexForTrailByte(static_cast<uint8_t>(text[index + 1])));
  }
  if (index > 0 && isLeadByte(static_cast<uint8_t>(text[index - 1])) &&
      isTrailByte(static_cast<uint8_t>(text[index]))) {
    return isVowelIndex(indexForTrailByte(static_cast<uint8_t>(text[index])));
  }
  return false;
}

inline uint8_t lowercaseIndex(uint8_t index) {
  if (isUppercaseIndex(index)) {
    return static_cast<uint8_t>(index + 33);
  }
  return index;
}

inline void encodeIndex(uint8_t index, uint8_t &lead, uint8_t &trail) {
  lead = kLeadByte;
  trail = trailByteForIndex(index);
}

inline bool toLowercasePair(uint8_t lead, uint8_t trail, uint8_t &outLead, uint8_t &outTrail) {
  if (!isEncodedPair(lead, trail)) {
    return false;
  }
  const uint8_t index = indexForTrailByte(trail);
  encodeIndex(lowercaseIndex(index), outLead, outTrail);
  return true;
}

inline uint8_t fallbackAsciiForIndex(uint8_t index) {
  static const char kFallback[] = "ABVGDEZhZIJKLMNOPRSTUFKHTSchShchYEHYuYa"
                                  "abvgdezhzijklmnoprstufkhtschshchyeyuya";
  if (index < sizeof(kFallback) - 1) {
    return static_cast<uint8_t>(kFallback[index]);
  }
  return static_cast<uint8_t>('?');
}

inline uint8_t fallbackAsciiByte(uint8_t lead, uint8_t trail) {
  if (!isEncodedPair(lead, trail)) {
    return static_cast<uint8_t>('?');
  }
  return fallbackAsciiForIndex(indexForTrailByte(trail));
}

inline size_t encodedLengthAt(const String &text, size_t index) {
  if (index >= text.length()) {
    return 0;
  }
  if (isLeadByte(static_cast<uint8_t>(text[index])) && index + 1 < text.length() &&
      isTrailByte(static_cast<uint8_t>(text[index + 1]))) {
    return 2;
  }
  return 1;
}

inline size_t advanceIndex(const String &text, size_t index) {
  return index + encodedLengthAt(text, index);
}

}  // namespace CyrillicText
