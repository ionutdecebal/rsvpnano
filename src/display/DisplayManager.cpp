#include "display/DisplayManager.h"

#include <algorithm>
#include <cctype>
#include <cstring>

#include <esp_heap_caps.h>
#include <esp_log.h>

#include "board/BoardConfig.h"
#include "display/EmbeddedSerifFont.h"
#include "display/EmbeddedSerifFont70.h"
#include "display/Panel.h"

namespace {
constexpr int kDisplayWidth = BoardConfig::DISPLAY_WIDTH;
constexpr int kDisplayHeight = BoardConfig::DISPLAY_HEIGHT;
constexpr int kPanelNativeWidth = BoardConfig::PANEL_NATIVE_WIDTH;
constexpr int kPanelNativeHeight = BoardConfig::PANEL_NATIVE_HEIGHT;

constexpr int kBaseGlyphHeight = kEmbeddedSerifHeight;
constexpr int kMinTextScale = 1;
constexpr int kMaxTextScale = 1;
constexpr uint8_t kGlyphAlphaThreshold = 16;
constexpr uint16_t kTrueBlack = 0x0000;
constexpr uint16_t kPureWhite = 0xFFFF;
constexpr uint16_t kDarkWordColor = 0xFFFF;
constexpr uint16_t kLightWordColor = 0x0000;
constexpr uint16_t kFocusLetterColor = 0xF800;
constexpr uint16_t kNightWordColor = 0xFCE0;
constexpr uint16_t kNightFocusColor = 0xFA80;
constexpr uint16_t kDarkMenuDimColor = 0x8410;
constexpr uint16_t kLightMenuDimColor = 0x6B4D;
constexpr uint16_t kDarkFooterColor = 0x528A;
constexpr uint16_t kLightFooterColor = 0x5ACB;
constexpr uint8_t kNightDimAlpha = 92;
constexpr uint8_t kNightFooterAlpha = 132;
constexpr int kRsvpSideMargin = 12;
constexpr int kRsvpGuideTickHeight = 5;
constexpr int kRsvpGuideTopOffset = 7;
constexpr int kRsvpGuideBottomOffset = 7;
constexpr int kWpmFeedbackBottomMargin = 16;
constexpr int kTinyGlyphWidth = 5;
constexpr int kTinyGlyphHeight = 7;
constexpr int kTinyGlyphSpacing = 1;
constexpr int kTinyScale = 2;
constexpr int kFooterMarginX = 12;
constexpr int kFooterMarginBottom = 8;
constexpr int kCompactMenuRowHeight = 22;
constexpr int kCompactMenuX = 28;
constexpr int kLibraryRowHeight = 38;
constexpr int kLibraryInsetX = 26;
constexpr int kLibraryTitleYOffset = 4;
constexpr int kLibrarySubtitleYOffset = 20;
constexpr int kLibraryScreenPaddingY = 28;
constexpr uint8_t kLibrarySubtitleAlpha = 120;
#if CONFIG_IDF_TARGET_ESP32C6
// 320x172 panel: enlarge the quick-scroll context font (smaller divisor =
// bigger glyphs) and adjust line metrics to match.
constexpr int kContextMarginX = 14;
constexpr int kContextTop = 6;
constexpr int kContextLineHeight = 33;
constexpr int kContextParagraphGap = 8;
constexpr int kContextParagraphIndent = 28;
constexpr int kContextSpaceWidth = 10;
constexpr int kContextSerifDivisor = 2;
constexpr size_t kContextTargetLines = 4;
#else
constexpr int kContextMarginX = 18;
constexpr int kContextTop = 8;
constexpr int kContextLineHeight = 23;
constexpr int kContextParagraphGap = 7;
constexpr int kContextParagraphIndent = 22;
constexpr int kContextSpaceWidth = 8;
constexpr int kContextSerifDivisor = 3;
constexpr size_t kContextTargetLines = 6;
#endif
constexpr int kPhantomCurrentGapLarge = 30;
constexpr int kPhantomCurrentGapMedium = 24;
constexpr int kPhantomCurrentGapSmall = 20;
constexpr uint8_t kPhantomAlphaLarge = 54;
constexpr uint8_t kPhantomAlphaMedium = 62;
constexpr uint8_t kPhantomAlphaSmall = 72;
constexpr int kTypographyTrackingMin = -2;
constexpr int kTypographyTrackingMax = 3;
constexpr int kTypographyAnchorMin = 30;
constexpr int kTypographyAnchorMax = 40;
constexpr int kTypographyGuideHalfWidthMin = 12;
constexpr int kTypographyGuideHalfWidthMax = 30;
constexpr int kTypographyGuideGapMin = 2;
constexpr int kTypographyGuideGapMax = 8;
constexpr int kOpticalLetterGapPx = 2;

constexpr int kVirtualBufferWidth = (kDisplayWidth + kMinTextScale - 1) / kMinTextScale;
constexpr int kVirtualBufferHeight = (kDisplayHeight + kMinTextScale - 1) / kMinTextScale;

constexpr size_t kBytesPerPixel = sizeof(uint16_t);
constexpr size_t kMaxChunkBytes = 16 * 1024;
constexpr int kTxBufferWidth = kDisplayWidth > kPanelNativeWidth ? kDisplayWidth : kPanelNativeWidth;
constexpr int kMaxChunkPhysicalRows = kMaxChunkBytes / (kTxBufferWidth * kBytesPerPixel);
static_assert(kMaxChunkPhysicalRows > 0, "Display chunk buffer must hold at least one row");

constexpr size_t kTxBufferPixels = static_cast<size_t>(kTxBufferWidth) * kMaxChunkPhysicalRows;

struct TinyGlyph {
  char c;
  uint8_t rows[kTinyGlyphHeight];
};

DisplayManager::TypographyConfig &activeTypographyConfig() {
  static DisplayManager::TypographyConfig config;
  return config;
}

int clampTypographyTracking(int value) {
  return std::max(kTypographyTrackingMin, std::min(kTypographyTrackingMax, value));
}

int clampTypographyAnchorPercent(int value) {
  return std::max(kTypographyAnchorMin, std::min(kTypographyAnchorMax, value));
}

int clampTypographyGuideHalfWidth(int value) {
  return std::max(kTypographyGuideHalfWidthMin, std::min(kTypographyGuideHalfWidthMax, value));
}

int clampTypographyGuideGap(int value) {
  return std::max(kTypographyGuideGapMin, std::min(kTypographyGuideGapMax, value));
}

int currentTypographyTrackingPx() {
  return clampTypographyTracking(activeTypographyConfig().trackingPx);
}

int currentAnchorPercent() {
  return clampTypographyAnchorPercent(activeTypographyConfig().anchorPercent);
}

int currentGuideHalfWidth() {
  return clampTypographyGuideHalfWidth(activeTypographyConfig().guideHalfWidth);
}

int currentGuideGap() {
  return clampTypographyGuideGap(activeTypographyConfig().guideGap);
}

struct ReaderTextStyle {
  uint8_t scalePercent;
  int currentGap;
  uint8_t alpha;
};

constexpr TinyGlyph kTinyGlyphs[] = {
    {' ', {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
    {'!', {0x04, 0x04, 0x04, 0x04, 0x04, 0x00, 0x04}},
    {'"', {0x0A, 0x0A, 0x0A, 0x00, 0x00, 0x00, 0x00}},
    {'#', {0x0A, 0x0A, 0x1F, 0x0A, 0x1F, 0x0A, 0x0A}},
    {'%', {0x19, 0x19, 0x02, 0x04, 0x08, 0x13, 0x13}},
    {'&', {0x0C, 0x12, 0x14, 0x08, 0x15, 0x12, 0x0D}},
    {'\'', {0x04, 0x04, 0x08, 0x00, 0x00, 0x00, 0x00}},
    {'(', {0x02, 0x04, 0x08, 0x08, 0x08, 0x04, 0x02}},
    {')', {0x08, 0x04, 0x02, 0x02, 0x02, 0x04, 0x08}},
    {'*', {0x00, 0x15, 0x0E, 0x1F, 0x0E, 0x15, 0x00}},
    {'+', {0x00, 0x04, 0x04, 0x1F, 0x04, 0x04, 0x00}},
    {',', {0x00, 0x00, 0x00, 0x00, 0x06, 0x04, 0x08}},
    {'-', {0x00, 0x00, 0x00, 0x1F, 0x00, 0x00, 0x00}},
    {'.', {0x00, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x0C}},
    {'/', {0x01, 0x02, 0x02, 0x04, 0x08, 0x08, 0x10}},
    {'0', {0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E}},
    {'1', {0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E}},
    {'2', {0x0E, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1F}},
    {'3', {0x1E, 0x01, 0x01, 0x0E, 0x01, 0x01, 0x1E}},
    {'4', {0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02}},
    {'5', {0x1F, 0x10, 0x10, 0x1E, 0x01, 0x01, 0x1E}},
    {'6', {0x0E, 0x10, 0x10, 0x1E, 0x11, 0x11, 0x0E}},
    {'7', {0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08}},
    {'8', {0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E}},
    {'9', {0x0E, 0x11, 0x11, 0x0F, 0x01, 0x01, 0x0E}},
    {':', {0x00, 0x0C, 0x0C, 0x00, 0x0C, 0x0C, 0x00}},
    {';', {0x00, 0x0C, 0x0C, 0x00, 0x06, 0x04, 0x08}},
    {'?', {0x0E, 0x11, 0x01, 0x02, 0x04, 0x00, 0x04}},
    {'>', {0x10, 0x08, 0x04, 0x02, 0x04, 0x08, 0x10}},
    {'A', {0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11}},
    {'B', {0x1E, 0x11, 0x11, 0x1E, 0x11, 0x11, 0x1E}},
    {'C', {0x0E, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0E}},
    {'D', {0x1E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x1E}},
    {'E', {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F}},
    {'F', {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x10}},
    {'G', {0x0E, 0x11, 0x10, 0x17, 0x11, 0x11, 0x0F}},
    {'H', {0x11, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11}},
    {'I', {0x0E, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0E}},
    {'J', {0x07, 0x02, 0x02, 0x02, 0x12, 0x12, 0x0C}},
    {'K', {0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11}},
    {'L', {0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F}},
    {'M', {0x11, 0x1B, 0x15, 0x15, 0x11, 0x11, 0x11}},
    {'N', {0x11, 0x19, 0x15, 0x13, 0x11, 0x11, 0x11}},
    {'O', {0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E}},
    {'P', {0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x10}},
    {'Q', {0x0E, 0x11, 0x11, 0x11, 0x15, 0x12, 0x0D}},
    {'R', {0x1E, 0x11, 0x11, 0x1E, 0x14, 0x12, 0x11}},
    {'S', {0x0F, 0x10, 0x10, 0x0E, 0x01, 0x01, 0x1E}},
    {'T', {0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04}},
    {'U', {0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E}},
    {'V', {0x11, 0x11, 0x11, 0x11, 0x0A, 0x0A, 0x04}},
    {'W', {0x11, 0x11, 0x11, 0x15, 0x15, 0x1B, 0x11}},
    {'X', {0x11, 0x0A, 0x04, 0x04, 0x04, 0x0A, 0x11}},
    {'Y', {0x11, 0x0A, 0x04, 0x04, 0x04, 0x04, 0x04}},
    {'Z', {0x1F, 0x01, 0x02, 0x04, 0x08, 0x10, 0x1F}},
    {'_', {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1F}},
};

const EmbeddedSerifGlyph &glyphFor(char c) {
  if (c < static_cast<char>(kEmbeddedSerifFirstChar) ||
      c > static_cast<char>(kEmbeddedSerifLastChar)) {
    c = '?';
  }

  return kEmbeddedSerifGlyphs[static_cast<uint8_t>(c) - kEmbeddedSerifFirstChar];
}

const EmbeddedSerif70Glyph &glyph70For(char c) {
  if (c < static_cast<char>(kEmbeddedSerif70FirstChar) ||
      c > static_cast<char>(kEmbeddedSerif70LastChar)) {
    c = '?';
  }

  return kEmbeddedSerif70Glyphs[static_cast<uint8_t>(c) - kEmbeddedSerif70FirstChar];
}

const uint8_t *tinyRowsFor(char c) {
  if (c >= 'a' && c <= 'z') {
    c = static_cast<char>(c - 'a' + 'A');
  }

  for (const TinyGlyph &glyph : kTinyGlyphs) {
    if (glyph.c == c) {
      return glyph.rows;
    }
  }

  for (const TinyGlyph &glyph : kTinyGlyphs) {
    if (glyph.c == ' ') {
      return glyph.rows;
    }
  }

  return kTinyGlyphs[0].rows;
}

uint16_t panelColor(uint16_t rgb565) {
  return static_cast<uint16_t>((rgb565 << 8) | (rgb565 >> 8));
}

bool isWordCharacter(char c) {
  return std::isalnum(static_cast<unsigned char>(c)) != 0;
}

int scaledAdvance(int value, int divisor) {
  divisor = std::max(1, divisor);
  return std::max(1, (value + divisor - 1) / divisor);
}

int scaledSignedAdvance(int value, int divisor) {
  divisor = std::max(1, divisor);
  if (value >= 0) {
    return value / divisor;
  }
  return -(((-value) + divisor - 1) / divisor);
}

int scaledPercentDimension(int value, uint8_t scalePercent) {
  if (scalePercent == 0) {
    scalePercent = 1;
  }
  return std::max(1, (value * static_cast<int>(scalePercent) + 99) / 100);
}

int scaledSignedPercent(int value, uint8_t scalePercent) {
  if (scalePercent == 0) {
    scalePercent = 1;
  }
  if (value >= 0) {
    return (value * static_cast<int>(scalePercent) + 50) / 100;
  }
  return -(((-value) * static_cast<int>(scalePercent) + 50) / 100);
}

int trackedAdvance(int advance, size_t index, size_t length) {
  if (index + 1 >= length) {
    return advance;
  }
  return std::max(1, advance + currentTypographyTrackingPx());
}

int trackedAdvanceScaled(int advance, int divisor, size_t index, size_t length) {
  const int scaled = scaledAdvance(advance, divisor);
  if (index + 1 >= length) {
    return scaled;
  }
  return std::max(1, scaled + scaledSignedAdvance(currentTypographyTrackingPx(), divisor));
}

int trackedAdvanceScaledPercent(int advance, uint8_t scalePercent, size_t index, size_t length) {
  const int scaled = scaledPercentDimension(advance, scalePercent);
  if (index + 1 >= length) {
    return scaled;
  }
  return std::max(1, scaled + scaledSignedPercent(currentTypographyTrackingPx(), scalePercent));
}

int opticalKerningAdjustment(char currentChar, char nextChar, int currentXOffset, int currentWidth,
                             int trackedAdvanceValue, int nextXOffset, int desiredGap) {
  if (!isWordCharacter(currentChar) || !isWordCharacter(nextChar) || currentWidth <= 0) {
    return 0;
  }

  desiredGap = std::max(1, desiredGap);
  const int visibleGap =
      trackedAdvanceValue + nextXOffset - (currentXOffset + currentWidth);
  if (visibleGap <= desiredGap) {
    return 0;
  }

  return std::min(visibleGap - desiredGap, std::max(0, trackedAdvanceValue - 1));
}

int regularDesiredGap() { return std::max(1, kOpticalLetterGapPx + currentTypographyTrackingPx()); }

int scaledDesiredGap(int divisor) {
  return std::max(1, scaledAdvance(kOpticalLetterGapPx, divisor) +
                         scaledSignedAdvance(currentTypographyTrackingPx(), divisor));
}

int scaledPercentDesiredGap(uint8_t scalePercent) {
  return std::max(1, scaledPercentDimension(kOpticalLetterGapPx, scalePercent) +
                         scaledSignedPercent(currentTypographyTrackingPx(), scalePercent));
}

struct TextLayoutMetrics {
  int minX = 0;
  int maxX = 0;
  int focusCenterX = 0;
  bool hasPixels = false;
};

void updateTextLayoutBounds(TextLayoutMetrics &layout, int left, int width) {
  if (width <= 0) {
    return;
  }

  const int right = left + width;
  if (!layout.hasPixels) {
    layout.minX = left;
    layout.maxX = right;
    layout.hasPixels = true;
    return;
  }

  layout.minX = std::min(layout.minX, left);
  layout.maxX = std::max(layout.maxX, right);
}

int textLayoutWidth(const TextLayoutMetrics &layout) {
  if (!layout.hasPixels) {
    return 0;
  }
  return std::max(0, layout.maxX - layout.minX);
}

TextLayoutMetrics serifWordLayout(const String &word, int focusIndex, int divisor = 1) {
  TextLayoutMetrics layout;
  int cursorX = 0;
  const bool trackFocus = focusIndex >= 0;

  for (size_t i = 0; i < word.length(); ++i) {
    const EmbeddedSerifGlyph &glyph = glyphFor(word[i]);
    const int xOffset = scaledSignedAdvance(static_cast<int>(glyph.xOffset), divisor);
    const int width = glyph.width == 0 ? 0 : scaledAdvance(static_cast<int>(glyph.width), divisor);
    const int advance = scaledAdvance(static_cast<int>(glyph.xAdvance), divisor);
    const int left = cursorX + xOffset;
    updateTextLayoutBounds(layout, left, width);

    if (trackFocus && static_cast<int>(i) == focusIndex) {
      layout.focusCenterX = width > 0 ? left + (width / 2) : cursorX + (advance / 2);
    }

    int tracked = trackedAdvanceScaled(static_cast<int>(glyph.xAdvance), divisor, i, word.length());
    if (i + 1 < word.length()) {
      const EmbeddedSerifGlyph &nextGlyph = glyphFor(word[i + 1]);
      tracked -= opticalKerningAdjustment(
          word[i], word[i + 1], xOffset, width, tracked,
          scaledSignedAdvance(static_cast<int>(nextGlyph.xOffset), divisor), scaledDesiredGap(divisor));
    }
    cursorX += std::max(1, tracked);
  }

  if (!trackFocus && layout.hasPixels) {
    layout.focusCenterX = layout.minX + (textLayoutWidth(layout) / 2);
  }

  return layout;
}

TextLayoutMetrics serifWordLayoutScaledPercent(const String &word, int focusIndex,
                                               uint8_t scalePercent) {
  TextLayoutMetrics layout;
  int cursorX = 0;
  const bool trackFocus = focusIndex >= 0;

  for (size_t i = 0; i < word.length(); ++i) {
    const EmbeddedSerifGlyph &glyph = glyphFor(word[i]);
    const int xOffset = scaledSignedPercent(static_cast<int>(glyph.xOffset), scalePercent);
    const int width =
        glyph.width == 0 ? 0 : scaledPercentDimension(static_cast<int>(glyph.width), scalePercent);
    const int advance = scaledPercentDimension(static_cast<int>(glyph.xAdvance), scalePercent);
    const int left = cursorX + xOffset;
    updateTextLayoutBounds(layout, left, width);

    if (trackFocus && static_cast<int>(i) == focusIndex) {
      layout.focusCenterX = width > 0 ? left + (width / 2) : cursorX + (advance / 2);
    }

    int tracked =
        trackedAdvanceScaledPercent(static_cast<int>(glyph.xAdvance), scalePercent, i, word.length());
    if (i + 1 < word.length()) {
      const EmbeddedSerifGlyph &nextGlyph = glyphFor(word[i + 1]);
      tracked -= opticalKerningAdjustment(
          word[i], word[i + 1], xOffset, width, tracked,
          scaledSignedPercent(static_cast<int>(nextGlyph.xOffset), scalePercent),
          scaledPercentDesiredGap(scalePercent));
    }
    cursorX += std::max(1, tracked);
  }

  if (!trackFocus && layout.hasPixels) {
    layout.focusCenterX = layout.minX + (textLayoutWidth(layout) / 2);
  }

  return layout;
}

TextLayoutMetrics serif70WordLayout(const String &word, int focusIndex) {
  TextLayoutMetrics layout;
  int cursorX = 0;
  const bool trackFocus = focusIndex >= 0;

  for (size_t i = 0; i < word.length(); ++i) {
    const EmbeddedSerif70Glyph &glyph = glyph70For(word[i]);
    const int left = cursorX + static_cast<int>(glyph.xOffset);
    const int width = static_cast<int>(glyph.width);
    const int advance = static_cast<int>(glyph.xAdvance);
    updateTextLayoutBounds(layout, left, width);

    if (trackFocus && static_cast<int>(i) == focusIndex) {
      layout.focusCenterX = width > 0 ? left + (width / 2) : cursorX + (advance / 2);
    }

    int tracked = trackedAdvance(advance, i, word.length());
    if (i + 1 < word.length()) {
      const EmbeddedSerif70Glyph &nextGlyph = glyph70For(word[i + 1]);
      tracked -= opticalKerningAdjustment(word[i], word[i + 1], static_cast<int>(glyph.xOffset),
                                          width, tracked, static_cast<int>(nextGlyph.xOffset),
                                          regularDesiredGap());
    }
    cursorX += std::max(1, tracked);
  }

  if (!trackFocus && layout.hasPixels) {
    layout.focusCenterX = layout.minX + (textLayoutWidth(layout) / 2);
  }

  return layout;
}

int serifWordWidth(const String &word) { return textLayoutWidth(serifWordLayout(word, -1)); }

int scaledWordWidth(const String &word, int divisor) {
  return textLayoutWidth(serifWordLayout(word, -1, divisor));
}

int scaledWordWidthPercent(const String &word, uint8_t scalePercent) {
  return textLayoutWidth(serifWordLayoutScaledPercent(word, -1, scalePercent));
}

ReaderTextStyle readerTextStyle(uint8_t fontSizeLevel) {
  static constexpr ReaderTextStyle kStyles[] = {
      {100, kPhantomCurrentGapLarge, kPhantomAlphaLarge},
      {70, kPhantomCurrentGapMedium, kPhantomAlphaMedium},
      {50, kPhantomCurrentGapSmall, kPhantomAlphaSmall},
  };

  const size_t styleCount = sizeof(kStyles) / sizeof(kStyles[0]);
  if (fontSizeLevel >= styleCount) {
    fontSizeLevel = 0;
  }
  return kStyles[fontSizeLevel];
}

int orpOrdinalForLength(int length) {
  if (length <= 1) {
    return 0;
  }
  if (length <= 5) {
    return 1;
  }
  if (length <= 9) {
    return 2;
  }
  if (length <= 13) {
    return 3;
  }
  return 4;
}

int findFocusLetterIndex(const String &word) {
  int wordCharacterCount = 0;
  for (size_t i = 0; i < word.length(); ++i) {
    if (isWordCharacter(word[i])) {
      ++wordCharacterCount;
    }
  }

  if (wordCharacterCount == 0) {
    return word.length() > 0 ? 0 : -1;
  }

  const int targetOrdinal = std::min(orpOrdinalForLength(wordCharacterCount), wordCharacterCount - 1);
  int currentOrdinal = 0;
  for (size_t i = 0; i < word.length(); ++i) {
    if (!isWordCharacter(word[i])) {
      continue;
    }
    if (currentOrdinal == targetOrdinal) {
      return static_cast<int>(i);
    }
    ++currentOrdinal;
  }

  return 0;
}

int rsvpStartX(const String &word, int focusIndex, int virtualWidth, int divisor = 1,
               bool clampToMargins = true) {
  const TextLayoutMetrics layout = serifWordLayout(word, focusIndex, divisor);
  const int wordWidth = textLayoutWidth(layout);
  if (focusIndex < 0) {
    return ((virtualWidth - wordWidth) / 2) - layout.minX;
  }

  const int anchorX = (virtualWidth * currentAnchorPercent()) / 100;
  const int x = anchorX - layout.focusCenterX;
  if (!clampToMargins) {
    return x;
  }
  const int minX = kRsvpSideMargin - layout.minX;
  const int maxX = virtualWidth - kRsvpSideMargin - layout.maxX;

  if (maxX < minX) {
    return x;
  }

  return std::max(minX, std::min(maxX, x));
}

int rsvpStartXScaledPercent(const String &word, int focusIndex, int virtualWidth,
                            uint8_t scalePercent, bool clampToMargins = true) {
  const TextLayoutMetrics layout = serifWordLayoutScaledPercent(word, focusIndex, scalePercent);
  const int wordWidth = textLayoutWidth(layout);
  if (focusIndex < 0) {
    return ((virtualWidth - wordWidth) / 2) - layout.minX;
  }

  const int anchorX = (virtualWidth * currentAnchorPercent()) / 100;
  const int x = anchorX - layout.focusCenterX;
  if (!clampToMargins) {
    return x;
  }
  const int minX = kRsvpSideMargin - layout.minX;
  const int maxX = virtualWidth - kRsvpSideMargin - layout.maxX;

  if (maxX < minX) {
    return x;
  }

  return std::max(minX, std::min(maxX, x));
}

int serif70WordWidth(const String &word) { return textLayoutWidth(serif70WordLayout(word, -1)); }

int rsvpStartX70(const String &word, int focusIndex, int virtualWidth, bool clampToMargins = true) {
  const TextLayoutMetrics layout = serif70WordLayout(word, focusIndex);
  const int wordWidth = textLayoutWidth(layout);
  if (focusIndex < 0) {
    return ((virtualWidth - wordWidth) / 2) - layout.minX;
  }

  const int anchorX = (virtualWidth * currentAnchorPercent()) / 100;
  const int x = anchorX - layout.focusCenterX;
  if (!clampToMargins) {
    return x;
  }

  const int minX = kRsvpSideMargin - layout.minX;
  const int maxX = virtualWidth - kRsvpSideMargin - layout.maxX;
  if (maxX < minX) {
    return x;
  }

  return std::max(minX, std::min(maxX, x));
}

}  // namespace

static const char *kDisplayTag = "display";

DisplayManager::~DisplayManager() {
  if (virtualFrame_ != nullptr) {
    heap_caps_free(virtualFrame_);
    virtualFrame_ = nullptr;
  }

  if (txBuffer_ != nullptr) {
    heap_caps_free(txBuffer_);
    txBuffer_ = nullptr;
  }
}

void DisplayManager::setBatteryLabel(const String &label) {
  if (batteryLabel_ == label) {
    return;
  }

  batteryLabel_ = label;
  lastRenderKey_ = "";
}

void DisplayManager::setBrightnessPercent(uint8_t percent) {
  if (percent == 0) {
    percent = 1;
  } else if (percent > 100) {
    percent = 100;
  }

  brightnessPercent_ = percent;
  if (initialized_) {
    applyBrightness();
  }
}

void DisplayManager::setDarkMode(bool darkMode) {
  if (darkMode_ == darkMode) {
    return;
  }

  darkMode_ = darkMode;
  lastRenderKey_ = "";
}

void DisplayManager::setNightMode(bool nightMode) {
  if (nightMode_ == nightMode) {
    return;
  }

  nightMode_ = nightMode;
  lastRenderKey_ = "";
}

void DisplayManager::setTypographyConfig(const TypographyConfig &config) {
  TypographyConfig next;
  next.trackingPx = static_cast<int8_t>(clampTypographyTracking(config.trackingPx));
  next.anchorPercent = static_cast<uint8_t>(clampTypographyAnchorPercent(config.anchorPercent));
  next.guideHalfWidth =
      static_cast<uint8_t>(clampTypographyGuideHalfWidth(config.guideHalfWidth));
  next.guideGap = static_cast<uint8_t>(clampTypographyGuideGap(config.guideGap));

  TypographyConfig &current = activeTypographyConfig();
  if (current.trackingPx == next.trackingPx && current.anchorPercent == next.anchorPercent &&
      current.guideHalfWidth == next.guideHalfWidth && current.guideGap == next.guideGap) {
    return;
  }

  current = next;
  lastRenderKey_ = "";
}

DisplayManager::TypographyConfig DisplayManager::typographyConfig() const {
  return activeTypographyConfig();
}

bool DisplayManager::darkMode() const { return darkMode_; }

bool DisplayManager::nightMode() const { return nightMode_; }

bool DisplayManager::begin() {
  ESP_LOGI(kDisplayTag, "Begin");

  if (!allocateBuffers()) {
    ESP_LOGE(kDisplayTag, "Buffer allocation failed");
    return false;
  }
  ESP_LOGI(kDisplayTag, "Buffers ready");

  if (!initPanel()) {
    ESP_LOGE(kDisplayTag, "Panel init failed");
    return false;
  }

  initialized_ = true;
  lastRenderKey_ = "";
  fillScreen(backgroundColor());
  applyBrightness();
  ESP_LOGI(kDisplayTag, "AXS15231B LCD initialized");
  return true;
}

void DisplayManager::prepareForSleep() {
  if (!initialized_) {
    return;
  }

  fillScreen(kTrueBlack);
  Panel::sleep();
  initialized_ = false;
  lastRenderKey_ = "";
}

bool DisplayManager::wakeFromSleep() {
  if (!allocateBuffers()) {
    ESP_LOGE(kDisplayTag, "Buffer allocation failed after wake");
    return false;
  }

  Panel::wake();
  initialized_ = true;
  lastRenderKey_ = "";
  applyBrightness();
  return true;
}

bool DisplayManager::allocateBuffers() {
  if (virtualFrame_ == nullptr) {
    virtualFrame_ = static_cast<uint16_t *>(
        heap_caps_malloc(kVirtualBufferWidth * kVirtualBufferHeight * sizeof(uint16_t),
                         MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (virtualFrame_ == nullptr) {
      virtualFrame_ = static_cast<uint16_t *>(
          heap_caps_malloc(kVirtualBufferWidth * kVirtualBufferHeight * sizeof(uint16_t),
                           MALLOC_CAP_8BIT));
    }
  }

  if (txBuffer_ == nullptr) {
    txBufferBytes_ = kTxBufferPixels * sizeof(uint16_t);
    txBuffer_ = static_cast<uint16_t *>(
        heap_caps_malloc(txBufferBytes_, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL));
  }

  return virtualFrame_ != nullptr && txBuffer_ != nullptr;
}

bool DisplayManager::initPanel() {
  Panel::init();
  ESP_LOGI(kDisplayTag, "Panel init sequence complete");
  return true;
}

bool DisplayManager::drawBitmap(int xStart, int yStart, int xEnd, int yEnd, const void *colorData) {
  if (colorData == nullptr || xEnd <= xStart || yEnd <= yStart) {
    return false;
  }

  Panel::pushColors(static_cast<uint16_t>(xStart), static_cast<uint16_t>(yStart),
                    static_cast<uint16_t>(xEnd - xStart),
                    static_cast<uint16_t>(yEnd - yStart),
                    static_cast<const uint16_t *>(colorData));
  return true;
}

void DisplayManager::fillScreen(uint16_t color) {
  if (txBuffer_ == nullptr) {
    return;
  }

  const size_t pixelsPerChunk = static_cast<size_t>(kPanelNativeWidth) * kMaxChunkPhysicalRows;
  for (size_t i = 0; i < pixelsPerChunk; ++i) {
    txBuffer_[i] = panelColor(color);
  }

  for (int yStart = 0; yStart < kPanelNativeHeight; yStart += kMaxChunkPhysicalRows) {
    const int rows = std::min(kMaxChunkPhysicalRows, kPanelNativeHeight - yStart);
    if (!drawBitmap(0, yStart, kPanelNativeWidth, yStart + rows, txBuffer_)) {
      return;
    }
  }
}

void DisplayManager::clearVirtualBuffer(int width, int height) {
  const uint16_t background = panelColor(backgroundColor());
  for (int row = 0; row < height; ++row) {
    std::fill_n(virtualFrame_ + row * kVirtualBufferWidth, width, background);
  }
}

uint16_t DisplayManager::backgroundColor() const {
  if (nightMode_) {
    return kTrueBlack;
  }
  return darkMode_ ? kTrueBlack : kPureWhite;
}

uint16_t DisplayManager::wordColor() const {
  if (nightMode_) {
    return kNightWordColor;
  }
  return darkMode_ ? kDarkWordColor : kLightWordColor;
}

uint16_t DisplayManager::focusColor() const {
  if (nightMode_) {
    return kNightFocusColor;
  }
  return kFocusLetterColor;
}

uint16_t DisplayManager::dimColor() const {
  if (nightMode_) {
    return blendOverBackground(wordColor(), kNightDimAlpha);
  }
  return darkMode_ ? kDarkMenuDimColor : kLightMenuDimColor;
}

uint16_t DisplayManager::footerColor() const {
  if (nightMode_) {
    return blendOverBackground(wordColor(), kNightFooterAlpha);
  }
  return darkMode_ ? kDarkFooterColor : kLightFooterColor;
}

uint16_t DisplayManager::selectedBarColor() const {
  return nightMode_ ? focusColor() : kFocusLetterColor;
}

uint16_t DisplayManager::blendOverBackground(uint16_t rgb565, uint8_t alpha) const {
  if (alpha >= 250) {
    return rgb565;
  }

  const uint16_t bg = backgroundColor();
  const uint32_t inverseAlpha = 255U - alpha;
  const uint32_t r =
      ((((rgb565 >> 11) & 0x1F) * alpha) + (((bg >> 11) & 0x1F) * inverseAlpha)) / 255U;
  const uint32_t g =
      ((((rgb565 >> 5) & 0x3F) * alpha) + (((bg >> 5) & 0x3F) * inverseAlpha)) / 255U;
  const uint32_t b = (((rgb565 & 0x1F) * alpha) + ((bg & 0x1F) * inverseAlpha)) / 255U;
  return static_cast<uint16_t>((r << 11) | (g << 5) | b);
}

int DisplayManager::chooseTextScale(const String &word) const {
  const int usableWidth = std::max(1, measureTextWidth(word));
  const int maxScaleX = kDisplayWidth / usableWidth;
  const int maxScaleY = kDisplayHeight / kBaseGlyphHeight;
  const int maxScale = std::min(kMaxTextScale, std::min(maxScaleX, maxScaleY));
  return std::max(1, maxScale);
}

int DisplayManager::measureTextWidth(const String &word) const {
  return textLayoutWidth(serifWordLayout(word, -1));
}

int DisplayManager::measureSerifTextWidth(const String &text, int divisor) const {
  return textLayoutWidth(serifWordLayout(text, -1, divisor));
}

int DisplayManager::measureSerif70TextWidth(const String &text) const {
  return textLayoutWidth(serif70WordLayout(text, -1));
}

int DisplayManager::measureSerifTextWidthScaled(const String &text, uint8_t scalePercent) const {
  return textLayoutWidth(serifWordLayoutScaledPercent(text, -1, scalePercent));
}

int DisplayManager::measureTinyTextWidth(const String &text, int scale) const {
  if (text.isEmpty()) {
    return 0;
  }
  return static_cast<int>(text.length()) * (kTinyGlyphWidth + kTinyGlyphSpacing) * scale -
         kTinyGlyphSpacing * scale;
}

String DisplayManager::fitSerifText(const String &text, int maxWidth, int divisor) const {
  if (measureSerifTextWidth(text, divisor) <= maxWidth) {
    return text;
  }

  String fitted = text;
  const String ellipsis = "...";
  while (!fitted.isEmpty() && measureSerifTextWidth(fitted + ellipsis, divisor) > maxWidth) {
    fitted.remove(fitted.length() - 1);
  }
  fitted.trim();
  return fitted.isEmpty() ? ellipsis : fitted + ellipsis;
}

String DisplayManager::fitTinyText(const String &text, int maxWidth, int scale) const {
  if (measureTinyTextWidth(text, scale) <= maxWidth) {
    return text;
  }

  String fitted = text;
  const String ellipsis = "...";
  while (!fitted.isEmpty() && measureTinyTextWidth(fitted + ellipsis, scale) > maxWidth) {
    fitted.remove(fitted.length() - 1);
  }
  fitted.trim();
  return fitted.isEmpty() ? ellipsis : fitted + ellipsis;
}

void DisplayManager::drawGlyph(int x, int y, char c, uint16_t color) {
  const EmbeddedSerifGlyph &glyph = glyphFor(c);
  if (glyph.width == 0) {
    return;
  }

  const uint8_t *bitmap = kEmbeddedSerifBitmaps + glyph.bitmapOffset;
  for (int row = 0; row < kBaseGlyphHeight; ++row) {
    const int dstY = y + row;
    if (dstY < 0 || dstY >= kVirtualBufferHeight) {
      continue;
    }

    for (int col = 0; col < glyph.width; ++col) {
      const int dstX = x + col;
      if (dstX < 0 || dstX >= kVirtualBufferWidth) {
        continue;
      }

      const uint8_t alpha = bitmap[row * glyph.width + col];
      if (alpha < kGlyphAlphaThreshold) {
        continue;
      }

      virtualFrame_[dstY * kVirtualBufferWidth + dstX] =
          panelColor(blendOverBackground(color, alpha));
    }
  }
}

void DisplayManager::drawSerifGlyphScaled(int x, int y, char c, uint16_t color, int divisor) {
  divisor = std::max(1, divisor);
  const EmbeddedSerifGlyph &glyph = glyphFor(c);
  if (glyph.width == 0) {
    return;
  }

  const uint8_t *bitmap = kEmbeddedSerifBitmaps + glyph.bitmapOffset;
  const int scaledWidth = std::max(1, (glyph.width + divisor - 1) / divisor);
  const int scaledHeight = std::max(1, (kBaseGlyphHeight + divisor - 1) / divisor);

  for (int dstRow = 0; dstRow < scaledHeight; ++dstRow) {
    const int dstY = y + dstRow;
    if (dstY < 0 || dstY >= kVirtualBufferHeight) {
      continue;
    }

    const int sourceYStart = dstRow * divisor;
    const int sourceYEnd = std::min(kBaseGlyphHeight, sourceYStart + divisor);
    for (int dstCol = 0; dstCol < scaledWidth; ++dstCol) {
      const int dstX = x + dstCol;
      if (dstX < 0 || dstX >= kVirtualBufferWidth) {
        continue;
      }

      const int sourceXStart = dstCol * divisor;
      const int sourceXEnd = std::min(static_cast<int>(glyph.width), sourceXStart + divisor);
      uint32_t alphaSum = 0;
      uint32_t sampleCount = 0;
      for (int sourceY = sourceYStart; sourceY < sourceYEnd; ++sourceY) {
        for (int sourceX = sourceXStart; sourceX < sourceXEnd; ++sourceX) {
          alphaSum += bitmap[sourceY * glyph.width + sourceX];
          ++sampleCount;
        }
      }

      const uint8_t alpha =
          sampleCount == 0 ? 0 : static_cast<uint8_t>(alphaSum / sampleCount);
      if (alpha < kGlyphAlphaThreshold) {
        continue;
      }

      virtualFrame_[dstY * kVirtualBufferWidth + dstX] =
          panelColor(blendOverBackground(color, alpha));
    }
  }
}

void DisplayManager::drawSerif70Glyph(int x, int y, char c, uint16_t color) {
  const EmbeddedSerif70Glyph &glyph = glyph70For(c);
  if (glyph.width == 0) {
    return;
  }

  const uint8_t *bitmap = kEmbeddedSerif70Bitmaps + glyph.bitmapOffset;
  for (int row = 0; row < kEmbeddedSerif70Height; ++row) {
    const int dstY = y + row;
    if (dstY < 0 || dstY >= kVirtualBufferHeight) {
      continue;
    }

    for (int col = 0; col < glyph.width; ++col) {
      const int dstX = x + col;
      if (dstX < 0 || dstX >= kVirtualBufferWidth) {
        continue;
      }

      const uint8_t alpha = bitmap[row * glyph.width + col];
      if (alpha < kGlyphAlphaThreshold) {
        continue;
      }

      virtualFrame_[dstY * kVirtualBufferWidth + dstX] =
          panelColor(blendOverBackground(color, alpha));
    }
  }
}

void DisplayManager::drawSerifGlyphScaledPercent(int x, int y, char c, uint16_t color,
                                                 uint8_t scalePercent) {
  if (scalePercent >= 100) {
    drawGlyph(x, y, c, color);
    return;
  }

  const EmbeddedSerifGlyph &glyph = glyphFor(c);
  if (glyph.width == 0) {
    return;
  }

  const uint8_t *bitmap = kEmbeddedSerifBitmaps + glyph.bitmapOffset;
  const int scaledWidth = scaledPercentDimension(glyph.width, scalePercent);
  const int scaledHeight = scaledPercentDimension(kBaseGlyphHeight, scalePercent);

  for (int dstRow = 0; dstRow < scaledHeight; ++dstRow) {
    const int dstY = y + dstRow;
    if (dstY < 0 || dstY >= kVirtualBufferHeight) {
      continue;
    }

    const int sourceYStart = (dstRow * kBaseGlyphHeight) / scaledHeight;
    const int sourceYEnd =
        std::min(kBaseGlyphHeight, ((dstRow + 1) * kBaseGlyphHeight + scaledHeight - 1) / scaledHeight);
    for (int dstCol = 0; dstCol < scaledWidth; ++dstCol) {
      const int dstX = x + dstCol;
      if (dstX < 0 || dstX >= kVirtualBufferWidth) {
        continue;
      }

      const int sourceXStart = (dstCol * glyph.width) / scaledWidth;
      const int sourceXEnd =
          std::min(static_cast<int>(glyph.width),
                   ((dstCol + 1) * glyph.width + scaledWidth - 1) / scaledWidth);
      uint32_t alphaSum = 0;
      uint32_t sampleCount = 0;
      for (int sourceY = sourceYStart; sourceY < sourceYEnd; ++sourceY) {
        for (int sourceX = sourceXStart; sourceX < sourceXEnd; ++sourceX) {
          alphaSum += bitmap[sourceY * glyph.width + sourceX];
          ++sampleCount;
        }
      }

      const uint8_t alpha =
          sampleCount == 0 ? 0 : static_cast<uint8_t>(alphaSum / sampleCount);
      if (alpha < kGlyphAlphaThreshold) {
        continue;
      }

      virtualFrame_[dstY * kVirtualBufferWidth + dstX] =
          panelColor(blendOverBackground(color, alpha));
    }
  }
}

void DisplayManager::fillVirtualRect(int x, int y, int width, int height, uint16_t color) {
  const uint16_t panel = panelColor(color);
  const int xEnd = std::min(kVirtualBufferWidth, x + width);
  const int yEnd = std::min(kVirtualBufferHeight, y + height);
  x = std::max(0, x);
  y = std::max(0, y);

  for (int row = y; row < yEnd; ++row) {
    for (int col = x; col < xEnd; ++col) {
      virtualFrame_[row * kVirtualBufferWidth + col] = panel;
    }
  }
}

void DisplayManager::drawSerifTextAt(const String &text, int x, int y, uint16_t color,
                                     int divisor) {
  divisor = std::max(1, divisor);
  int cursorX = x;
  for (size_t i = 0; i < text.length(); ++i) {
    const EmbeddedSerifGlyph &glyph = glyphFor(text[i]);
    const int xOffset = scaledSignedAdvance(static_cast<int>(glyph.xOffset), divisor);
    const int width = glyph.width == 0 ? 0 : scaledAdvance(static_cast<int>(glyph.width), divisor);
    drawSerifGlyphScaled(cursorX + xOffset, y, text[i], color, divisor);
    int tracked = trackedAdvanceScaled(static_cast<int>(glyph.xAdvance), divisor, i, text.length());
    if (i + 1 < text.length()) {
      const EmbeddedSerifGlyph &nextGlyph = glyphFor(text[i + 1]);
      tracked -= opticalKerningAdjustment(
          text[i], text[i + 1], xOffset, width, tracked,
          scaledSignedAdvance(static_cast<int>(nextGlyph.xOffset), divisor), scaledDesiredGap(divisor));
    }
    cursorX += std::max(1, tracked);
  }
}

void DisplayManager::drawSerif70TextAt(const String &text, int x, int y, uint16_t color) {
  int cursorX = x;
  for (size_t i = 0; i < text.length(); ++i) {
    const EmbeddedSerif70Glyph &glyph = glyph70For(text[i]);
    drawSerif70Glyph(cursorX + static_cast<int>(glyph.xOffset), y, text[i], color);
    int tracked = trackedAdvance(static_cast<int>(glyph.xAdvance), i, text.length());
    if (i + 1 < text.length()) {
      const EmbeddedSerif70Glyph &nextGlyph = glyph70For(text[i + 1]);
      tracked -= opticalKerningAdjustment(text[i], text[i + 1], static_cast<int>(glyph.xOffset),
                                          static_cast<int>(glyph.width), tracked,
                                          static_cast<int>(nextGlyph.xOffset), regularDesiredGap());
    }
    cursorX += std::max(1, tracked);
  }
}

void DisplayManager::drawSerifTextScaledAt(const String &text, int x, int y, uint16_t color,
                                           uint8_t scalePercent) {
  int cursorX = x;
  for (size_t i = 0; i < text.length(); ++i) {
    const EmbeddedSerifGlyph &glyph = glyphFor(text[i]);
    const int xOffset = scaledSignedPercent(static_cast<int>(glyph.xOffset), scalePercent);
    const int width = glyph.width == 0
                          ? 0
                          : scaledPercentDimension(static_cast<int>(glyph.width), scalePercent);
    drawSerifGlyphScaledPercent(cursorX + xOffset, y, text[i], color, scalePercent);
    int tracked =
        trackedAdvanceScaledPercent(static_cast<int>(glyph.xAdvance), scalePercent, i, text.length());
    if (i + 1 < text.length()) {
      const EmbeddedSerifGlyph &nextGlyph = glyphFor(text[i + 1]);
      tracked -= opticalKerningAdjustment(
          text[i], text[i + 1], xOffset, width, tracked,
          scaledSignedPercent(static_cast<int>(nextGlyph.xOffset), scalePercent),
          scaledPercentDesiredGap(scalePercent));
    }
    cursorX += std::max(1, tracked);
  }
}

void DisplayManager::drawTinyGlyph(int x, int y, char c, uint16_t color, int scale) {
  const uint8_t *rows = tinyRowsFor(c);
  const uint16_t panel = panelColor(color);

  for (int row = 0; row < kTinyGlyphHeight; ++row) {
    for (int col = 0; col < kTinyGlyphWidth; ++col) {
      if ((rows[row] & (1 << (kTinyGlyphWidth - 1 - col))) == 0) {
        continue;
      }

      for (int yy = 0; yy < scale; ++yy) {
        const int dstY = y + row * scale + yy;
        if (dstY < 0 || dstY >= kVirtualBufferHeight) {
          continue;
        }

        for (int xx = 0; xx < scale; ++xx) {
          const int dstX = x + col * scale + xx;
          if (dstX < 0 || dstX >= kVirtualBufferWidth) {
            continue;
          }
          virtualFrame_[dstY * kVirtualBufferWidth + dstX] = panel;
        }
      }
    }
  }
}

void DisplayManager::drawTinyTextAt(const String &text, int x, int y, uint16_t color, int scale) {
  int cursorX = x;
  for (size_t i = 0; i < text.length(); ++i) {
    drawTinyGlyph(cursorX, y, text[i], color, scale);
    cursorX += (kTinyGlyphWidth + kTinyGlyphSpacing) * scale;
  }
}

void DisplayManager::drawTinyTextCentered(const String &text, int y, uint16_t color, int scale) {
  const int textWidth = measureTinyTextWidth(text, scale);
  drawTinyTextAt(text, std::max(0, (kVirtualBufferWidth - textWidth) / 2), y, color, scale);
}

void DisplayManager::drawBatteryBadge() {
  if (batteryLabel_.isEmpty()) {
    return;
  }

  const int width = measureTinyTextWidth(batteryLabel_, kTinyScale);
  const int x = std::max(kFooterMarginX, kDisplayWidth - kFooterMarginX - width);
  drawTinyTextAt(batteryLabel_, x, kFooterMarginBottom, footerColor(), kTinyScale);
}

void DisplayManager::drawFooter(const String &chapterLabel, uint8_t progressPercent) {
  const String percent = String(progressPercent) + "%";
  const int y = kDisplayHeight - kTinyGlyphHeight * kTinyScale - kFooterMarginBottom;
  const int percentWidth = measureTinyTextWidth(percent, kTinyScale);
  const int rightX = std::max(kFooterMarginX, kDisplayWidth - kFooterMarginX - percentWidth);
  const int maxChapterWidth = std::max(0, rightX - kFooterMarginX - 18);
  const String chapter = fitTinyText(chapterLabel.isEmpty() ? "START" : chapterLabel,
                                    maxChapterWidth, kTinyScale);

  drawTinyTextAt(chapter, kFooterMarginX, y, footerColor(), kTinyScale);
  drawTinyTextAt(percent, rightX, y, footerColor(), kTinyScale);
}

void DisplayManager::drawRsvpAnchorGuide(int anchorX, int textY, int textHeight) {
  const int topY = std::max(2, textY - kRsvpGuideTopOffset);
  const int bottomY = std::min(kVirtualBufferHeight - 3, textY + textHeight + kRsvpGuideBottomOffset);
  const int guideHalfWidth = currentGuideHalfWidth();
  const int guideGap = currentGuideGap();
  const int leftX = std::max(0, anchorX - guideHalfWidth);
  const int rightX = std::min(kVirtualBufferWidth - 1, anchorX + guideHalfWidth);
  const int leftWidth = std::max(0, (anchorX - guideGap) - leftX);
  const int rightWidth = std::max(0, rightX - (anchorX + guideGap) + 1);
  const uint16_t guideColor = blendOverBackground(wordColor(), nightMode_ ? 136 : 96);

  fillVirtualRect(leftX, topY, leftWidth, 1, guideColor);
  fillVirtualRect(anchorX + guideGap, topY, rightWidth, 1, guideColor);
  fillVirtualRect(leftX, bottomY, leftWidth, 1, guideColor);
  fillVirtualRect(anchorX + guideGap, bottomY, rightWidth, 1, guideColor);
  fillVirtualRect(anchorX, topY, 1, kRsvpGuideTickHeight, focusColor());
  fillVirtualRect(anchorX, bottomY - kRsvpGuideTickHeight + 1, 1, kRsvpGuideTickHeight,
                  focusColor());
}

void DisplayManager::drawWordAt(const String &word, int x, int y, uint16_t color) {
  int cursorX = x;
  for (size_t i = 0; i < word.length(); ++i) {
    const EmbeddedSerifGlyph &glyph = glyphFor(word[i]);
    drawGlyph(cursorX + static_cast<int>(glyph.xOffset), y, word[i], color);
    int tracked = trackedAdvance(static_cast<int>(glyph.xAdvance), i, word.length());
    if (i + 1 < word.length()) {
      const EmbeddedSerifGlyph &nextGlyph = glyphFor(word[i + 1]);
      tracked -= opticalKerningAdjustment(word[i], word[i + 1], static_cast<int>(glyph.xOffset),
                                          static_cast<int>(glyph.width), tracked,
                                          static_cast<int>(nextGlyph.xOffset), regularDesiredGap());
    }
    cursorX += std::max(1, tracked);
  }
}

void DisplayManager::drawRsvpWordScaledAt(const String &word, int x, int y, int focusIndex,
                                          int divisor) {
  divisor = std::max(1, divisor);
  int cursorX = x;
  for (size_t i = 0; i < word.length(); ++i) {
    const EmbeddedSerifGlyph &glyph = glyphFor(word[i]);
    const uint16_t color = (static_cast<int>(i) == focusIndex) ? focusColor() : wordColor();
    const int xOffset = scaledSignedAdvance(static_cast<int>(glyph.xOffset), divisor);
    const int width =
        glyph.width == 0 ? 0 : scaledAdvance(static_cast<int>(glyph.width), divisor);
    drawSerifGlyphScaled(cursorX + xOffset, y, word[i], color, divisor);
    int tracked = trackedAdvanceScaled(static_cast<int>(glyph.xAdvance), divisor, i, word.length());
    if (i + 1 < word.length()) {
      const EmbeddedSerifGlyph &nextGlyph = glyphFor(word[i + 1]);
      tracked -= opticalKerningAdjustment(
          word[i], word[i + 1], xOffset, width, tracked,
          scaledSignedAdvance(static_cast<int>(nextGlyph.xOffset), divisor), scaledDesiredGap(divisor));
    }
    cursorX += std::max(1, tracked);
  }
}

void DisplayManager::drawRsvp70WordAt(const String &word, int x, int y, int focusIndex) {
  int cursorX = x;
  for (size_t i = 0; i < word.length(); ++i) {
    const EmbeddedSerif70Glyph &glyph = glyph70For(word[i]);
    const uint16_t color = (static_cast<int>(i) == focusIndex) ? focusColor() : wordColor();
    drawSerif70Glyph(cursorX + static_cast<int>(glyph.xOffset), y, word[i], color);
    int tracked = trackedAdvance(static_cast<int>(glyph.xAdvance), i, word.length());
    if (i + 1 < word.length()) {
      const EmbeddedSerif70Glyph &nextGlyph = glyph70For(word[i + 1]);
      tracked -= opticalKerningAdjustment(word[i], word[i + 1], static_cast<int>(glyph.xOffset),
                                          static_cast<int>(glyph.width), tracked,
                                          static_cast<int>(nextGlyph.xOffset), regularDesiredGap());
    }
    cursorX += std::max(1, tracked);
  }
}

void DisplayManager::drawRsvpWordScaledPercentAt(const String &word, int x, int y, int focusIndex,
                                                 uint8_t scalePercent) {
  int cursorX = x;
  for (size_t i = 0; i < word.length(); ++i) {
    const EmbeddedSerifGlyph &glyph = glyphFor(word[i]);
    const uint16_t color = (static_cast<int>(i) == focusIndex) ? focusColor() : wordColor();
    const int xOffset = scaledSignedPercent(static_cast<int>(glyph.xOffset), scalePercent);
    const int width = glyph.width == 0
                          ? 0
                          : scaledPercentDimension(static_cast<int>(glyph.width), scalePercent);
    drawSerifGlyphScaledPercent(cursorX + xOffset, y, word[i], color, scalePercent);
    int tracked =
        trackedAdvanceScaledPercent(static_cast<int>(glyph.xAdvance), scalePercent, i, word.length());
    if (i + 1 < word.length()) {
      const EmbeddedSerifGlyph &nextGlyph = glyphFor(word[i + 1]);
      tracked -= opticalKerningAdjustment(
          word[i], word[i + 1], xOffset, width, tracked,
          scaledSignedPercent(static_cast<int>(nextGlyph.xOffset), scalePercent),
          scaledPercentDesiredGap(scalePercent));
    }
    cursorX += std::max(1, tracked);
  }
}

void DisplayManager::drawRsvpWordAt(const String &word, int x, int y, int focusIndex) {
  drawRsvpWordScaledAt(word, x, y, focusIndex, 1);
}

void DisplayManager::drawWordLine(const String &word, int y, uint16_t color) {
  const TextLayoutMetrics layout = serifWordLayout(word, -1);
  const int textWidth = textLayoutWidth(layout);
  const int x = std::max(0, ((kVirtualBufferWidth - textWidth) / 2) - layout.minX);
  drawWordAt(word, x, y, color);
}

void DisplayManager::drawMenuItem(const String &item, int y, bool selected) {
  drawWordLine(item, y, selected ? focusColor() : dimColor());
}

void DisplayManager::applyBrightness() {
  Panel::setBrightnessPercent(brightnessPercent_);
  Panel::setBacklight(true);
}

void DisplayManager::flushScaledFrame(int scale, int virtualWidth, int virtualHeight) {
  for (int nativeYStart = 0; nativeYStart < kPanelNativeHeight;
       nativeYStart += kMaxChunkPhysicalRows) {
    const int nativeRows = std::min(kMaxChunkPhysicalRows, kPanelNativeHeight - nativeYStart);
    std::memset(txBuffer_, 0, txBufferBytes_);

    for (int localNativeY = 0; localNativeY < nativeRows; ++localNativeY) {
      const int nativeY = nativeYStart + localNativeY;
      uint16_t *dstRow = txBuffer_ + localNativeY * kPanelNativeWidth;

      for (int nativeX = 0; nativeX < kPanelNativeWidth; ++nativeX) {
        int logicalX = kDisplayWidth - 1 - nativeY;
        int logicalY = nativeX;
        if (BoardConfig::UI_ROTATED_180) {
          logicalX = nativeY;
          logicalY = kDisplayHeight - 1 - nativeX;
        }
        const int sourceX = logicalX / scale;
        const int sourceY = logicalY / scale;

        if (sourceX >= 0 && sourceX < virtualWidth && sourceY >= 0 && sourceY < virtualHeight) {
          dstRow[nativeX] = virtualFrame_[sourceY * kVirtualBufferWidth + sourceX];
        }
      }
    }

    if (!drawBitmap(0, nativeYStart, kPanelNativeWidth, nativeYStart + nativeRows, txBuffer_)) {
      return;
    }
  }
}

void DisplayManager::renderCenteredWord(const String &word, uint16_t color) {
  String normalized = word;
  const uint16_t renderColor = (color == kPureWhite) ? wordColor() : color;
  const String renderKey = "center|" + normalized + "|" + String(renderColor) + "|b:" +
                           batteryLabel_ + "|d:" + String(darkMode_ ? 1 : 0) + "|n:" +
                           String(nightMode_ ? 1 : 0);

  if (!initialized_ || renderKey == lastRenderKey_) {
    return;
  }

  lastRenderKey_ = renderKey;

  const int scale = chooseTextScale(normalized);
  const int virtualWidth = (kDisplayWidth + scale - 1) / scale;
  const int virtualHeight = (kDisplayHeight + scale - 1) / scale;

  clearVirtualBuffer(virtualWidth, virtualHeight);
  const int y = std::max(0, (virtualHeight - kBaseGlyphHeight) / 2);
  drawWordLine(normalized, y, renderColor);
  drawBatteryBadge();

  flushScaledFrame(scale, virtualWidth, virtualHeight);
}

void DisplayManager::renderRsvpWord(const String &word, const String &chapterLabel,
                                    uint8_t progressPercent, bool showFooter) {
  const String renderKey =
      "rsvp|" + word + "|" + chapterLabel + "|" + String(progressPercent) + "|" +
      String(showFooter ? 1 : 0) + "|b:" + batteryLabel_ + "|d:" +
      String(darkMode_ ? 1 : 0) + "|n:" + String(nightMode_ ? 1 : 0);
  if (!initialized_ || renderKey == lastRenderKey_) {
    return;
  }

  lastRenderKey_ = renderKey;

  const int scale = 1;
  const int virtualWidth = kDisplayWidth;
  const int virtualHeight = kDisplayHeight;
  const int y = std::max(0, (virtualHeight - kBaseGlyphHeight) / 2);
  const int focusIndex = findFocusLetterIndex(word);
  const int x = rsvpStartX(word, focusIndex, virtualWidth, 1, false);
  const int anchorX = (virtualWidth * currentAnchorPercent()) / 100;

  clearVirtualBuffer(virtualWidth, virtualHeight);
  drawRsvpAnchorGuide(anchorX, y, kBaseGlyphHeight);
  drawRsvpWordAt(word, x, y, focusIndex);
  if (showFooter) {
    drawFooter(chapterLabel, progressPercent);
  }
  drawBatteryBadge();
  flushScaledFrame(scale, virtualWidth, virtualHeight);
}

void DisplayManager::renderRsvpWordWithWpm(const String &word, uint16_t wpm,
                                           const String &chapterLabel, uint8_t progressPercent,
                                           bool showFooter) {
  const String wpmText = String(wpm) + " WPM";
  const String renderKey =
      "rsvp_wpm|" + word + "|" + wpmText + "|" + chapterLabel + "|" +
      String(progressPercent) + "|" + String(showFooter ? 1 : 0) + "|b:" + batteryLabel_ +
      "|d:" + String(darkMode_ ? 1 : 0) + "|n:" + String(nightMode_ ? 1 : 0);
  if (!initialized_ || renderKey == lastRenderKey_) {
    return;
  }

  lastRenderKey_ = renderKey;

  const int scale = 1;
  const int virtualWidth = kDisplayWidth;
  const int virtualHeight = kDisplayHeight;
  const int wordY = std::max(0, (virtualHeight - kBaseGlyphHeight) / 2);
  const int wpmY =
      std::max(0, virtualHeight - kTinyGlyphHeight * kTinyScale - kWpmFeedbackBottomMargin - 24);
  const int focusIndex = findFocusLetterIndex(word);
  const int x = rsvpStartX(word, focusIndex, virtualWidth, 1, false);
  const int anchorX = (virtualWidth * currentAnchorPercent()) / 100;

  clearVirtualBuffer(virtualWidth, virtualHeight);
  drawRsvpAnchorGuide(anchorX, wordY, kBaseGlyphHeight);
  drawRsvpWordAt(word, x, wordY, focusIndex);
  drawTinyTextCentered(wpmText, wpmY, focusColor(), kTinyScale);
  if (showFooter) {
    drawFooter(chapterLabel, progressPercent);
  }
  drawBatteryBadge();
  flushScaledFrame(scale, virtualWidth, virtualHeight);
}

void DisplayManager::renderPhantomRsvpWord(const String &beforeText, const String &word,
                                           const String &afterText, uint8_t fontSizeLevel,
                                           const String &chapterLabel, uint8_t progressPercent,
                                           bool showFooter) {
  const String renderKey =
      "rsvp_phantom|" + beforeText + "|" + word + "|" + afterText + "|s:" +
      String(fontSizeLevel) + "|" + chapterLabel + "|" + String(progressPercent) + "|" +
      String(showFooter ? 1 : 0) + "|b:" + batteryLabel_ + "|d:" +
      String(darkMode_ ? 1 : 0) + "|n:" + String(nightMode_ ? 1 : 0);
  if (!initialized_ || renderKey == lastRenderKey_) {
    return;
  }

  lastRenderKey_ = renderKey;

  if (fontSizeLevel == 1) {
    const int scale = 1;
    const int virtualWidth = kDisplayWidth;
    const int virtualHeight = kDisplayHeight;
    const int textY = std::max(0, (virtualHeight - kEmbeddedSerif70Height) / 2);
    const int focusIndex = findFocusLetterIndex(word);
    const int currentX = rsvpStartX70(word, focusIndex, virtualWidth, false);
    const int anchorX = (virtualWidth * currentAnchorPercent()) / 100;
    const TextLayoutMetrics currentLayout = serif70WordLayout(word, focusIndex);
    const uint16_t phantomColor = blendOverBackground(wordColor(), kPhantomAlphaMedium);

    clearVirtualBuffer(virtualWidth, virtualHeight);
    drawRsvpAnchorGuide(anchorX, textY, kEmbeddedSerif70Height);
    if (!beforeText.isEmpty()) {
      const TextLayoutMetrics beforeLayout = serif70WordLayout(beforeText, -1);
      const int beforeX =
          currentX + currentLayout.minX - kPhantomCurrentGapMedium - beforeLayout.maxX;
      drawSerif70TextAt(beforeText, beforeX, textY, phantomColor);
    }
    drawRsvp70WordAt(word, currentX, textY, focusIndex);
    if (!afterText.isEmpty()) {
      const TextLayoutMetrics afterLayout = serif70WordLayout(afterText, -1);
      const int afterX =
          currentX + currentLayout.maxX + kPhantomCurrentGapMedium - afterLayout.minX;
      drawSerif70TextAt(afterText, afterX, textY, phantomColor);
    }
    if (showFooter) {
      drawFooter(chapterLabel, progressPercent);
    }
    drawBatteryBadge();
    flushScaledFrame(scale, virtualWidth, virtualHeight);
    return;
  }

  const ReaderTextStyle style = readerTextStyle(fontSizeLevel);
  const int scale = 1;
  const int virtualWidth = kDisplayWidth;
  const int virtualHeight = kDisplayHeight;
  const int textHeight = scaledPercentDimension(kBaseGlyphHeight, style.scalePercent);
  const int textY = std::max(0, (virtualHeight - textHeight) / 2);
  const int focusIndex = findFocusLetterIndex(word);
  const int currentX =
      rsvpStartXScaledPercent(word, focusIndex, virtualWidth, style.scalePercent, false);
  const int anchorX = (virtualWidth * currentAnchorPercent()) / 100;
  const TextLayoutMetrics currentLayout =
      serifWordLayoutScaledPercent(word, focusIndex, style.scalePercent);
  const uint16_t phantomColor = blendOverBackground(wordColor(), style.alpha);

  clearVirtualBuffer(virtualWidth, virtualHeight);
  drawRsvpAnchorGuide(anchorX, textY, textHeight);
  if (!beforeText.isEmpty()) {
    const TextLayoutMetrics beforeLayout =
        serifWordLayoutScaledPercent(beforeText, -1, style.scalePercent);
    const int beforeX = currentX + currentLayout.minX - style.currentGap - beforeLayout.maxX;
    drawSerifTextScaledAt(beforeText, beforeX, textY, phantomColor, style.scalePercent);
  }
  drawRsvpWordScaledPercentAt(word, currentX, textY, focusIndex, style.scalePercent);
  if (!afterText.isEmpty()) {
    const TextLayoutMetrics afterLayout =
        serifWordLayoutScaledPercent(afterText, -1, style.scalePercent);
    const int afterX = currentX + currentLayout.maxX + style.currentGap - afterLayout.minX;
    drawSerifTextScaledAt(afterText, afterX, textY, phantomColor, style.scalePercent);
  }
  if (showFooter) {
    drawFooter(chapterLabel, progressPercent);
  }
  drawBatteryBadge();
  flushScaledFrame(scale, virtualWidth, virtualHeight);
}

void DisplayManager::renderTypographyPreview(const String &beforeText, const String &word,
                                             const String &afterText, uint8_t fontSizeLevel,
                                             const String &title, const String &line1,
                                             const String &line2) {
  const TypographyConfig config = activeTypographyConfig();
  const String renderKey =
      "typography_preview|" + beforeText + "|" + word + "|" + afterText + "|s:" +
      String(fontSizeLevel) + "|" + title + "|" + line1 + "|" + line2 + "|t:" +
      String(static_cast<int>(config.trackingPx)) + "|a:" +
      String(static_cast<unsigned int>(config.anchorPercent)) + "|w:" +
      String(static_cast<unsigned int>(config.guideHalfWidth)) + "|g:" +
      String(static_cast<unsigned int>(config.guideGap)) + "|b:" + batteryLabel_ + "|d:" +
      String(darkMode_ ? 1 : 0) + "|n:" + String(nightMode_ ? 1 : 0);
  if (!initialized_ || renderKey == lastRenderKey_) {
    return;
  }

  lastRenderKey_ = renderKey;

  const int scale = 1;
  const int virtualWidth = kDisplayWidth;
  const int virtualHeight = kDisplayHeight;
  const int tinyHeight = kTinyGlyphHeight * kTinyScale;
  const int titleY = 14;
  const int line2Y = std::max(titleY + tinyHeight + 1, virtualHeight - tinyHeight - 12);
  const int line1Y = std::max(titleY + tinyHeight + 1, line2Y - tinyHeight - 8);
  const int textTop = titleY + tinyHeight + 12;
  const int textBottom = std::max(textTop + 1, line1Y - 14);
  const int maxLabelWidth = virtualWidth - 24;

  clearVirtualBuffer(virtualWidth, virtualHeight);
  drawTinyTextCentered(fitTinyText(title, maxLabelWidth, kTinyScale), titleY, wordColor(),
                       kTinyScale);

  if (fontSizeLevel == 1) {
    const int textHeight = kEmbeddedSerif70Height;
    int textY = (textTop + textBottom - textHeight) / 2;
    textY = std::max(textTop, std::min(textY, textBottom - textHeight));
    const int focusIndex = findFocusLetterIndex(word);
    const int currentX = rsvpStartX70(word, focusIndex, virtualWidth, false);
    const int anchorX = (virtualWidth * currentAnchorPercent()) / 100;
    const TextLayoutMetrics currentLayout = serif70WordLayout(word, focusIndex);
    const uint16_t phantomColor = blendOverBackground(wordColor(), kPhantomAlphaMedium);

    drawRsvpAnchorGuide(anchorX, textY, textHeight);
    if (!beforeText.isEmpty()) {
      const TextLayoutMetrics beforeLayout = serif70WordLayout(beforeText, -1);
      const int beforeX =
          currentX + currentLayout.minX - kPhantomCurrentGapMedium - beforeLayout.maxX;
      drawSerif70TextAt(beforeText, beforeX, textY, phantomColor);
    }
    drawRsvp70WordAt(word, currentX, textY, focusIndex);
    if (!afterText.isEmpty()) {
      const TextLayoutMetrics afterLayout = serif70WordLayout(afterText, -1);
      const int afterX =
          currentX + currentLayout.maxX + kPhantomCurrentGapMedium - afterLayout.minX;
      drawSerif70TextAt(afterText, afterX, textY, phantomColor);
    }
  } else {
    const ReaderTextStyle style = readerTextStyle(fontSizeLevel);
    const int textHeight = scaledPercentDimension(kBaseGlyphHeight, style.scalePercent);
    int textY = (textTop + textBottom - textHeight) / 2;
    textY = std::max(textTop, std::min(textY, textBottom - textHeight));
    const int focusIndex = findFocusLetterIndex(word);
    const int currentX =
        rsvpStartXScaledPercent(word, focusIndex, virtualWidth, style.scalePercent, false);
    const int anchorX = (virtualWidth * currentAnchorPercent()) / 100;
    const TextLayoutMetrics currentLayout =
        serifWordLayoutScaledPercent(word, focusIndex, style.scalePercent);
    const uint16_t phantomColor = blendOverBackground(wordColor(), style.alpha);

    drawRsvpAnchorGuide(anchorX, textY, textHeight);
    if (!beforeText.isEmpty()) {
      const TextLayoutMetrics beforeLayout =
          serifWordLayoutScaledPercent(beforeText, -1, style.scalePercent);
      const int beforeX = currentX + currentLayout.minX - style.currentGap - beforeLayout.maxX;
      drawSerifTextScaledAt(beforeText, beforeX, textY, phantomColor, style.scalePercent);
    }
    drawRsvpWordScaledPercentAt(word, currentX, textY, focusIndex, style.scalePercent);
    if (!afterText.isEmpty()) {
      const TextLayoutMetrics afterLayout =
          serifWordLayoutScaledPercent(afterText, -1, style.scalePercent);
      const int afterX = currentX + currentLayout.maxX + style.currentGap - afterLayout.minX;
      drawSerifTextScaledAt(afterText, afterX, textY, phantomColor, style.scalePercent);
    }
  }

  if (!line1.isEmpty()) {
    drawTinyTextCentered(fitTinyText(line1, maxLabelWidth, kTinyScale), line1Y, focusColor(),
                         kTinyScale);
  }
  if (!line2.isEmpty()) {
    drawTinyTextCentered(fitTinyText(line2, maxLabelWidth, kTinyScale), line2Y, dimColor(),
                         kTinyScale);
  }
  drawBatteryBadge();
  flushScaledFrame(scale, virtualWidth, virtualHeight);
}

void DisplayManager::renderPhantomRsvpWordWithWpm(const String &beforeText, const String &word,
                                                  const String &afterText, uint8_t fontSizeLevel,
                                                  uint16_t wpm, const String &chapterLabel,
                                                  uint8_t progressPercent, bool showFooter) {
  const String wpmText = String(wpm) + " WPM";
  const String renderKey =
      "rsvp_phantom_wpm|" + beforeText + "|" + word + "|" + afterText + "|s:" +
      String(fontSizeLevel) + "|" + wpmText + "|" + chapterLabel + "|" +
      String(progressPercent) + "|" + String(showFooter ? 1 : 0) + "|b:" + batteryLabel_ +
      "|d:" + String(darkMode_ ? 1 : 0) + "|n:" + String(nightMode_ ? 1 : 0);
  if (!initialized_ || renderKey == lastRenderKey_) {
    return;
  }

  lastRenderKey_ = renderKey;

  if (fontSizeLevel == 1) {
    const int scale = 1;
    const int virtualWidth = kDisplayWidth;
    const int virtualHeight = kDisplayHeight;
    const int textY = std::max(0, (virtualHeight - kEmbeddedSerif70Height) / 2);
    const int wpmY =
        std::max(0, virtualHeight - kTinyGlyphHeight * kTinyScale - kWpmFeedbackBottomMargin - 24);
    const int focusIndex = findFocusLetterIndex(word);
    const int currentX = rsvpStartX70(word, focusIndex, virtualWidth, false);
    const int anchorX = (virtualWidth * currentAnchorPercent()) / 100;
    const TextLayoutMetrics currentLayout = serif70WordLayout(word, focusIndex);
    const uint16_t phantomColor = blendOverBackground(wordColor(), kPhantomAlphaMedium);

    clearVirtualBuffer(virtualWidth, virtualHeight);
    drawRsvpAnchorGuide(anchorX, textY, kEmbeddedSerif70Height);
    if (!beforeText.isEmpty()) {
      const TextLayoutMetrics beforeLayout = serif70WordLayout(beforeText, -1);
      const int beforeX =
          currentX + currentLayout.minX - kPhantomCurrentGapMedium - beforeLayout.maxX;
      drawSerif70TextAt(beforeText, beforeX, textY, phantomColor);
    }
    drawRsvp70WordAt(word, currentX, textY, focusIndex);
    if (!afterText.isEmpty()) {
      const TextLayoutMetrics afterLayout = serif70WordLayout(afterText, -1);
      const int afterX =
          currentX + currentLayout.maxX + kPhantomCurrentGapMedium - afterLayout.minX;
      drawSerif70TextAt(afterText, afterX, textY, phantomColor);
    }
    drawTinyTextCentered(wpmText, wpmY, focusColor(), kTinyScale);
    if (showFooter) {
      drawFooter(chapterLabel, progressPercent);
    }
    drawBatteryBadge();
    flushScaledFrame(scale, virtualWidth, virtualHeight);
    return;
  }

  const ReaderTextStyle style = readerTextStyle(fontSizeLevel);
  const int scale = 1;
  const int virtualWidth = kDisplayWidth;
  const int virtualHeight = kDisplayHeight;
  const int textHeight = scaledPercentDimension(kBaseGlyphHeight, style.scalePercent);
  const int textY = std::max(0, (virtualHeight - textHeight) / 2);
  const int wpmY =
      std::max(0, virtualHeight - kTinyGlyphHeight * kTinyScale - kWpmFeedbackBottomMargin - 24);
  const int focusIndex = findFocusLetterIndex(word);
  const int currentX =
      rsvpStartXScaledPercent(word, focusIndex, virtualWidth, style.scalePercent, false);
  const int anchorX = (virtualWidth * currentAnchorPercent()) / 100;
  const TextLayoutMetrics currentLayout =
      serifWordLayoutScaledPercent(word, focusIndex, style.scalePercent);
  const uint16_t phantomColor = blendOverBackground(wordColor(), style.alpha);

  clearVirtualBuffer(virtualWidth, virtualHeight);
  drawRsvpAnchorGuide(anchorX, textY, textHeight);
  if (!beforeText.isEmpty()) {
    const TextLayoutMetrics beforeLayout =
        serifWordLayoutScaledPercent(beforeText, -1, style.scalePercent);
    const int beforeX = currentX + currentLayout.minX - style.currentGap - beforeLayout.maxX;
    drawSerifTextScaledAt(beforeText, beforeX, textY, phantomColor, style.scalePercent);
  }
  drawRsvpWordScaledPercentAt(word, currentX, textY, focusIndex, style.scalePercent);
  if (!afterText.isEmpty()) {
    const TextLayoutMetrics afterLayout =
        serifWordLayoutScaledPercent(afterText, -1, style.scalePercent);
    const int afterX = currentX + currentLayout.maxX + style.currentGap - afterLayout.minX;
    drawSerifTextScaledAt(afterText, afterX, textY, phantomColor, style.scalePercent);
  }
  drawTinyTextCentered(wpmText, wpmY, focusColor(), kTinyScale);
  if (showFooter) {
    drawFooter(chapterLabel, progressPercent);
  }
  drawBatteryBadge();
  flushScaledFrame(scale, virtualWidth, virtualHeight);
}

void DisplayManager::renderContextView(const std::vector<ContextWord> &words,
                                       const String &chapterLabel, uint8_t progressPercent) {
  if (words.empty()) {
    renderRsvpWord("", chapterLabel, progressPercent, true);
    return;
  }

  String renderKey = "context|" + chapterLabel + "|" + String(progressPercent);
  renderKey += "|b:";
  renderKey += batteryLabel_;
  renderKey += "|d:";
  renderKey += String(darkMode_ ? 1 : 0);
  renderKey += "|n:";
  renderKey += String(nightMode_ ? 1 : 0);
  for (const ContextWord &word : words) {
    renderKey += "|";
    renderKey += word.current ? "*" : "";
    renderKey += word.paragraphStart ? ">" : "";
    renderKey += word.text;
  }

  if (!initialized_ || renderKey == lastRenderKey_) {
    return;
  }

  lastRenderKey_ = renderKey;

  struct ContextLine {
    size_t start = 0;
    size_t end = 0;
    bool paragraphStart = false;
    bool containsCurrent = false;
  };

  const int scale = 1;
  const int virtualWidth = kDisplayWidth;
  const int virtualHeight = kDisplayHeight;
  const int textBottom =
      virtualHeight - kTinyGlyphHeight * kTinyScale - kFooterMarginBottom - 6;
  const int contextGlyphHeight =
      std::max(1, (kBaseGlyphHeight + kContextSerifDivisor - 1) / kContextSerifDivisor);
  const int maxLineWidth = virtualWidth - (kContextMarginX * 2);
  std::vector<ContextLine> lines;
  lines.reserve(16);

  size_t index = 0;
  int currentLine = 0;
  bool foundCurrentLine = false;
  while (index < words.size()) {
    ContextLine line;
    line.start = index;
    line.paragraphStart = words[index].paragraphStart;
    int lineWidth = line.paragraphStart ? kContextParagraphIndent : 0;

    while (index < words.size()) {
      if (index > line.start && words[index].paragraphStart) {
        break;
      }

      const int wordWidth = measureSerifTextWidth(words[index].text, kContextSerifDivisor);
      const int gap = (index == line.start) ? 0 : kContextSpaceWidth;
      if (index > line.start && lineWidth + gap + wordWidth > maxLineWidth) {
        break;
      }

      lineWidth += gap + wordWidth;
      line.containsCurrent = line.containsCurrent || words[index].current;
      ++index;

      if (lineWidth >= maxLineWidth) {
        break;
      }
    }

    line.end = std::max(line.start + 1, index);
    if (line.end > words.size()) {
      line.end = words.size();
    }
    if (line.containsCurrent && !foundCurrentLine) {
      currentLine = static_cast<int>(lines.size());
      foundCurrentLine = true;
    }
    lines.push_back(line);

    if (line.end == line.start) {
      ++index;
    }
  }

  size_t firstLine = 0;
  if (currentLine > 2) {
    firstLine = static_cast<size_t>(currentLine - 2);
  }
  if (firstLine + kContextTargetLines > lines.size() && lines.size() > kContextTargetLines) {
    firstLine = lines.size() - kContextTargetLines;
  }

  clearVirtualBuffer(virtualWidth, virtualHeight);

  int y = kContextTop;
  for (size_t lineIndex = firstLine; lineIndex < lines.size(); ++lineIndex) {
    const ContextLine &line = lines[lineIndex];
    if (lineIndex != firstLine && line.paragraphStart) {
      y += kContextParagraphGap;
    }
    if (y + contextGlyphHeight > textBottom) {
      break;
    }

    int x = kContextMarginX + (line.paragraphStart ? kContextParagraphIndent : 0);
    for (size_t wordIndex = line.start; wordIndex < line.end && wordIndex < words.size();
         ++wordIndex) {
      const ContextWord &word = words[wordIndex];
      const uint16_t color = word.current ? focusColor() : wordColor();
      const String visibleWord = fitSerifText(word.text, virtualWidth - x - kContextMarginX,
                                              kContextSerifDivisor);
      drawSerifTextAt(visibleWord, x, y, color, kContextSerifDivisor);
      x += measureSerifTextWidth(visibleWord, kContextSerifDivisor) + kContextSpaceWidth;
    }

    y += kContextLineHeight;
  }

  drawFooter(chapterLabel, progressPercent);
  drawBatteryBadge();
  flushScaledFrame(scale, virtualWidth, virtualHeight);
}

void DisplayManager::renderMenu(const char *const *items, size_t itemCount, size_t selectedIndex) {
  if (items == nullptr || itemCount == 0) {
    renderCenteredWord("MENU");
    return;
  }

  std::vector<String> menuItems;
  menuItems.reserve(itemCount);
  for (size_t i = 0; i < itemCount; ++i) {
    menuItems.push_back(items[i] == nullptr ? "" : items[i]);
  }

  renderMenu(menuItems, selectedIndex);
}

void DisplayManager::renderMenu(const std::vector<String> &items, size_t selectedIndex) {
  if (items.empty()) {
    renderCenteredWord("MENU");
    return;
  }

  if (selectedIndex >= items.size()) {
    selectedIndex = items.size() - 1;
  }

  String renderKey = "menuv|";
  renderKey += String(selectedIndex);
  renderKey += "|b:";
  renderKey += batteryLabel_;
  renderKey += "|d:";
  renderKey += String(darkMode_ ? 1 : 0);
  renderKey += "|n:";
  renderKey += String(nightMode_ ? 1 : 0);
  for (const String &item : items) {
    renderKey += "|";
    renderKey += item;
  }

  if (!initialized_ || renderKey == lastRenderKey_) {
    return;
  }

  lastRenderKey_ = renderKey;

  const int scale = 1;
  const int virtualWidth = kDisplayWidth;
  const int virtualHeight = kDisplayHeight;
  const size_t itemCount = items.size();
  const size_t visibleCount =
      std::min(itemCount, static_cast<size_t>(std::max(1, virtualHeight / kCompactMenuRowHeight)));
  size_t firstVisible = 0;
  if (selectedIndex >= visibleCount / 2) {
    firstVisible = selectedIndex - visibleCount / 2;
  }
  if (firstVisible + visibleCount > itemCount) {
    firstVisible = itemCount - visibleCount;
  }

  const int rowHeight = kCompactMenuRowHeight;
  const int totalHeight = rowHeight * static_cast<int>(visibleCount);
  int y = std::max(0, (virtualHeight - totalHeight) / 2);

  clearVirtualBuffer(virtualWidth, virtualHeight);

  for (size_t row = 0; row < visibleCount; ++row) {
    const size_t itemIndex = firstVisible + row;
    const bool selected = itemIndex == selectedIndex;
    const uint16_t color = selected ? focusColor() : dimColor();
    const int maxWidth = virtualWidth - kCompactMenuX - 16;
    if (selected) {
      fillVirtualRect(10, y + 2, 5, kTinyGlyphHeight * kTinyScale + 2, selectedBarColor());
    }
    drawTinyTextAt(fitTinyText(items[itemIndex], maxWidth, kTinyScale), kCompactMenuX, y + 3, color,
                   kTinyScale);
    y += rowHeight;
  }

  drawBatteryBadge();
  flushScaledFrame(scale, virtualWidth, virtualHeight);
}

void DisplayManager::renderLibrary(const std::vector<LibraryItem> &items, size_t selectedIndex) {
  if (items.empty()) {
    renderCenteredWord("LIBRARY");
    return;
  }

  if (selectedIndex >= items.size()) {
    selectedIndex = items.size() - 1;
  }

  String renderKey = "library|";
  renderKey += String(selectedIndex);
  renderKey += "|b:";
  renderKey += batteryLabel_;
  renderKey += "|d:";
  renderKey += String(darkMode_ ? 1 : 0);
  renderKey += "|n:";
  renderKey += String(nightMode_ ? 1 : 0);
  for (const LibraryItem &item : items) {
    renderKey += "|";
    renderKey += item.title;
    renderKey += "~";
    renderKey += item.subtitle;
  }

  if (!initialized_ || renderKey == lastRenderKey_) {
    return;
  }

  lastRenderKey_ = renderKey;

  const int scale = 1;
  const int virtualWidth = kDisplayWidth;
  const int virtualHeight = kDisplayHeight;
  const size_t itemCount = items.size();
  const int usableHeight = std::max(kLibraryRowHeight, virtualHeight - (2 * kLibraryScreenPaddingY));
  const size_t visibleCount =
      std::min(itemCount, static_cast<size_t>(std::max(1, usableHeight / kLibraryRowHeight)));
  size_t firstVisible = 0;
  if (selectedIndex >= visibleCount / 2) {
    firstVisible = selectedIndex - visibleCount / 2;
  }
  if (firstVisible + visibleCount > itemCount) {
    firstVisible = itemCount - visibleCount;
  }

  const int totalHeight = kLibraryRowHeight * static_cast<int>(visibleCount);
  int y = std::max(kLibraryScreenPaddingY, (virtualHeight - totalHeight) / 2);

  clearVirtualBuffer(virtualWidth, virtualHeight);

  for (size_t row = 0; row < visibleCount; ++row) {
    const size_t itemIndex = firstVisible + row;
    const LibraryItem &item = items[itemIndex];
    const bool selected = itemIndex == selectedIndex;
    const uint16_t titleColor = selected ? focusColor() : wordColor();
    const uint16_t subtitleColor = blendOverBackground(titleColor, kLibrarySubtitleAlpha);
    const int maxWidth = virtualWidth - kLibraryInsetX - 16;
    const int rowY = y + static_cast<int>(row) * kLibraryRowHeight;

    if (selected) {
      fillVirtualRect(10, rowY + 3, 5, kLibraryRowHeight - 6, selectedBarColor());
    }

    const String title = fitTinyText(item.title, maxWidth, kTinyScale);
    if (item.subtitle.isEmpty()) {
      drawTinyTextAt(title, kLibraryInsetX, rowY + 12, titleColor, kTinyScale);
      continue;
    }

    drawTinyTextAt(title, kLibraryInsetX, rowY + kLibraryTitleYOffset, titleColor, kTinyScale);
    drawTinyTextAt(fitTinyText(item.subtitle, maxWidth, kTinyScale), kLibraryInsetX,
                   rowY + kLibrarySubtitleYOffset, subtitleColor, kTinyScale);
  }

  drawBatteryBadge();
  flushScaledFrame(scale, virtualWidth, virtualHeight);
}

void DisplayManager::renderStatus(const String &title, const String &line1, const String &line2) {
  const String renderKey = "status|" + title + "|" + line1 + "|" + line2 + "|b:" +
                           batteryLabel_ + "|d:" + String(darkMode_ ? 1 : 0) + "|n:" +
                           String(nightMode_ ? 1 : 0);
  if (!initialized_ || renderKey == lastRenderKey_) {
    return;
  }

  lastRenderKey_ = renderKey;

  const int scale = 1;
  const int virtualWidth = kDisplayWidth;
  const int virtualHeight = kDisplayHeight;
  const int titleY = std::max(0, (virtualHeight - kBaseGlyphHeight) / 2 - 26);
  const int line1Y = std::min(virtualHeight - kTinyGlyphHeight * kTinyScale,
                              titleY + kBaseGlyphHeight + 22);
  const int line2Y = std::min(virtualHeight - kTinyGlyphHeight * kTinyScale,
                              line1Y + kTinyGlyphHeight * kTinyScale + 10);

  clearVirtualBuffer(virtualWidth, virtualHeight);
  drawWordLine(title, titleY, wordColor());
  if (!line1.isEmpty()) {
    drawTinyTextCentered(line1, line1Y, dimColor(), kTinyScale);
  }
  if (!line2.isEmpty()) {
    drawTinyTextCentered(line2, line2Y, focusColor(), kTinyScale);
  }
  drawBatteryBadge();

  flushScaledFrame(scale, virtualWidth, virtualHeight);
}

void DisplayManager::renderProgress(const String &title, const String &line1, const String &line2,
                                    int progressPercent) {
  progressPercent = std::max(-1, std::min(100, progressPercent));
  const String renderKey =
      "progress|" + title + "|" + line1 + "|" + line2 + "|" + String(progressPercent) +
      "|b:" + batteryLabel_ + "|d:" + String(darkMode_ ? 1 : 0) + "|n:" +
      String(nightMode_ ? 1 : 0);
  if (!initialized_ || renderKey == lastRenderKey_) {
    return;
  }

  lastRenderKey_ = renderKey;

  const int scale = 1;
  const int virtualWidth = kDisplayWidth;
  const int virtualHeight = kDisplayHeight;
  const int titleY = std::max(0, (virtualHeight - kBaseGlyphHeight) / 2 - 34);
  const int line1Y = std::min(virtualHeight - kTinyGlyphHeight * kTinyScale,
                              titleY + kBaseGlyphHeight + 18);
  const int line2Y = std::min(virtualHeight - kTinyGlyphHeight * kTinyScale,
                              line1Y + kTinyGlyphHeight * kTinyScale + 10);
  const int barWidth = std::min(300, virtualWidth - 48);
  const int barHeight = 8;
  const int barX = std::max(0, (virtualWidth - barWidth) / 2);
  const int barY = std::min(virtualHeight - barHeight - 8,
                            line2Y + kTinyGlyphHeight * kTinyScale + 14);

  clearVirtualBuffer(virtualWidth, virtualHeight);
  drawWordLine(title, titleY, wordColor());
  if (!line1.isEmpty()) {
    drawTinyTextCentered(line1, line1Y, dimColor(), kTinyScale);
  }
  if (!line2.isEmpty()) {
    drawTinyTextCentered(line2, line2Y, focusColor(), kTinyScale);
  }

  if (progressPercent >= 0) {
    fillVirtualRect(barX, barY, barWidth, barHeight, dimColor());
    fillVirtualRect(barX + 1, barY + 1, barWidth - 2, barHeight - 2, backgroundColor());
    const int fillWidth = std::max(1, ((barWidth - 2) * progressPercent) / 100);
    fillVirtualRect(barX + 1, barY + 1, fillWidth, barHeight - 2, focusColor());
  }

  drawBatteryBadge();
  flushScaledFrame(scale, virtualWidth, virtualHeight);
}
