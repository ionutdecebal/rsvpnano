#include "display/DisplayManager.h"

#include <algorithm>
#include <cctype>
#include <cstring>

#include <esp_heap_caps.h>
#include <esp_log.h>

#include "board/BoardConfig.h"
#include "display/EmbeddedAtkinsonFont.h"
#include "display/EmbeddedAtkinsonFont70.h"
#include "display/EmbeddedOpenDyslexicFont.h"
#include "display/EmbeddedOpenDyslexicFont70.h"
#include "display/EmbeddedSerifFont.h"
#include "display/EmbeddedSerifFont70.h"
#include "display/axs15231b.h"
#include "text/LatinText.h"

namespace {
constexpr int kDisplayWidth = BoardConfig::DISPLAY_WIDTH;
constexpr int kDisplayHeight = BoardConfig::DISPLAY_HEIGHT;
constexpr int kPanelNativeWidth = BoardConfig::PANEL_NATIVE_WIDTH;
constexpr int kPanelNativeHeight = BoardConfig::PANEL_NATIVE_HEIGHT;

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
constexpr int kScrollMarginX = 18;
constexpr int kScrollTop = 8;
constexpr int kScrollLineHeight = 29;
constexpr int kScrollParagraphGap = 8;
constexpr int kScrollParagraphIndent = 22;
constexpr int kScrollSpaceWidth = 10;
constexpr int kScrollSerifDivisor = 2;
constexpr int kWordTickerGapLarge = 16;
constexpr int kWordTickerGapMedium = 12;
constexpr int kWordTickerGapSmall = 9;
constexpr int kWordTickerBandPadding = 10;
constexpr int kPhantomCurrentGapLarge = 30;
constexpr int kPhantomCurrentGapMedium = 24;
constexpr int kPhantomCurrentGapSmall = 20;
constexpr uint8_t kPhantomAlphaLarge = 54;
constexpr uint8_t kPhantomAlphaMedium = 62;
constexpr uint8_t kPhantomAlphaSmall = 72;
constexpr int kTypographyTrackingMin = -2;
constexpr int kTypographyTrackingMax = 3;
constexpr int kTypographyAnchorMin = 30;
constexpr int kTypographyAnchorMax = 60;
constexpr int kTypographyGuideHalfWidthMin = 12;
constexpr int kTypographyGuideHalfWidthMax = 30;
constexpr int kTypographyGuideGapMin = 2;
constexpr int kTypographyGuideGapMax = 8;
constexpr int kOpticalLetterGapPx = 2;

constexpr int kVirtualBufferWidth = kDisplayWidth;
constexpr int kVirtualBufferHeight = kPanelNativeHeight;

constexpr size_t kBytesPerPixel = sizeof(uint16_t);
constexpr size_t kMaxChunkBytes = 16 * 1024;
constexpr int kTxBufferWidth = kDisplayWidth > kPanelNativeWidth ? kDisplayWidth : kPanelNativeWidth;
constexpr int kMaxChunkPhysicalRows = kMaxChunkBytes / (kTxBufferWidth * kBytesPerPixel);
static_assert(kMaxChunkPhysicalRows > 0, "Display chunk buffer must hold at least one row");

constexpr size_t kTxBufferPixels = static_cast<size_t>(kTxBufferWidth) * kMaxChunkPhysicalRows;

int logicalWidthForOrientation(BoardConfig::UiOrientation orientation) {
  switch (orientation) {
    case BoardConfig::UiOrientation::Portrait:
    case BoardConfig::UiOrientation::PortraitFlipped:
      return kPanelNativeWidth;
    case BoardConfig::UiOrientation::Landscape:
    case BoardConfig::UiOrientation::LandscapeFlipped:
    default:
      return kDisplayWidth;
  }
}

int logicalHeightForOrientation(BoardConfig::UiOrientation orientation) {
  switch (orientation) {
    case BoardConfig::UiOrientation::Portrait:
    case BoardConfig::UiOrientation::PortraitFlipped:
      return kPanelNativeHeight;
    case BoardConfig::UiOrientation::Landscape:
    case BoardConfig::UiOrientation::LandscapeFlipped:
    default:
      return kDisplayHeight;
  }
}

bool isPortraitOrientation(BoardConfig::UiOrientation orientation) {
  return orientation == BoardConfig::UiOrientation::Portrait ||
         orientation == BoardConfig::UiOrientation::PortraitFlipped;
}

void mapPhysicalToLogical(BoardConfig::UiOrientation orientation, int physicalX, int physicalY,
                          int &logicalX, int &logicalY) {
  switch (orientation) {
    case BoardConfig::UiOrientation::Portrait:
      logicalX = physicalX;
      logicalY = physicalY;
      break;
    case BoardConfig::UiOrientation::PortraitFlipped:
      logicalX = kPanelNativeWidth - 1 - physicalX;
      logicalY = kPanelNativeHeight - 1 - physicalY;
      break;
    case BoardConfig::UiOrientation::Landscape:
      logicalX = kDisplayWidth - 1 - physicalY;
      logicalY = physicalX;
      break;
    case BoardConfig::UiOrientation::LandscapeFlipped:
    default:
      logicalX = physicalY;
      logicalY = kDisplayHeight - 1 - physicalX;
      break;
  }
}

uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
  return static_cast<uint16_t>(((r & 0xF8U) << 8) | ((g & 0xFCU) << 3) | (b >> 3));
}

struct TinyGlyph {
  char c;
  uint8_t rows[kTinyGlyphHeight];
};

struct ReaderGlyph {
  ReaderGlyph() = default;
  ReaderGlyph(const uint8_t *bitmapPtr, int xOffsetValue, int widthValue, int xAdvanceValue,
              int heightValue)
      : bitmap(bitmapPtr),
        xOffset(xOffsetValue),
        width(widthValue),
        xAdvance(xAdvanceValue),
        height(heightValue) {}

  const uint8_t *bitmap = nullptr;
  int xOffset = 0;
  int width = 0;
  int xAdvance = 0;
  int height = 0;
};

DisplayManager::TypographyConfig &activeTypographyConfig() {
  static DisplayManager::TypographyConfig config;
  return config;
}

DisplayManager::ReaderTypeface sanitizeReaderTypeface(DisplayManager::ReaderTypeface typeface) {
  switch (typeface) {
    case DisplayManager::ReaderTypeface::Standard:
    case DisplayManager::ReaderTypeface::OpenDyslexic:
    case DisplayManager::ReaderTypeface::AtkinsonHyperlegible:
      return typeface;
  }
  return DisplayManager::ReaderTypeface::Standard;
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

bool currentFocusHighlightEnabled() {
  return activeTypographyConfig().focusHighlight;
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

DisplayManager::ReaderTypeface currentReaderTypeface() {
  return sanitizeReaderTypeface(activeTypographyConfig().typeface);
}

DisplayManager::ReaderTypeface effectiveReaderTypefaceForText(const String &) {
  return currentReaderTypeface();
}

int baseGlyphHeightForTypeface(DisplayManager::ReaderTypeface typeface) {
  switch (typeface) {
    case DisplayManager::ReaderTypeface::OpenDyslexic:
      return kEmbeddedOpenDyslexicHeight;
    case DisplayManager::ReaderTypeface::AtkinsonHyperlegible:
      return kEmbeddedAtkinsonHeight;
    case DisplayManager::ReaderTypeface::Standard:
    default:
      return kEmbeddedSerifHeight;
  }
}

int baseGlyphHeight() {
  return baseGlyphHeightForTypeface(currentReaderTypeface());
}

int mediumGlyphHeightForTypeface(DisplayManager::ReaderTypeface typeface) {
  switch (typeface) {
    case DisplayManager::ReaderTypeface::OpenDyslexic:
      return kEmbeddedOpenDyslexic70Height;
    case DisplayManager::ReaderTypeface::AtkinsonHyperlegible:
      return kEmbeddedAtkinson70Height;
    case DisplayManager::ReaderTypeface::Standard:
    default:
      return kEmbeddedSerif70Height;
  }
}

int mediumGlyphHeight() {
  return mediumGlyphHeightForTypeface(currentReaderTypeface());
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
    {'<', {0x01, 0x02, 0x04, 0x08, 0x04, 0x02, 0x01}},
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

ReaderGlyph serifGlyphForByte(uint8_t value) {
  if (value < kEmbeddedSerifFirstChar || value > kEmbeddedSerifLastChar) {
    value = static_cast<uint8_t>('?');
  }
  const EmbeddedSerifGlyph &glyph = kEmbeddedSerifGlyphs[value - kEmbeddedSerifFirstChar];
  return {kEmbeddedSerifBitmaps + glyph.bitmapOffset, glyph.xOffset, glyph.width, glyph.xAdvance,
          kEmbeddedSerifHeight};
}

ReaderGlyph serif70GlyphForByte(uint8_t value) {
  if (value < kEmbeddedSerif70FirstChar || value > kEmbeddedSerif70LastChar) {
    value = static_cast<uint8_t>('?');
  }
  const EmbeddedSerif70Glyph &glyph = kEmbeddedSerif70Glyphs[value - kEmbeddedSerif70FirstChar];
  return {kEmbeddedSerif70Bitmaps + glyph.bitmapOffset, glyph.xOffset, glyph.width,
          glyph.xAdvance, kEmbeddedSerif70Height};
}

ReaderGlyph glyphFor(char c, DisplayManager::ReaderTypeface typeface) {
  const uint8_t value = LatinText::byteValue(c);

  switch (typeface) {
    case DisplayManager::ReaderTypeface::OpenDyslexic: {
      const uint8_t glyphValue =
          (value >= kEmbeddedOpenDyslexicFirstChar && value <= kEmbeddedOpenDyslexicLastChar)
              ? value
              : LatinText::fallbackAsciiByte(value);
      const EmbeddedOpenDyslexicGlyph &glyph =
          kEmbeddedOpenDyslexicGlyphs[glyphValue - kEmbeddedOpenDyslexicFirstChar];
      return {kEmbeddedOpenDyslexicBitmaps + glyph.bitmapOffset, glyph.xOffset, glyph.width,
              glyph.xAdvance, kEmbeddedOpenDyslexicHeight};
    }
    case DisplayManager::ReaderTypeface::AtkinsonHyperlegible: {
      const uint8_t glyphValue =
          (value >= kEmbeddedAtkinsonFirstChar && value <= kEmbeddedAtkinsonLastChar)
              ? value
              : LatinText::fallbackAsciiByte(value);
      const EmbeddedAtkinsonGlyph &glyph =
          kEmbeddedAtkinsonGlyphs[glyphValue - kEmbeddedAtkinsonFirstChar];
      return {kEmbeddedAtkinsonBitmaps + glyph.bitmapOffset, glyph.xOffset, glyph.width,
              glyph.xAdvance, kEmbeddedAtkinsonHeight};
    }
    case DisplayManager::ReaderTypeface::Standard:
    default:
      return serifGlyphForByte(value);
  }
}

ReaderGlyph glyphFor(char c) { return glyphFor(c, currentReaderTypeface()); }

ReaderGlyph glyph70For(char c, DisplayManager::ReaderTypeface typeface) {
  const uint8_t value = LatinText::byteValue(c);

  switch (typeface) {
    case DisplayManager::ReaderTypeface::OpenDyslexic: {
      const uint8_t glyphValue =
          (value >= kEmbeddedOpenDyslexic70FirstChar && value <= kEmbeddedOpenDyslexic70LastChar)
              ? value
              : LatinText::fallbackAsciiByte(value);
      const EmbeddedOpenDyslexic70Glyph &glyph =
          kEmbeddedOpenDyslexic70Glyphs[glyphValue - kEmbeddedOpenDyslexic70FirstChar];
      return {kEmbeddedOpenDyslexic70Bitmaps + glyph.bitmapOffset, glyph.xOffset, glyph.width,
              glyph.xAdvance, kEmbeddedOpenDyslexic70Height};
    }
    case DisplayManager::ReaderTypeface::AtkinsonHyperlegible: {
      const uint8_t glyphValue =
          (value >= kEmbeddedAtkinson70FirstChar && value <= kEmbeddedAtkinson70LastChar)
              ? value
              : LatinText::fallbackAsciiByte(value);
      const EmbeddedAtkinson70Glyph &glyph =
          kEmbeddedAtkinson70Glyphs[glyphValue - kEmbeddedAtkinson70FirstChar];
      return {kEmbeddedAtkinson70Bitmaps + glyph.bitmapOffset, glyph.xOffset, glyph.width,
              glyph.xAdvance, kEmbeddedAtkinson70Height};
    }
    case DisplayManager::ReaderTypeface::Standard:
    default:
      return serif70GlyphForByte(value);
  }
}

ReaderGlyph glyph70For(char c) { return glyph70For(c, currentReaderTypeface()); }

const uint8_t *tinyRowsFor(char c) {
  uint8_t value = LatinText::byteValue(c);
  for (int pass = 0; pass < 2; ++pass) {
    char lookup = static_cast<char>(value);
    if (lookup >= 'a' && lookup <= 'z') {
      lookup = static_cast<char>(lookup - 'a' + 'A');
    }

    for (const TinyGlyph &glyph : kTinyGlyphs) {
      if (glyph.c == lookup) {
        return glyph.rows;
      }
    }

    const uint8_t fallback = LatinText::fallbackAsciiByte(value);
    if (fallback == value) {
      break;
    }
    value = fallback;
  }

  for (const TinyGlyph &glyph : kTinyGlyphs) {
    if (glyph.c == '?') {
      return glyph.rows;
    }
  }

  return kTinyGlyphs[0].rows;
}

uint16_t panelColor(uint16_t rgb565) {
  return static_cast<uint16_t>((rgb565 << 8) | (rgb565 >> 8));
}

bool isWordCharacter(char c) { return LatinText::isWordCharacter(LatinText::byteValue(c)); }

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
  const DisplayManager::ReaderTypeface typeface = effectiveReaderTypefaceForText(word);

  for (size_t i = 0; i < word.length(); ++i) {
    const ReaderGlyph glyph = glyphFor(word[i], typeface);
    const int xOffset = scaledSignedAdvance(glyph.xOffset, divisor);
    const int width = glyph.width == 0 ? 0 : scaledAdvance(glyph.width, divisor);
    const int advance = scaledAdvance(glyph.xAdvance, divisor);
    const int left = cursorX + xOffset;
    updateTextLayoutBounds(layout, left, width);

    if (trackFocus && static_cast<int>(i) == focusIndex) {
      layout.focusCenterX = width > 0 ? left + (width / 2) : cursorX + (advance / 2);
    }

    int tracked = trackedAdvanceScaled(glyph.xAdvance, divisor, i, word.length());
    if (i + 1 < word.length()) {
      const ReaderGlyph nextGlyph = glyphFor(word[i + 1], typeface);
      tracked -= opticalKerningAdjustment(
          word[i], word[i + 1], xOffset, width, tracked,
          scaledSignedAdvance(nextGlyph.xOffset, divisor), scaledDesiredGap(divisor));
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
  const DisplayManager::ReaderTypeface typeface = effectiveReaderTypefaceForText(word);

  for (size_t i = 0; i < word.length(); ++i) {
    const ReaderGlyph glyph = glyphFor(word[i], typeface);
    const int xOffset = scaledSignedPercent(glyph.xOffset, scalePercent);
    const int width = glyph.width == 0 ? 0 : scaledPercentDimension(glyph.width, scalePercent);
    const int advance = scaledPercentDimension(glyph.xAdvance, scalePercent);
    const int left = cursorX + xOffset;
    updateTextLayoutBounds(layout, left, width);

    if (trackFocus && static_cast<int>(i) == focusIndex) {
      layout.focusCenterX = width > 0 ? left + (width / 2) : cursorX + (advance / 2);
    }

    int tracked = trackedAdvanceScaledPercent(glyph.xAdvance, scalePercent, i, word.length());
    if (i + 1 < word.length()) {
      const ReaderGlyph nextGlyph = glyphFor(word[i + 1], typeface);
      tracked -= opticalKerningAdjustment(
          word[i], word[i + 1], xOffset, width, tracked,
          scaledSignedPercent(nextGlyph.xOffset, scalePercent),
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
  const DisplayManager::ReaderTypeface typeface = effectiveReaderTypefaceForText(word);

  for (size_t i = 0; i < word.length(); ++i) {
    const ReaderGlyph glyph = glyph70For(word[i], typeface);
    const int left = cursorX + glyph.xOffset;
    const int width = glyph.width;
    const int advance = glyph.xAdvance;
    updateTextLayoutBounds(layout, left, width);

    if (trackFocus && static_cast<int>(i) == focusIndex) {
      layout.focusCenterX = width > 0 ? left + (width / 2) : cursorX + (advance / 2);
    }

    int tracked = trackedAdvance(advance, i, word.length());
    if (i + 1 < word.length()) {
      const ReaderGlyph nextGlyph = glyph70For(word[i + 1], typeface);
      tracked -= opticalKerningAdjustment(word[i], word[i + 1], glyph.xOffset, width, tracked,
                                          nextGlyph.xOffset,
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
  tickerPlaybackFrameActive_ = false;
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
  tickerPlaybackFrameActive_ = false;
  lastRenderKey_ = "";
}

void DisplayManager::setNightMode(bool nightMode) {
  if (nightMode_ == nightMode) {
    return;
  }

  nightMode_ = nightMode;
  tickerPlaybackFrameActive_ = false;
  lastRenderKey_ = "";
}

void DisplayManager::setUiOrientation(BoardConfig::UiOrientation orientation) {
  if (uiOrientation_ == orientation) {
    return;
  }

  uiOrientation_ = orientation;
  tickerPlaybackFrameActive_ = false;
  lastRenderKey_ = "";
}

void DisplayManager::setUiRotated180(bool rotated180) {
  setUiOrientation(rotated180 ? BoardConfig::UiOrientation::LandscapeFlipped
                              : BoardConfig::UiOrientation::Landscape);
}

void DisplayManager::setTypographyConfig(const TypographyConfig &config) {
  TypographyConfig next;
  next.typeface = sanitizeReaderTypeface(config.typeface);
  next.focusHighlight = config.focusHighlight;
  next.trackingPx = static_cast<int8_t>(clampTypographyTracking(config.trackingPx));
  next.anchorPercent = static_cast<uint8_t>(clampTypographyAnchorPercent(config.anchorPercent));
  next.guideHalfWidth =
      static_cast<uint8_t>(clampTypographyGuideHalfWidth(config.guideHalfWidth));
  next.guideGap = static_cast<uint8_t>(clampTypographyGuideGap(config.guideGap));

  TypographyConfig &current = activeTypographyConfig();
  if (current.typeface == next.typeface && current.focusHighlight == next.focusHighlight &&
      current.trackingPx == next.trackingPx &&
      current.anchorPercent == next.anchorPercent &&
      current.guideHalfWidth == next.guideHalfWidth && current.guideGap == next.guideGap) {
    return;
  }

  current = next;
  tickerPlaybackFrameActive_ = false;
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
  tickerPlaybackFrameActive_ = false;
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
  axs15231bSleep();
  initialized_ = false;
  tickerPlaybackFrameActive_ = false;
  lastRenderKey_ = "";
}

bool DisplayManager::wakeFromSleep() {
  if (!allocateBuffers()) {
    ESP_LOGE(kDisplayTag, "Buffer allocation failed after wake");
    return false;
  }

  axs15231bWake();
  initialized_ = true;
  tickerPlaybackFrameActive_ = false;
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
  axs15231bInit();
  ESP_LOGI(kDisplayTag, "Panel init sequence complete");
  return true;
}

bool DisplayManager::drawBitmap(int xStart, int yStart, int xEnd, int yEnd, const void *colorData) {
  if (colorData == nullptr || xEnd <= xStart || yEnd <= yStart) {
    return false;
  }

  axs15231bPushColors(static_cast<uint16_t>(xStart), static_cast<uint16_t>(yStart),
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

uint16_t DisplayManager::focusTimerBreakColor() const {
  return nightMode_ ? rgb565(112, 176, 126) : rgb565(68, 132, 88);
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

int DisplayManager::logicalWidth() const { return logicalWidthForOrientation(uiOrientation_); }

int DisplayManager::logicalHeight() const { return logicalHeightForOrientation(uiOrientation_); }

int DisplayManager::chooseTextScale(const String &word) const {
  const int usableWidth = std::max(1, measureTextWidth(word));
  const int maxScaleX = kDisplayWidth / usableWidth;
  const int maxScaleY = kDisplayHeight / baseGlyphHeightForTypeface(effectiveReaderTypefaceForText(word));
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
  while (!fitted.isEmpty() && fitted[fitted.length() - 1] == ' ') {
    fitted.remove(fitted.length() - 1, 1);
  }
  return fitted.isEmpty() ? ellipsis : fitted + ellipsis;
}

String DisplayManager::fitSerifTextScaled(const String &text, int maxWidth,
                                          uint8_t scalePercent) const {
  if (measureSerifTextWidthScaled(text, scalePercent) <= maxWidth) {
    return text;
  }

  String fitted = text;
  const String ellipsis = "...";
  while (!fitted.isEmpty() &&
         measureSerifTextWidthScaled(fitted + ellipsis, scalePercent) > maxWidth) {
    fitted.remove(fitted.length() - 1);
  }
  while (!fitted.isEmpty() && fitted[fitted.length() - 1] == ' ') {
    fitted.remove(fitted.length() - 1, 1);
  }
  return fitted.isEmpty() ? ellipsis : fitted + ellipsis;
}

String DisplayManager::fitSerifTextTrailingScaled(const String &text, int maxWidth,
                                                  uint8_t scalePercent) const {
  if (measureSerifTextWidthScaled(text, scalePercent) <= maxWidth) {
    return text;
  }

  String fitted = text;
  const String ellipsis = "...";
  while (!fitted.isEmpty() &&
         measureSerifTextWidthScaled(ellipsis + fitted, scalePercent) > maxWidth) {
    fitted.remove(0, 1);
  }
  while (!fitted.isEmpty() && fitted[0] == ' ') {
    fitted.remove(0, 1);
  }
  return fitted.isEmpty() ? ellipsis : ellipsis + fitted;
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
  while (!fitted.isEmpty() && fitted[fitted.length() - 1] == ' ') {
    fitted.remove(fitted.length() - 1, 1);
  }
  return fitted.isEmpty() ? ellipsis : fitted + ellipsis;
}

String DisplayManager::fitTinyTextTrailing(const String &text, int maxWidth, int scale) const {
  if (measureTinyTextWidth(text, scale) <= maxWidth) {
    return text;
  }

  String fitted = text;
  const String ellipsis = "...";
  while (!fitted.isEmpty() && measureTinyTextWidth(ellipsis + fitted, scale) > maxWidth) {
    fitted.remove(0, 1);
  }
  while (!fitted.isEmpty() && fitted[0] == ' ') {
    fitted.remove(0, 1);
  }
  return fitted.isEmpty() ? ellipsis : ellipsis + fitted;
}

void DisplayManager::drawGlyph(int x, int y, char c, uint16_t color) {
  drawGlyph(x, y, c, color, currentReaderTypeface());
}

void DisplayManager::drawGlyph(int x, int y, char c, uint16_t color, ReaderTypeface typeface) {
  const ReaderGlyph glyph = glyphFor(c, typeface);
  if (glyph.width == 0) {
    return;
  }

  for (int row = 0; row < glyph.height; ++row) {
    const int dstY = y + row;
    if (dstY < 0 || dstY >= kVirtualBufferHeight) {
      continue;
    }

    for (int col = 0; col < glyph.width; ++col) {
      const int dstX = x + col;
      if (dstX < 0 || dstX >= kVirtualBufferWidth) {
        continue;
      }

      const uint8_t alpha = glyph.bitmap[row * glyph.width + col];
      if (alpha < kGlyphAlphaThreshold) {
        continue;
      }

      virtualFrame_[dstY * kVirtualBufferWidth + dstX] =
          panelColor(blendOverBackground(color, alpha));
    }
  }
}

void DisplayManager::drawSerifGlyphScaled(int x, int y, char c, uint16_t color, int divisor) {
  drawSerifGlyphScaled(x, y, c, color, divisor, currentReaderTypeface());
}

void DisplayManager::drawSerifGlyphScaled(int x, int y, char c, uint16_t color, int divisor,
                                          ReaderTypeface typeface) {
  divisor = std::max(1, divisor);
  const ReaderGlyph glyph = glyphFor(c, typeface);
  if (glyph.width == 0) {
    return;
  }

  const int glyphHeight = glyph.height;
  const int scaledWidth = std::max(1, (glyph.width + divisor - 1) / divisor);
  const int scaledHeight = std::max(1, (glyphHeight + divisor - 1) / divisor);

  for (int dstRow = 0; dstRow < scaledHeight; ++dstRow) {
    const int dstY = y + dstRow;
    if (dstY < 0 || dstY >= kVirtualBufferHeight) {
      continue;
    }

    const int sourceYStart = dstRow * divisor;
    const int sourceYEnd = std::min(glyphHeight, sourceYStart + divisor);
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
          alphaSum += glyph.bitmap[sourceY * glyph.width + sourceX];
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
  drawSerif70Glyph(x, y, c, color, currentReaderTypeface());
}

void DisplayManager::drawSerif70Glyph(int x, int y, char c, uint16_t color, ReaderTypeface typeface) {
  const ReaderGlyph glyph = glyph70For(c, typeface);
  if (glyph.width == 0) {
    return;
  }

  for (int row = 0; row < glyph.height; ++row) {
    const int dstY = y + row;
    if (dstY < 0 || dstY >= kVirtualBufferHeight) {
      continue;
    }

    for (int col = 0; col < glyph.width; ++col) {
      const int dstX = x + col;
      if (dstX < 0 || dstX >= kVirtualBufferWidth) {
        continue;
      }

      const uint8_t alpha = glyph.bitmap[row * glyph.width + col];
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
  drawSerifGlyphScaledPercent(x, y, c, color, scalePercent, currentReaderTypeface());
}

void DisplayManager::drawSerifGlyphScaledPercent(int x, int y, char c, uint16_t color,
                                                 uint8_t scalePercent,
                                                 ReaderTypeface typeface) {
  if (scalePercent >= 100) {
    drawGlyph(x, y, c, color, typeface);
    return;
  }

  const ReaderGlyph glyph = glyphFor(c, typeface);
  if (glyph.width == 0) {
    return;
  }

  const int glyphHeight = glyph.height;
  const int scaledWidth = scaledPercentDimension(glyph.width, scalePercent);
  const int scaledHeight = scaledPercentDimension(glyphHeight, scalePercent);

  for (int dstRow = 0; dstRow < scaledHeight; ++dstRow) {
    const int dstY = y + dstRow;
    if (dstY < 0 || dstY >= kVirtualBufferHeight) {
      continue;
    }

    const int sourceYStart = (dstRow * glyphHeight) / scaledHeight;
    const int sourceYEnd =
        std::min(glyphHeight, ((dstRow + 1) * glyphHeight + scaledHeight - 1) / scaledHeight);
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
          alphaSum += glyph.bitmap[sourceY * glyph.width + sourceX];
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
  const ReaderTypeface typeface = effectiveReaderTypefaceForText(text);
  for (size_t i = 0; i < text.length(); ++i) {
    const ReaderGlyph glyph = glyphFor(text[i], typeface);
    const int xOffset = scaledSignedAdvance(glyph.xOffset, divisor);
    const int width = glyph.width == 0 ? 0 : scaledAdvance(glyph.width, divisor);
    drawSerifGlyphScaled(cursorX + xOffset, y, text[i], color, divisor, typeface);
    int tracked = trackedAdvanceScaled(glyph.xAdvance, divisor, i, text.length());
    if (i + 1 < text.length()) {
      const ReaderGlyph nextGlyph = glyphFor(text[i + 1], typeface);
      tracked -= opticalKerningAdjustment(
          text[i], text[i + 1], xOffset, width, tracked,
          scaledSignedAdvance(nextGlyph.xOffset, divisor), scaledDesiredGap(divisor));
    }
    cursorX += std::max(1, tracked);
  }
}

void DisplayManager::drawSerif70TextAt(const String &text, int x, int y, uint16_t color) {
  int cursorX = x;
  const ReaderTypeface typeface = effectiveReaderTypefaceForText(text);
  for (size_t i = 0; i < text.length(); ++i) {
    const ReaderGlyph glyph = glyph70For(text[i], typeface);
    drawSerif70Glyph(cursorX + glyph.xOffset, y, text[i], color, typeface);
    int tracked = trackedAdvance(glyph.xAdvance, i, text.length());
    if (i + 1 < text.length()) {
      const ReaderGlyph nextGlyph = glyph70For(text[i + 1], typeface);
      tracked -= opticalKerningAdjustment(text[i], text[i + 1], glyph.xOffset, glyph.width, tracked,
                                          nextGlyph.xOffset, regularDesiredGap());
    }
    cursorX += std::max(1, tracked);
  }
}

void DisplayManager::drawSerifTextScaledAt(const String &text, int x, int y, uint16_t color,
                                           uint8_t scalePercent) {
  int cursorX = x;
  const ReaderTypeface typeface = effectiveReaderTypefaceForText(text);
  for (size_t i = 0; i < text.length(); ++i) {
    const ReaderGlyph glyph = glyphFor(text[i], typeface);
    const int xOffset = scaledSignedPercent(glyph.xOffset, scalePercent);
    const int width =
        glyph.width == 0 ? 0 : scaledPercentDimension(glyph.width, scalePercent);
    drawSerifGlyphScaledPercent(cursorX + xOffset, y, text[i], color, scalePercent, typeface);
    int tracked = trackedAdvanceScaledPercent(glyph.xAdvance, scalePercent, i, text.length());
    if (i + 1 < text.length()) {
      const ReaderGlyph nextGlyph = glyphFor(text[i + 1], typeface);
      tracked -= opticalKerningAdjustment(
          text[i], text[i + 1], xOffset, width, tracked,
          scaledSignedPercent(nextGlyph.xOffset, scalePercent),
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

void DisplayManager::drawTinyTextCentered(const String &text, int y, uint16_t color, int scale,
                                          int width, int xOffset) {
  const int textWidth = measureTinyTextWidth(text, scale);
  drawTinyTextAt(text, std::max(xOffset, xOffset + ((width - textWidth) / 2)), y, color, scale);
}

void DisplayManager::drawSerif70TextCentered(const String &text, int y, uint16_t color, int width,
                                             int xOffset) {
  const int textWidth = measureSerif70TextWidth(text);
  drawSerif70TextAt(text, std::max(xOffset, xOffset + ((width - textWidth) / 2)), y, color);
}

void DisplayManager::drawSerifTextScaledCentered(const String &text, int y, uint16_t color,
                                                 uint8_t scalePercent, int width, int xOffset) {
  const int textWidth = measureSerifTextWidthScaled(text, scalePercent);
  drawSerifTextScaledAt(text, std::max(xOffset, xOffset + ((width - textWidth) / 2)), y, color,
                        scalePercent);
}

void DisplayManager::drawBatteryBadge() {
  drawBatteryBadge(kDisplayWidth, kDisplayHeight);
}

void DisplayManager::drawBatteryBadge(int logicalWidth, int logicalHeight) {
  if (batteryLabel_.isEmpty()) {
    return;
  }

  const int width = measureTinyTextWidth(batteryLabel_, kTinyScale);
  const int x = std::max(kFooterMarginX, logicalWidth - kFooterMarginX - width);
  const int y = logicalHeight > (kDisplayHeight * 2) ? kFooterMarginBottom + 8 : kFooterMarginBottom;
  drawTinyTextAt(batteryLabel_, x, y, footerColor(), kTinyScale);
}

void DisplayManager::drawPreviousSentenceHint() {
  drawTinyTextAt("<<", kFooterMarginX, kFooterMarginBottom, footerColor(), kTinyScale);
}

void DisplayManager::drawFooter(const String &chapterLabel, const String &statusLabel) {
  const String status = statusLabel.isEmpty() ? "0%" : statusLabel;
  const int y = kDisplayHeight - kTinyGlyphHeight * kTinyScale - kFooterMarginBottom;
  const int statusWidth = measureTinyTextWidth(status, kTinyScale);
  const int rightX = std::max(kFooterMarginX, kDisplayWidth - kFooterMarginX - statusWidth);
  const int maxChapterWidth = std::max(0, rightX - kFooterMarginX - 18);
  const String chapter = fitTinyText(chapterLabel.isEmpty() ? "START" : chapterLabel,
                                    maxChapterWidth, kTinyScale);

  drawTinyTextAt(chapter, kFooterMarginX, y, footerColor(), kTinyScale);
  drawTinyTextAt(status, rightX, y, footerColor(), kTinyScale);
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
  const uint16_t guideTickColor = currentFocusHighlightEnabled() ? focusColor() : guideColor;

  fillVirtualRect(leftX, topY, leftWidth, 1, guideColor);
  fillVirtualRect(anchorX + guideGap, topY, rightWidth, 1, guideColor);
  fillVirtualRect(leftX, bottomY, leftWidth, 1, guideColor);
  fillVirtualRect(anchorX + guideGap, bottomY, rightWidth, 1, guideColor);
  fillVirtualRect(anchorX, topY, 1, kRsvpGuideTickHeight, guideTickColor);
  fillVirtualRect(anchorX, bottomY - kRsvpGuideTickHeight + 1, 1, kRsvpGuideTickHeight,
                  guideTickColor);
}

void DisplayManager::drawWordAt(const String &word, int x, int y, uint16_t color) {
  int cursorX = x;
  const ReaderTypeface typeface = effectiveReaderTypefaceForText(word);
  for (size_t i = 0; i < word.length(); ++i) {
    const ReaderGlyph glyph = glyphFor(word[i], typeface);
    drawGlyph(cursorX + glyph.xOffset, y, word[i], color, typeface);
    int tracked = trackedAdvance(glyph.xAdvance, i, word.length());
    if (i + 1 < word.length()) {
      const ReaderGlyph nextGlyph = glyphFor(word[i + 1], typeface);
      tracked -= opticalKerningAdjustment(word[i], word[i + 1], glyph.xOffset, glyph.width, tracked,
                                          nextGlyph.xOffset, regularDesiredGap());
    }
    cursorX += std::max(1, tracked);
  }
}

void DisplayManager::drawRsvpWordScaledAt(const String &word, int x, int y, int focusIndex,
                                          int divisor) {
  divisor = std::max(1, divisor);
  const bool highlightFocus = currentFocusHighlightEnabled();
  int cursorX = x;
  const ReaderTypeface typeface = effectiveReaderTypefaceForText(word);
  for (size_t i = 0; i < word.length(); ++i) {
    const ReaderGlyph glyph = glyphFor(word[i], typeface);
    const uint16_t color =
        (highlightFocus && static_cast<int>(i) == focusIndex) ? focusColor() : wordColor();
    const int xOffset = scaledSignedAdvance(glyph.xOffset, divisor);
    const int width = glyph.width == 0 ? 0 : scaledAdvance(glyph.width, divisor);
    drawSerifGlyphScaled(cursorX + xOffset, y, word[i], color, divisor, typeface);
    int tracked = trackedAdvanceScaled(glyph.xAdvance, divisor, i, word.length());
    if (i + 1 < word.length()) {
      const ReaderGlyph nextGlyph = glyphFor(word[i + 1], typeface);
      tracked -= opticalKerningAdjustment(
          word[i], word[i + 1], xOffset, width, tracked,
          scaledSignedAdvance(nextGlyph.xOffset, divisor), scaledDesiredGap(divisor));
    }
    cursorX += std::max(1, tracked);
  }
}

void DisplayManager::drawRsvp70WordAt(const String &word, int x, int y, int focusIndex) {
  const bool highlightFocus = currentFocusHighlightEnabled();
  int cursorX = x;
  const ReaderTypeface typeface = effectiveReaderTypefaceForText(word);
  for (size_t i = 0; i < word.length(); ++i) {
    const ReaderGlyph glyph = glyph70For(word[i], typeface);
    const uint16_t color =
        (highlightFocus && static_cast<int>(i) == focusIndex) ? focusColor() : wordColor();
    drawSerif70Glyph(cursorX + glyph.xOffset, y, word[i], color, typeface);
    int tracked = trackedAdvance(glyph.xAdvance, i, word.length());
    if (i + 1 < word.length()) {
      const ReaderGlyph nextGlyph = glyph70For(word[i + 1], typeface);
      tracked -= opticalKerningAdjustment(word[i], word[i + 1], glyph.xOffset, glyph.width, tracked,
                                          nextGlyph.xOffset, regularDesiredGap());
    }
    cursorX += std::max(1, tracked);
  }
}

void DisplayManager::drawRsvpWordScaledPercentAt(const String &word, int x, int y, int focusIndex,
                                                 uint8_t scalePercent) {
  const bool highlightFocus = currentFocusHighlightEnabled();
  int cursorX = x;
  const ReaderTypeface typeface = effectiveReaderTypefaceForText(word);
  for (size_t i = 0; i < word.length(); ++i) {
    const ReaderGlyph glyph = glyphFor(word[i], typeface);
    const uint16_t color =
        (highlightFocus && static_cast<int>(i) == focusIndex) ? focusColor() : wordColor();
    const int xOffset = scaledSignedPercent(glyph.xOffset, scalePercent);
    const int width =
        glyph.width == 0 ? 0 : scaledPercentDimension(glyph.width, scalePercent);
    drawSerifGlyphScaledPercent(cursorX + xOffset, y, word[i], color, scalePercent, typeface);
    int tracked = trackedAdvanceScaledPercent(glyph.xAdvance, scalePercent, i, word.length());
    if (i + 1 < word.length()) {
      const ReaderGlyph nextGlyph = glyphFor(word[i + 1], typeface);
      tracked -= opticalKerningAdjustment(
          word[i], word[i + 1], xOffset, width, tracked,
          scaledSignedPercent(nextGlyph.xOffset, scalePercent),
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
  axs15231bSetBrightnessPercent(brightnessPercent_);
  axs15231bSetBacklight(true);
}

void DisplayManager::flushScaledFrame(int scale, int virtualWidth, int virtualHeight) {
  tickerPlaybackFrameActive_ = false;
  for (int nativeYStart = 0; nativeYStart < kPanelNativeHeight;
       nativeYStart += kMaxChunkPhysicalRows) {
    const int nativeRows = std::min(kMaxChunkPhysicalRows, kPanelNativeHeight - nativeYStart);
    std::memset(txBuffer_, 0, txBufferBytes_);

    for (int localNativeY = 0; localNativeY < nativeRows; ++localNativeY) {
      const int nativeY = nativeYStart + localNativeY;
      uint16_t *dstRow = txBuffer_ + localNativeY * kPanelNativeWidth;

      for (int nativeX = 0; nativeX < kPanelNativeWidth; ++nativeX) {
        int logicalX = 0;
        int logicalY = 0;
        mapPhysicalToLogical(uiOrientation_, nativeX, nativeY, logicalX, logicalY);
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

void DisplayManager::flushFullWidthLogicalBand(int yStart, int yEnd) {
  if (!initialized_) {
    return;
  }

  if (isPortraitOrientation(uiOrientation_)) {
    flushScaledFrame(1, logicalWidth(), logicalHeight());
    return;
  }

  yStart = std::max(0, std::min(kDisplayHeight, yStart));
  yEnd = std::max(0, std::min(kDisplayHeight, yEnd));
  if (yEnd <= yStart) {
    return;
  }

  const bool flipped = uiOrientation_ == BoardConfig::UiOrientation::LandscapeFlipped;
  const int physicalXStart = flipped ? (kDisplayHeight - yEnd) : yStart;
  const int physicalXEnd = flipped ? (kDisplayHeight - yStart) : yEnd;
  const int physicalWidth = physicalXEnd - physicalXStart;
  if (physicalWidth <= 0 || txBuffer_ == nullptr) {
    return;
  }

  for (int nativeYStart = 0; nativeYStart < kPanelNativeHeight;
       nativeYStart += kMaxChunkPhysicalRows) {
    const int nativeRows = std::min(kMaxChunkPhysicalRows, kPanelNativeHeight - nativeYStart);

    for (int localNativeY = 0; localNativeY < nativeRows; ++localNativeY) {
      const int nativeY = nativeYStart + localNativeY;
      uint16_t *dstRow = txBuffer_ + (localNativeY * physicalWidth);

      for (int localNativeX = 0; localNativeX < physicalWidth; ++localNativeX) {
        const int nativeX = physicalXStart + localNativeX;
        int logicalX = 0;
        int logicalY = 0;
        mapPhysicalToLogical(uiOrientation_, nativeX, nativeY, logicalX, logicalY);
        dstRow[localNativeX] = virtualFrame_[logicalY * kVirtualBufferWidth + logicalX];
      }
    }

    if (!drawBitmap(physicalXStart, nativeYStart, physicalXEnd, nativeYStart + nativeRows,
                    txBuffer_)) {
      return;
    }
  }

  tickerPlaybackFrameActive_ = true;
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
  const int glyphHeight = baseGlyphHeightForTypeface(effectiveReaderTypefaceForText(normalized));

  clearVirtualBuffer(virtualWidth, virtualHeight);
  const int y = std::max(0, (virtualHeight - glyphHeight) / 2);
  drawWordLine(normalized, y, renderColor);
  drawBatteryBadge();

  flushScaledFrame(scale, virtualWidth, virtualHeight);
}

void DisplayManager::renderRsvpWord(const String &word, const String &chapterLabel,
                                    uint8_t progressPercent, bool showFooter,
                                    const String &footerStatusLabel) {
  const String renderKey =
      "rsvp|" + word + "|" + chapterLabel + "|" + String(progressPercent) + "|" +
      String(showFooter ? 1 : 0) + "|f:" + footerStatusLabel + "|b:" + batteryLabel_ +
      "|d:" + String(darkMode_ ? 1 : 0) + "|n:" + String(nightMode_ ? 1 : 0);
  if (!initialized_ || renderKey == lastRenderKey_) {
    return;
  }

  lastRenderKey_ = renderKey;

  const int scale = 1;
  const int virtualWidth = logicalWidth();
  const int virtualHeight = logicalHeight();
  const int glyphHeight = baseGlyphHeightForTypeface(effectiveReaderTypefaceForText(word));
  const int y = std::max(0, (virtualHeight - glyphHeight) / 2);
  const int focusIndex = findFocusLetterIndex(word);
  const int x = rsvpStartX(word, focusIndex, virtualWidth, 1, false);
  const int anchorX = (virtualWidth * currentAnchorPercent()) / 100;

  clearVirtualBuffer(virtualWidth, virtualHeight);
  drawRsvpAnchorGuide(anchorX, y, glyphHeight);
  drawRsvpWordAt(word, x, y, focusIndex);
  if (showFooter) {
    drawFooter(chapterLabel, footerStatusLabel.isEmpty() ? String(progressPercent) + "%"
                                                         : footerStatusLabel);
  }
  drawBatteryBadge(virtualWidth, virtualHeight);
  flushScaledFrame(scale, virtualWidth, virtualHeight);
}

void DisplayManager::renderRsvpWordWithWpm(const String &word, uint16_t wpm,
                                           const String &chapterLabel, uint8_t progressPercent,
                                           bool showFooter, const String &footerStatusLabel) {
  const String wpmText = String(wpm) + " WPM";
  const String renderKey =
      "rsvp_wpm|" + word + "|" + wpmText + "|" + chapterLabel + "|" +
      String(progressPercent) + "|" + String(showFooter ? 1 : 0) + "|f:" + footerStatusLabel +
      "|b:" + batteryLabel_ + "|d:" + String(darkMode_ ? 1 : 0) + "|n:" +
      String(nightMode_ ? 1 : 0);
  if (!initialized_ || renderKey == lastRenderKey_) {
    return;
  }

  lastRenderKey_ = renderKey;

  const int scale = 1;
  const int virtualWidth = kDisplayWidth;
  const int virtualHeight = kDisplayHeight;
  const int glyphHeight = baseGlyphHeightForTypeface(effectiveReaderTypefaceForText(word));
  const int wordY = std::max(0, (virtualHeight - glyphHeight) / 2);
  const int wpmY =
      std::max(0, virtualHeight - kTinyGlyphHeight * kTinyScale - kWpmFeedbackBottomMargin - 24);
  const int focusIndex = findFocusLetterIndex(word);
  const int x = rsvpStartX(word, focusIndex, virtualWidth, 1, false);
  const int anchorX = (virtualWidth * currentAnchorPercent()) / 100;

  clearVirtualBuffer(virtualWidth, virtualHeight);
  drawRsvpAnchorGuide(anchorX, wordY, glyphHeight);
  drawRsvpWordAt(word, x, wordY, focusIndex);
  drawTinyTextCentered(wpmText, wpmY, focusColor(), kTinyScale);
  if (showFooter) {
    drawFooter(chapterLabel, footerStatusLabel.isEmpty() ? String(progressPercent) + "%"
                                                         : footerStatusLabel);
  }
  drawBatteryBadge();
  flushScaledFrame(scale, virtualWidth, virtualHeight);
}

void DisplayManager::renderPhantomRsvpWord(const String &beforeText, const String &word,
                                           const String &afterText, uint8_t fontSizeLevel,
                                           const String &chapterLabel, uint8_t progressPercent,
                                           bool showFooter, const String &footerStatusLabel) {
  const String renderKey =
      "rsvp_phantom|" + beforeText + "|" + word + "|" + afterText + "|s:" +
      String(fontSizeLevel) + "|" + chapterLabel + "|" + String(progressPercent) + "|" +
      String(showFooter ? 1 : 0) + "|f:" + footerStatusLabel + "|b:" + batteryLabel_ +
      "|d:" + String(darkMode_ ? 1 : 0) + "|n:" + String(nightMode_ ? 1 : 0);
  if (!initialized_ || renderKey == lastRenderKey_) {
    return;
  }

  lastRenderKey_ = renderKey;

  if (fontSizeLevel == 1) {
    const int scale = 1;
    const int virtualWidth = kDisplayWidth;
    const int virtualHeight = kDisplayHeight;
    const int mediumHeight = mediumGlyphHeightForTypeface(effectiveReaderTypefaceForText(word));
    const int textY = std::max(0, (virtualHeight - mediumHeight) / 2);
    const int focusIndex = findFocusLetterIndex(word);
    const int currentX = rsvpStartX70(word, focusIndex, virtualWidth, false);
    const int anchorX = (virtualWidth * currentAnchorPercent()) / 100;
    const TextLayoutMetrics currentLayout = serif70WordLayout(word, focusIndex);
    const uint16_t phantomColor = blendOverBackground(wordColor(), kPhantomAlphaMedium);

    clearVirtualBuffer(virtualWidth, virtualHeight);
    drawRsvpAnchorGuide(anchorX, textY, mediumHeight);
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
      drawFooter(chapterLabel, footerStatusLabel.isEmpty() ? String(progressPercent) + "%"
                                                           : footerStatusLabel);
      drawPreviousSentenceHint();
    }
    drawBatteryBadge();
    flushScaledFrame(scale, virtualWidth, virtualHeight);
    return;
  }

  const ReaderTextStyle style = readerTextStyle(fontSizeLevel);
  const int scale = 1;
  const int virtualWidth = kDisplayWidth;
  const int virtualHeight = kDisplayHeight;
  const int textHeight = scaledPercentDimension(
      baseGlyphHeightForTypeface(effectiveReaderTypefaceForText(word)), style.scalePercent);
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
    drawFooter(chapterLabel, footerStatusLabel.isEmpty() ? String(progressPercent) + "%"
                                                         : footerStatusLabel);
    drawPreviousSentenceHint();
  }
  drawBatteryBadge();
  flushScaledFrame(scale, virtualWidth, virtualHeight);
}

void DisplayManager::renderWordTickerView(const std::vector<ContextWord> &words,
                                          size_t currentWordIndex, uint8_t fontSizeLevel,
                                          uint16_t motionPermille, const String &chapterLabel,
                                          uint8_t progressPercent, const String &overlayText,
                                          bool showFooter) {
  if (words.empty()) {
    renderRsvpWord("", chapterLabel, progressPercent, showFooter);
    return;
  }
  if (currentWordIndex >= words.size()) {
    currentWordIndex = words.size() - 1;
  }
  if (motionPermille > 1000) {
    motionPermille = 1000;
  }

  const bool canUseBandOnly = !showFooter && overlayText.isEmpty() && tickerPlaybackFrameActive_;
  String renderKey =
      "ticker|" + String(fontSizeLevel) + "|i:" + String(currentWordIndex) + "|m:" +
      String(motionPermille) + "|f:" + String(showFooter ? 1 : 0) + "|d:" +
      String(darkMode_ ? 1 : 0) + "|n:" + String(nightMode_ ? 1 : 0) + "|wc:" +
      String(words.size());
  if (!canUseBandOnly) {
    renderKey += "|c:";
    renderKey += chapterLabel;
    renderKey += "|p:";
    renderKey += String(progressPercent);
    renderKey += "|o:";
    renderKey += overlayText;
    renderKey += "|b:";
    renderKey += batteryLabel_;
  }
  const size_t keyStart = currentWordIndex > 2 ? currentWordIndex - 2 : 0;
  const size_t keyEnd = std::min(words.size(), currentWordIndex + 3);
  for (size_t index = keyStart; index < keyEnd; ++index) {
    renderKey += "|";
    renderKey += words[index].text;
  }
  if (!initialized_ || renderKey == lastRenderKey_) {
    return;
  }

  lastRenderKey_ = renderKey;

  const int scale = 1;
  const int virtualWidth = kDisplayWidth;
  const int virtualHeight = kDisplayHeight;
  const int overlayY =
      std::max(0, virtualHeight - kTinyGlyphHeight * kTinyScale - kWpmFeedbackBottomMargin - 24);
  const uint16_t textColor = wordColor();

  if (fontSizeLevel == 1) {
    auto layoutFor = [&](size_t index) { return serif70WordLayout(words[index].text, -1); };
    auto widthFor = [&](const TextLayoutMetrics &layout) { return textLayoutWidth(layout); };

    const int gap = kWordTickerGapMedium;
    const int mediumHeight =
        mediumGlyphHeightForTypeface(effectiveReaderTypefaceForText(words[currentWordIndex].text));
    const int textY = std::max(0, (virtualHeight - mediumHeight) / 2);
    const TextLayoutMetrics currentLayout = layoutFor(currentWordIndex);
    const int currentWidth = widthFor(currentLayout);
    const int currentLeftBase = (virtualWidth - currentWidth) / 2;
    int shiftX = 0;
    if (currentWordIndex + 1 < words.size()) {
      const int nextWidth = widthFor(layoutFor(currentWordIndex + 1));
      const int travel = gap + (currentWidth / 2) + (nextWidth / 2);
      shiftX = static_cast<int>((static_cast<int32_t>(travel) * motionPermille) / 1000);
    }

    const int bandTop = std::max(0, textY - kWordTickerBandPadding);
    const int bandBottom =
        std::min(virtualHeight, textY + mediumHeight + kWordTickerBandPadding);
    if (canUseBandOnly) {
      fillVirtualRect(0, bandTop, virtualWidth, bandBottom - bandTop, backgroundColor());
    } else {
      clearVirtualBuffer(virtualWidth, virtualHeight);
    }
    int left = currentLeftBase - shiftX;
    int originX = left - currentLayout.minX;
    drawSerif70TextAt(words[currentWordIndex].text, originX, textY, textColor);

    int nextLeft = left + currentWidth + gap;
    for (size_t index = currentWordIndex + 1; index < words.size(); ++index) {
      if (nextLeft >= virtualWidth + gap) {
        break;
      }
      const TextLayoutMetrics layout = layoutFor(index);
      const int width = widthFor(layout);
      originX = nextLeft - layout.minX;
      drawSerif70TextAt(words[index].text, originX, textY, textColor);
      nextLeft += width + gap;
    }

    int prevRight = left - gap;
    for (size_t index = currentWordIndex; index > 0;) {
      --index;
      if (prevRight <= -gap) {
        break;
      }
      const TextLayoutMetrics layout = layoutFor(index);
      const int width = widthFor(layout);
      const int prevLeft = prevRight - width;
      originX = prevLeft - layout.minX;
      drawSerif70TextAt(words[index].text, originX, textY, textColor);
      prevRight = prevLeft - gap;
    }
    if (!overlayText.isEmpty()) {
      drawTinyTextCentered(overlayText, overlayY, focusColor(), kTinyScale);
    }
    if (showFooter) {
      drawFooter(chapterLabel, String(progressPercent) + "%");
    }
    if (!canUseBandOnly) {
      drawBatteryBadge();
      flushScaledFrame(scale, virtualWidth, virtualHeight);
      tickerPlaybackFrameActive_ = !showFooter && overlayText.isEmpty();
    } else {
      flushFullWidthLogicalBand(bandTop, bandBottom);
    }
    return;
  }

  const ReaderTextStyle style = readerTextStyle(fontSizeLevel);
  auto layoutFor = [&](size_t index) {
    return serifWordLayoutScaledPercent(words[index].text, -1, style.scalePercent);
  };
  auto widthFor = [&](const TextLayoutMetrics &layout) { return textLayoutWidth(layout); };

  int gap = kWordTickerGapLarge;
  if (fontSizeLevel == 1) {
    gap = kWordTickerGapMedium;
  } else if (fontSizeLevel >= 2) {
    gap = kWordTickerGapSmall;
  }
  gap = std::max(4, scaledPercentDimension(gap, style.scalePercent));

  const int textHeight = scaledPercentDimension(
      baseGlyphHeightForTypeface(effectiveReaderTypefaceForText(words[currentWordIndex].text)),
      style.scalePercent);
  const int textY = std::max(0, (virtualHeight - textHeight) / 2);
  const TextLayoutMetrics currentLayout = layoutFor(currentWordIndex);
  const int currentWidth = widthFor(currentLayout);
  const int currentLeftBase = (virtualWidth - currentWidth) / 2;
  int shiftX = 0;
  if (currentWordIndex + 1 < words.size()) {
    const int nextWidth = widthFor(layoutFor(currentWordIndex + 1));
    const int travel = gap + (currentWidth / 2) + (nextWidth / 2);
    shiftX = static_cast<int>((static_cast<int32_t>(travel) * motionPermille) / 1000);
  }

  const int bandTop = std::max(0, textY - kWordTickerBandPadding);
  const int bandBottom = std::min(virtualHeight, textY + textHeight + kWordTickerBandPadding);
  if (canUseBandOnly) {
    fillVirtualRect(0, bandTop, virtualWidth, bandBottom - bandTop, backgroundColor());
  } else {
    clearVirtualBuffer(virtualWidth, virtualHeight);
  }
  int left = currentLeftBase - shiftX;
  int originX = left - currentLayout.minX;
  drawSerifTextScaledAt(words[currentWordIndex].text, originX, textY, textColor,
                        style.scalePercent);

  int nextLeft = left + currentWidth + gap;
  for (size_t index = currentWordIndex + 1; index < words.size(); ++index) {
    if (nextLeft >= virtualWidth + gap) {
      break;
    }
    const TextLayoutMetrics layout = layoutFor(index);
    const int width = widthFor(layout);
    originX = nextLeft - layout.minX;
    drawSerifTextScaledAt(words[index].text, originX, textY, textColor, style.scalePercent);
    nextLeft += width + gap;
  }

  int prevRight = left - gap;
  for (size_t index = currentWordIndex; index > 0;) {
    --index;
    if (prevRight <= -gap) {
      break;
    }
    const TextLayoutMetrics layout = layoutFor(index);
    const int width = widthFor(layout);
    const int prevLeft = prevRight - width;
    originX = prevLeft - layout.minX;
    drawSerifTextScaledAt(words[index].text, originX, textY, textColor, style.scalePercent);
    prevRight = prevLeft - gap;
  }
  if (!overlayText.isEmpty()) {
    drawTinyTextCentered(overlayText, overlayY, focusColor(), kTinyScale);
  }
  if (showFooter) {
    drawFooter(chapterLabel, String(progressPercent) + "%");
  }
  if (!canUseBandOnly) {
    drawBatteryBadge();
    flushScaledFrame(scale, virtualWidth, virtualHeight);
    tickerPlaybackFrameActive_ = !showFooter && overlayText.isEmpty();
  } else {
    flushFullWidthLogicalBand(bandTop, bandBottom);
  }
}

void DisplayManager::renderTypographyPreview(const String &beforeText, const String &word,
                                             const String &afterText, uint8_t fontSizeLevel,
                                             const String &title, const String &line1,
                                             const String &line2) {
  const TypographyConfig config = activeTypographyConfig();
  const String renderKey =
      "typography_preview|" + beforeText + "|" + word + "|" + afterText + "|s:" +
      String(fontSizeLevel) + "|" + title + "|" + line1 + "|" + line2 + "|t:" +
      String(static_cast<unsigned int>(config.typeface)) +
      "|h:" + String(config.focusHighlight ? 1 : 0) +
      "|tr:" +
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
    const int textHeight = mediumGlyphHeightForTypeface(effectiveReaderTypefaceForText(word));
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
    const int textHeight = scaledPercentDimension(
        baseGlyphHeightForTypeface(effectiveReaderTypefaceForText(word)), style.scalePercent);
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
                                                  uint8_t progressPercent, bool showFooter,
                                                  const String &footerStatusLabel) {
  const String wpmText = String(wpm) + " WPM";
  const String renderKey =
      "rsvp_phantom_wpm|" + beforeText + "|" + word + "|" + afterText + "|s:" +
      String(fontSizeLevel) + "|" + wpmText + "|" + chapterLabel + "|" +
      String(progressPercent) + "|" + String(showFooter ? 1 : 0) + "|f:" + footerStatusLabel +
      "|b:" + batteryLabel_ + "|d:" + String(darkMode_ ? 1 : 0) + "|n:" +
      String(nightMode_ ? 1 : 0);
  if (!initialized_ || renderKey == lastRenderKey_) {
    return;
  }

  lastRenderKey_ = renderKey;

  if (fontSizeLevel == 1) {
    const int scale = 1;
    const int virtualWidth = kDisplayWidth;
    const int virtualHeight = kDisplayHeight;
    const int mediumHeight = mediumGlyphHeightForTypeface(effectiveReaderTypefaceForText(word));
    const int textY = std::max(0, (virtualHeight - mediumHeight) / 2);
    const int wpmY =
        std::max(0, virtualHeight - kTinyGlyphHeight * kTinyScale - kWpmFeedbackBottomMargin - 24);
    const int focusIndex = findFocusLetterIndex(word);
    const int currentX = rsvpStartX70(word, focusIndex, virtualWidth, false);
    const int anchorX = (virtualWidth * currentAnchorPercent()) / 100;
    const TextLayoutMetrics currentLayout = serif70WordLayout(word, focusIndex);
    const uint16_t phantomColor = blendOverBackground(wordColor(), kPhantomAlphaMedium);

    clearVirtualBuffer(virtualWidth, virtualHeight);
    drawRsvpAnchorGuide(anchorX, textY, mediumHeight);
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
      drawFooter(chapterLabel, footerStatusLabel.isEmpty() ? String(progressPercent) + "%"
                                                           : footerStatusLabel);
      drawPreviousSentenceHint();
    }
    drawBatteryBadge();
    flushScaledFrame(scale, virtualWidth, virtualHeight);
    return;
  }

  const ReaderTextStyle style = readerTextStyle(fontSizeLevel);
  const int scale = 1;
  const int virtualWidth = kDisplayWidth;
  const int virtualHeight = kDisplayHeight;
  const int textHeight = scaledPercentDimension(
      baseGlyphHeightForTypeface(effectiveReaderTypefaceForText(word)), style.scalePercent);
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
    drawFooter(chapterLabel, footerStatusLabel.isEmpty() ? String(progressPercent) + "%"
                                                         : footerStatusLabel);
    drawPreviousSentenceHint();
  }
  drawBatteryBadge();
  flushScaledFrame(scale, virtualWidth, virtualHeight);
}

void DisplayManager::renderScrollView(const std::vector<ContextWord> &words, uint32_t contentToken,
                                      size_t windowStartIndex, size_t currentWordIndex,
                                      uint16_t scrollProgressPermille,
                                      const String &chapterLabel, uint8_t progressPercent,
                                      const String &overlayText,
                                      const String &footerStatusLabel) {
  if (words.empty()) {
    renderRsvpWord("", chapterLabel, progressPercent, true, footerStatusLabel);
    return;
  }

  struct ContextLine {
    size_t start = 0;
    size_t end = 0;
    bool paragraphStart = false;
  };

  const int scale = 1;
  const int virtualWidth = kDisplayWidth;
  const int virtualHeight = kDisplayHeight;
  const int overlayReserve = overlayText.isEmpty() ? 0 : (kTinyGlyphHeight * kTinyScale + 6);
  const int textTop = kScrollTop;
  const int textBottom =
      virtualHeight - kTinyGlyphHeight * kTinyScale - kFooterMarginBottom - 6 - overlayReserve;
  const ReaderTypeface contextTypeface = currentReaderTypeface();
  const int contextGlyphHeight = std::max(
      1, (baseGlyphHeightForTypeface(contextTypeface) + kScrollSerifDivisor - 1) /
             kScrollSerifDivisor);
  const int maxLineWidth = virtualWidth - (kScrollMarginX * 2);

  size_t currentLocalIndex = 0;
  if (currentWordIndex >= windowStartIndex && currentWordIndex < windowStartIndex + words.size()) {
    currentLocalIndex = currentWordIndex - windowStartIndex;
  }
  size_t nextLocalIndex = currentLocalIndex;
  if (nextLocalIndex + 1 < words.size()) {
    ++nextLocalIndex;
  }
  if (scrollProgressPermille > 1000) {
    scrollProgressPermille = 1000;
  }

  std::vector<ContextLine> lines;
  lines.reserve(16);
  size_t currentLineIndex = 0;
  size_t nextLineIndex = 0;
  bool foundCurrentLine = false;
  bool foundNextLine = false;

  size_t index = 0;
  while (index < words.size()) {
    ContextLine line;
    line.start = index;
    line.paragraphStart = words[index].paragraphStart;
    int lineWidth = line.paragraphStart ? kScrollParagraphIndent : 0;

    while (index < words.size()) {
      if (index > line.start && words[index].paragraphStart) {
        break;
      }

      const int wordWidth = measureSerifTextWidth(words[index].text, kScrollSerifDivisor);
      const int gap = (index == line.start) ? 0 : kScrollSpaceWidth;
      if (index > line.start && lineWidth + gap + wordWidth > maxLineWidth) {
        break;
      }

      lineWidth += gap + wordWidth;
      ++index;

      if (lineWidth >= maxLineWidth) {
        break;
      }
    }

    line.end = std::max(line.start + 1, index);
    if (line.end > words.size()) {
      line.end = words.size();
    }
    if (!foundCurrentLine && currentLocalIndex >= line.start && currentLocalIndex < line.end) {
      currentLineIndex = lines.size();
      foundCurrentLine = true;
    }
    if (!foundNextLine && nextLocalIndex >= line.start && nextLocalIndex < line.end) {
      nextLineIndex = lines.size();
      foundNextLine = true;
    }
    lines.push_back(line);

    if (line.end == line.start) {
      ++index;
    }
  }

  if (lines.empty()) {
    renderRsvpWord("", chapterLabel, progressPercent, true, footerStatusLabel);
    return;
  }

  if (!foundCurrentLine) {
    currentLineIndex = 0;
  }
  if (!foundNextLine) {
    nextLineIndex = currentLineIndex;
  }

  std::vector<int> lineTops;
  lineTops.reserve(lines.size());
  int contentBottom = textTop + contextGlyphHeight;
  int y = textTop;
  for (size_t lineIndex = 0; lineIndex < lines.size(); ++lineIndex) {
    if (lineIndex != 0 && lines[lineIndex].paragraphStart) {
      y += kScrollParagraphGap;
    }
    lineTops.push_back(y);
    contentBottom = y + contextGlyphHeight;
    y += kScrollLineHeight;
  }

  const int currentCenterY = lineTops[currentLineIndex] + (contextGlyphHeight / 2);
  const int nextCenterY = lineTops[nextLineIndex] + (contextGlyphHeight / 2);
  const int focusCenterY =
      currentCenterY +
      (((nextCenterY - currentCenterY) * static_cast<int>(scrollProgressPermille)) / 1000);
  const int preferredFocusY = textTop + ((textBottom - textTop) / 2);
  int scrollOffset = preferredFocusY - focusCenterY;
  const int minScrollOffset = std::min(0, textBottom - contentBottom);
  scrollOffset = std::max(minScrollOffset, std::min(0, scrollOffset));

  const String renderKey =
      "scroll|" + String(contentToken) + "|" + String(windowStartIndex) + "|" +
      String(currentWordIndex) + "|" + String(words.size()) + "|" + String(scrollOffset) +
      "|" + chapterLabel + "|" + String(progressPercent) + "|o:" + overlayText + "|f:" +
      footerStatusLabel + "|b:" + batteryLabel_ + "|d:" + String(darkMode_ ? 1 : 0) + "|n:" +
      String(nightMode_ ? 1 : 0);
  if (!initialized_ || renderKey == lastRenderKey_) {
    return;
  }

  lastRenderKey_ = renderKey;
  clearVirtualBuffer(virtualWidth, virtualHeight);

  for (size_t lineIndex = 0; lineIndex < lines.size(); ++lineIndex) {
    const ContextLine &line = lines[lineIndex];
    const int lineY = lineTops[lineIndex] + scrollOffset;
    if (lineY + contextGlyphHeight < 0) {
      continue;
    }
    if (lineY > textBottom) {
      break;
    }

    int x = kScrollMarginX + (line.paragraphStart ? kScrollParagraphIndent : 0);
    for (size_t wordIndex = line.start; wordIndex < line.end && wordIndex < words.size();
         ++wordIndex) {
      const ContextWord &word = words[wordIndex];
      const uint16_t color =
          (word.current && currentFocusHighlightEnabled()) ? focusColor() : wordColor();
      const String visibleWord =
          fitSerifText(word.text, virtualWidth - x - kScrollMarginX, kScrollSerifDivisor);
      drawSerifTextAt(visibleWord, x, lineY, color, kScrollSerifDivisor);
      x += measureSerifTextWidth(visibleWord, kScrollSerifDivisor) + kScrollSpaceWidth;
    }
  }

  if (!overlayText.isEmpty()) {
    const int overlayY = textBottom + 8;
    drawTinyTextCentered(fitTinyText(overlayText, virtualWidth - 24, kTinyScale), overlayY,
                         focusColor(), kTinyScale);
  }

  drawFooter(chapterLabel, footerStatusLabel.isEmpty() ? String(progressPercent) + "%"
                                                       : footerStatusLabel);
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

void DisplayManager::renderTextEntry(const String &title, const String &prompt, const String &value,
                                     const String &helperText,
                                     const std::vector<Button> &buttons) {
  String renderKey = "text-entry|";
  renderKey += title;
  renderKey += "|";
  renderKey += prompt;
  renderKey += "|";
  renderKey += value;
  renderKey += "|";
  renderKey += helperText;
  renderKey += "|b:";
  renderKey += batteryLabel_;
  renderKey += "|d:";
  renderKey += String(darkMode_ ? 1 : 0);
  renderKey += "|n:";
  renderKey += String(nightMode_ ? 1 : 0);
  for (const Button &button : buttons) {
    renderKey += "|";
    renderKey += button.label;
    renderKey += "@";
    renderKey += String(button.x);
    renderKey += ",";
    renderKey += String(button.y);
    renderKey += ",";
    renderKey += String(button.width);
    renderKey += ",";
    renderKey += String(button.height);
    renderKey += ",";
    renderKey += String(button.accent ? 1 : 0);
    renderKey += ",";
    renderKey += String(button.active ? 1 : 0);
  }

  if (!initialized_ || renderKey == lastRenderKey_) {
    return;
  }

  lastRenderKey_ = renderKey;

  const int scale = 1;
  const int virtualWidth = kDisplayWidth;
  const int virtualHeight = kDisplayHeight;
  const String headerText = title.isEmpty() ? helperText : title;
  const int headerY = 4;
  const int fieldX = 10;
  const int fieldY = headerText.isEmpty() ? 8 : 14;
  const int fieldWidth = virtualWidth - 20;
  const int fieldHeight = 28;
  constexpr uint8_t kFieldTextScalePercent = 36;
  const int fieldTextHeight = scaledPercentDimension(
      baseGlyphHeightForTypeface(effectiveReaderTypefaceForText(value.isEmpty() ? prompt : value)),
      kFieldTextScalePercent);
  const int fieldTextY = fieldY + std::max(1, (fieldHeight - fieldTextHeight) / 2);

  clearVirtualBuffer(virtualWidth, virtualHeight);

  if (!headerText.isEmpty()) {
    drawTinyTextCentered(fitTinyText(headerText, virtualWidth - 20, 1), headerY, footerColor(), 1);
  }

  fillVirtualRect(fieldX, fieldY, fieldWidth, fieldHeight, dimColor());
  fillVirtualRect(fieldX + 1, fieldY + 1, fieldWidth - 2, fieldHeight - 2, backgroundColor());
  if (value.isEmpty()) {
    if (!prompt.isEmpty()) {
      const String placeholder =
          fitSerifTextScaled(prompt, fieldWidth - 16, kFieldTextScalePercent);
      drawSerifTextScaledAt(placeholder, fieldX + 8, fieldTextY, dimColor(), kFieldTextScalePercent);
    }
  } else {
    const String fieldValue =
        fitSerifTextTrailingScaled(value, fieldWidth - 16, kFieldTextScalePercent);
    drawSerifTextScaledAt(fieldValue, fieldX + 8, fieldTextY, wordColor(), kFieldTextScalePercent);
  }

  for (const Button &button : buttons) {
    if (button.width <= 2 || button.height <= 2) {
      continue;
    }

    const uint16_t borderColor =
        button.active ? selectedBarColor() : (button.accent ? focusColor() : dimColor());
    uint16_t fillColor = backgroundColor();
    if (button.active) {
      fillColor = blendOverBackground(borderColor, nightMode_ ? 128 : 40);
    } else if (button.accent) {
      fillColor = blendOverBackground(borderColor, nightMode_ ? 92 : 24);
    }

    fillVirtualRect(button.x, button.y, button.width, button.height, borderColor);
    fillVirtualRect(button.x + 1, button.y + 1, button.width - 2, button.height - 2, fillColor);

    const bool singleAsciiLetter =
        button.label.length() == 1 &&
        ((button.label[0] >= 'a' && button.label[0] <= 'z') ||
         (button.label[0] >= 'A' && button.label[0] <= 'Z'));
    const uint8_t labelScalePercent = singleAsciiLetter ? 42 : 26;
    const String label =
        fitSerifTextScaled(button.label, std::max(0, static_cast<int>(button.width) - 8),
                           labelScalePercent);
    const int labelWidth = measureSerifTextWidthScaled(label, labelScalePercent);
    const int labelHeight = scaledPercentDimension(
        baseGlyphHeightForTypeface(effectiveReaderTypefaceForText(label)), labelScalePercent);
    const int textX =
        static_cast<int>(button.x) + std::max(0, (static_cast<int>(button.width) - labelWidth) / 2);
    const int textY =
        static_cast<int>(button.y) +
        std::max(1, (static_cast<int>(button.height) - labelHeight) / 2);
    if (singleAsciiLetter) {
      drawSerifTextScaledAt(label, textX, textY, wordColor(), labelScalePercent);
      continue;
    }

    if (!label.isEmpty()) {
      drawSerifTextScaledAt(label, textX, textY, wordColor(), labelScalePercent);
      continue;
    }

    const int fallbackScale = kTinyScale;
    const String fallbackLabel =
        fitTinyText(button.label, std::max(0, static_cast<int>(button.width) - 6), fallbackScale);
    const int fallbackWidth = measureTinyTextWidth(fallbackLabel, fallbackScale);
    const int fallbackX = static_cast<int>(button.x) +
                          std::max(0, (static_cast<int>(button.width) - fallbackWidth) / 2);
    const int fallbackY = static_cast<int>(button.y) +
                          std::max(1, (static_cast<int>(button.height) -
                                       (kTinyGlyphHeight * fallbackScale)) /
                                          2);
    drawTinyTextAt(fallbackLabel, fallbackX, fallbackY, wordColor(), fallbackScale);
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
  const int glyphHeight = baseGlyphHeightForTypeface(effectiveReaderTypefaceForText(title));
  const int titleY = std::max(0, (virtualHeight - glyphHeight) / 2 - 26);
  const int line1Y = std::min(virtualHeight - kTinyGlyphHeight * kTinyScale,
                              titleY + glyphHeight + 22);
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
  const int glyphHeight = baseGlyphHeightForTypeface(effectiveReaderTypefaceForText(title));
  const int titleY = std::max(0, (virtualHeight - glyphHeight) / 2 - 34);
  const int line1Y = std::min(virtualHeight - kTinyGlyphHeight * kTinyScale,
                              titleY + glyphHeight + 18);
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

void DisplayManager::renderFocusTimerScreen(const String &mode, const String &genre,
                                            const String &timer, const String &instruction,
                                            const String &footer, int progressPercent,
                                            bool breakAccent) {
  (void)genre;
  (void)footer;
  progressPercent = std::max(-1, std::min(100, progressPercent));
  const int virtualWidth = logicalWidth();
  const int virtualHeight = logicalHeight();
  const bool portrait = isPortraitOrientation(uiOrientation_);
  const bool timerRunning = progressPercent >= 0;

  String renderKey = "timer|";
  renderKey += mode;
  renderKey += "|";
  renderKey += genre;
  renderKey += "|";
  renderKey += timer;
  renderKey += "|";
  renderKey += instruction;
  renderKey += "|";
  renderKey += footer;
  renderKey += "|";
  renderKey += String(progressPercent);
  renderKey += "|o:";
  renderKey += String(static_cast<int>(uiOrientation_));
  renderKey += "|ba:";
  renderKey += String(breakAccent ? 1 : 0);
  renderKey += "|b:";
  renderKey += batteryLabel_;
  renderKey += "|d:";
  renderKey += String(darkMode_ ? 1 : 0);
  renderKey += "|n:";
  renderKey += String(nightMode_ ? 1 : 0);

  if (!initialized_ || renderKey == lastRenderKey_) {
    return;
  }

  lastRenderKey_ = renderKey;

  clearVirtualBuffer(virtualWidth, virtualHeight);

  const uint16_t accent = breakAccent ? focusTimerBreakColor() : focusColor();
  const uint16_t baseTextColor = wordColor();
  const uint16_t inverseTextColor = nightMode_ || darkMode_ ? wordColor() : kPureWhite;
  const uint16_t instructionColor = accent;
  int fillX = 0;
  int fillY = 0;
  int fillWidth = 0;
  int fillHeight = 0;
  if (timerRunning && progressPercent > 0) {
    if (portrait) {
      fillHeight = std::max(1, (virtualHeight * progressPercent) / 100);
      fillVirtualRect(0, virtualHeight - fillHeight, virtualWidth, fillHeight, accent);
      fillY = virtualHeight - fillHeight;
      fillWidth = virtualWidth;
    } else {
      fillWidth = std::max(1, (virtualWidth * progressPercent) / 100);
      fillVirtualRect(0, 0, fillWidth, virtualHeight, accent);
      fillHeight = virtualHeight;
    }
  }

  const int sidePadding = portrait ? 12 : 20;
  const int contentX = sidePadding;
  const int contentWidth = std::max(0, virtualWidth - (sidePadding * 2));
  auto wrapTinyLines = [&](const String &text, int maxWidth, int scale) {
    std::vector<String> lines;
    if (text.isEmpty() || maxWidth <= 0) {
      return lines;
    }

    auto fits = [&](const String &candidate) {
      return measureTinyTextWidth(candidate, scale) <= maxWidth;
    };

    auto appendBrokenWord = [&](const String &word, String &currentLine) {
      String segment;
      for (size_t i = 0; i < word.length(); ++i) {
        const String candidate = segment + word[i];
        if (!segment.isEmpty() && !fits(candidate)) {
          if (!currentLine.isEmpty()) {
            lines.push_back(currentLine);
            currentLine = "";
          }
          lines.push_back(segment);
          segment = String(word[i]);
        } else {
          segment = candidate;
        }
      }

      if (!segment.isEmpty()) {
        currentLine = segment;
      }
    };

    String currentLine;
    size_t index = 0;
    while (index < text.length()) {
      while (index < text.length() && text[index] == ' ') {
        ++index;
      }
      if (index >= text.length()) {
        break;
      }

      if (text[index] == '\n') {
        if (!currentLine.isEmpty()) {
          lines.push_back(currentLine);
          currentLine = "";
        }
        ++index;
        continue;
      }

      const size_t start = index;
      while (index < text.length() && text[index] != ' ' && text[index] != '\n') {
        ++index;
      }
      const String word = text.substring(start, index);
      if (currentLine.isEmpty()) {
        if (fits(word)) {
          currentLine = word;
        } else {
          appendBrokenWord(word, currentLine);
        }
        continue;
      }

      const String candidate = currentLine + " " + word;
      if (fits(candidate)) {
        currentLine = candidate;
      } else {
        lines.push_back(currentLine);
        currentLine = "";
        if (fits(word)) {
          currentLine = word;
        } else {
          appendBrokenWord(word, currentLine);
        }
      }
    }

    if (!currentLine.isEmpty()) {
      lines.push_back(currentLine);
    }

    return lines;
  };

  auto drawTinyTextAtClipped = [&](const String &text, int x, int y, uint16_t color, int scale,
                                   int clipX, int clipY, int clipWidth, int clipHeight) {
    if (clipWidth <= 0 || clipHeight <= 0) {
      return;
    }

    const int clipXEnd = clipX + clipWidth;
    const int clipYEnd = clipY + clipHeight;
    const uint16_t panel = panelColor(color);
    int cursorX = x;
    for (size_t i = 0; i < text.length(); ++i) {
      const uint8_t *rows = tinyRowsFor(text[i]);
      for (int row = 0; row < kTinyGlyphHeight; ++row) {
        for (int col = 0; col < kTinyGlyphWidth; ++col) {
          if ((rows[row] & (1 << (kTinyGlyphWidth - 1 - col))) == 0) {
            continue;
          }

          for (int yy = 0; yy < scale; ++yy) {
            const int dstY = y + row * scale + yy;
            if (dstY < 0 || dstY >= kVirtualBufferHeight || dstY < clipY || dstY >= clipYEnd) {
              continue;
            }

            for (int xx = 0; xx < scale; ++xx) {
              const int dstX = cursorX + col * scale + xx;
              if (dstX < 0 || dstX >= kVirtualBufferWidth || dstX < clipX || dstX >= clipXEnd) {
                continue;
              }
              virtualFrame_[dstY * kVirtualBufferWidth + dstX] = panel;
            }
          }
        }
      }
      cursorX += (kTinyGlyphWidth + kTinyGlyphSpacing) * scale;
    }
  };

  auto centeredXForTiny = [&](const String &text, int scale) {
    const int textWidth = measureTinyTextWidth(text, scale);
    return std::max(contentX, contentX + ((contentWidth - textWidth) / 2));
  };

  auto centeredXWithin = [&](const String &text, int scale, int blockX, int blockWidth) {
    const int textWidth = measureTinyTextWidth(text, scale);
    return std::max(blockX, blockX + ((blockWidth - textWidth) / 2));
  };

  const bool portraitFocusLayout = portrait && !breakAccent && (mode == "BEGIN" || mode == "WORK");
  int titleScale = portrait ? 5 : 7;
  if (portraitFocusLayout && timerRunning) {
    titleScale = 4;
  }
  while (titleScale > 1 && measureTinyTextWidth(mode, titleScale) > contentWidth) {
    --titleScale;
  }

  if (timerRunning) {
    int timerScale = portrait ? 5 : 7;
    while (timerScale > 1 && measureTinyTextWidth(timer, timerScale) > contentWidth) {
      --timerScale;
    }

    int titleY = portrait ? 120 : 24;
    int timerY = portrait ? 352 : 88;
    if (portraitFocusLayout) {
      titleY = 92;
      timerY = 306;
      const int dividerWidth =
          std::min(contentWidth, 40 + (static_cast<int>(mode.length()) * 12));
      const int dividerX = contentX + ((contentWidth - dividerWidth) / 2);
      const int dividerY = titleY + (kTinyGlyphHeight * titleScale) + 20;
      fillVirtualRect(dividerX, dividerY, dividerWidth, 2, accent);
    }
    const int titleX = centeredXForTiny(mode, titleScale);
    const int timerX = centeredXForTiny(timer, timerScale);

    drawTinyTextAt(mode, titleX, titleY, baseTextColor, titleScale);
    drawTinyTextAt(timer, timerX, timerY, baseTextColor, timerScale);

    if (fillWidth > 0 && fillHeight > 0) {
      drawTinyTextAtClipped(mode, titleX, titleY, inverseTextColor, titleScale, fillX, fillY,
                            fillWidth, fillHeight);
      drawTinyTextAtClipped(timer, timerX, timerY, inverseTextColor, timerScale, fillX, fillY,
                            fillWidth, fillHeight);
    }
  } else {
    int titleY = portrait ? 176 : 42;
    int dividerY = 0;
    int instructionScale = portrait ? 2 : 3;
    int instructionBlockWidth = contentWidth;
    int instructionBlockX = contentX;

    if (portraitFocusLayout) {
      titleY = 126;
      instructionScale = 2;
      instructionBlockWidth = std::max(96, contentWidth - 18);
      instructionBlockX = contentX + ((contentWidth - instructionBlockWidth) / 2);
      dividerY = titleY + (kTinyGlyphHeight * titleScale) + 22;
      const int dividerWidth =
          std::min(contentWidth, 40 + (static_cast<int>(mode.length()) * 12));
      const int dividerX = contentX + ((contentWidth - dividerWidth) / 2);
      fillVirtualRect(dividerX, dividerY, dividerWidth, 2, instructionColor);
    }

    drawTinyTextAt(mode, centeredXForTiny(mode, titleScale), titleY, baseTextColor, titleScale);

    if (!instruction.isEmpty()) {
      const std::vector<String> lines =
          wrapTinyLines(instruction, instructionBlockWidth, instructionScale);
      const int lineHeight = (kTinyGlyphHeight * instructionScale) + instructionScale + 4;
      int y = titleY + (kTinyGlyphHeight * titleScale) + (portrait ? 42 : 28);
      if (portraitFocusLayout) {
        y = dividerY + 66;
      }
      for (const String &line : lines) {
        drawTinyTextAt(line,
                       centeredXWithin(line, instructionScale, instructionBlockX,
                                       instructionBlockWidth),
                       y, instructionColor, instructionScale);
        y += lineHeight;
      }
    }
  }

  drawBatteryBadge(virtualWidth, virtualHeight);
  flushScaledFrame(1, virtualWidth, virtualHeight);
}
