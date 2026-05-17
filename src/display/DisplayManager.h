#pragma once

#include <Arduino.h>
#include <vector>

#include "board/BoardConfig.h"

class DisplayManager {
 public:
  enum class ReaderTypeface : uint8_t {
    Standard = 0,
    OpenDyslexic = 1,
    AtkinsonHyperlegible = 2,
  };

  struct TypographyConfig {
    ReaderTypeface typeface = ReaderTypeface::Standard;
    bool focusHighlight = true;
    int8_t trackingPx = 0;
    uint8_t anchorPercent = 35;
    uint8_t guideHalfWidth = 20;
    uint8_t guideGap = 4;
  };

  struct ContextWord {
    String text;
    bool paragraphStart = false;
    bool current = false;
  };

  struct LibraryItem {
    String title;
    String subtitle;
  };

  struct Button {
    String label;
    uint16_t x = 0;
    uint16_t y = 0;
    uint16_t width = 0;
    uint16_t height = 0;
    bool accent = false;
    bool active = false;
  };

  ~DisplayManager();

  bool begin();
  void setBatteryLabel(const String &label);
  void setBrightnessPercent(uint8_t percent);
  void setDarkMode(bool darkMode);
  void setNightMode(bool nightMode);
  void setUiOrientation(BoardConfig::UiOrientation orientation);
  void setUiRotated180(bool rotated180);
  void setTypographyConfig(const TypographyConfig &config);
  TypographyConfig typographyConfig() const;
  bool darkMode() const;
  bool nightMode() const;
  void prepareForSleep();
  bool wakeFromSleep();
  void renderCenteredWord(const String &word, uint16_t color = 0xFFFF);
  void renderRsvpWord(const String &word, const String &chapterLabel = "",
                      uint8_t progressPercent = 0, bool showFooter = true,
                      const String &footerStatusLabel = "");
  void renderRsvpWordWithWpm(const String &word, uint16_t wpm, const String &chapterLabel = "",
                             uint8_t progressPercent = 0, bool showFooter = true,
                             const String &footerStatusLabel = "");
  void renderPhantomRsvpWord(const String &beforeText, const String &word, const String &afterText,
                             uint8_t fontSizeLevel, const String &chapterLabel = "",
                             uint8_t progressPercent = 0, bool showFooter = true,
                             const String &footerStatusLabel = "");
  void renderPhantomRsvpWordWithWpm(const String &beforeText, const String &word,
                                    const String &afterText, uint8_t fontSizeLevel, uint16_t wpm,
                                    const String &chapterLabel = "",
                                    uint8_t progressPercent = 0, bool showFooter = true,
                                    const String &footerStatusLabel = "");
  void renderTypographyPreview(const String &beforeText, const String &word, const String &afterText,
                               uint8_t fontSizeLevel, const String &title,
                               const String &line1 = "", const String &line2 = "");
  void renderScrollView(const std::vector<ContextWord> &words, uint32_t contentToken,
                        size_t windowStartIndex, size_t currentWordIndex,
                        uint16_t scrollProgressPermille = 0, const String &chapterLabel = "",
                        uint8_t progressPercent = 0, const String &overlayText = "",
                        const String &footerStatusLabel = "");
  void renderWordTickerView(const std::vector<ContextWord> &words, size_t currentWordIndex,
                            uint8_t fontSizeLevel, uint16_t motionPermille = 0,
                            const String &chapterLabel = "", uint8_t progressPercent = 0,
                            const String &overlayText = "", bool showFooter = true);
  void renderMenu(const char *const *items, size_t itemCount, size_t selectedIndex);
  void renderMenu(const std::vector<String> &items, size_t selectedIndex);
  void renderLibrary(const std::vector<LibraryItem> &items, size_t selectedIndex);
  void renderTextEntry(const String &title, const String &prompt, const String &value,
                       const String &helperText, const std::vector<Button> &buttons);
  void renderStatus(const String &title, const String &line1 = "", const String &line2 = "");
  void renderProgress(const String &title, const String &line1 = "", const String &line2 = "",
                      int progressPercent = -1);
  void renderFocusTimerScreen(const String &mode, const String &genre, const String &timer,
                              const String &instruction, const String &footer = "",
                              int progressPercent = -1, bool breakAccent = false);

 private:
  bool initPanel();
  bool allocateBuffers();
  bool drawBitmap(int xStart, int yStart, int xEnd, int yEnd, const void *colorData);
  void fillScreen(uint16_t color);
  void clearVirtualBuffer(int width, int height);
  uint16_t backgroundColor() const;
  uint16_t wordColor() const;
  uint16_t focusColor() const;
  uint16_t dimColor() const;
  uint16_t footerColor() const;
  uint16_t selectedBarColor() const;
  uint16_t blendOverBackground(uint16_t rgb565, uint8_t alpha) const;
  int chooseTextScale(const String &word) const;
  int measureTextWidth(const String &word) const;
  int measureSerifTextWidth(const String &text, int divisor) const;
  int measureSerif70TextWidth(const String &text) const;
  int measureSerifTextWidthScaled(const String &text, uint8_t scalePercent) const;
  int measureTinyTextWidth(const String &text, int scale) const;
  String fitSerifText(const String &text, int maxWidth, int divisor) const;
  String fitSerifTextScaled(const String &text, int maxWidth, uint8_t scalePercent) const;
  String fitSerifTextTrailingScaled(const String &text, int maxWidth, uint8_t scalePercent) const;
  String fitTinyText(const String &text, int maxWidth, int scale) const;
  String fitTinyTextTrailing(const String &text, int maxWidth, int scale) const;
  void drawGlyph(int x, int y, char c, uint16_t color);
  void drawGlyph(int x, int y, char c, uint16_t color, ReaderTypeface typeface);
  void drawSerifGlyphScaled(int x, int y, char c, uint16_t color, int divisor);
  void drawSerifGlyphScaled(int x, int y, char c, uint16_t color, int divisor,
                            ReaderTypeface typeface);
  void drawSerif70Glyph(int x, int y, char c, uint16_t color);
  void drawSerif70Glyph(int x, int y, char c, uint16_t color, ReaderTypeface typeface);
  void drawSerifGlyphScaledPercent(int x, int y, char c, uint16_t color, uint8_t scalePercent);
  void drawSerifGlyphScaledPercent(int x, int y, char c, uint16_t color, uint8_t scalePercent,
                                   ReaderTypeface typeface);
  void fillVirtualRect(int x, int y, int width, int height, uint16_t color);
  void drawSerifTextAt(const String &text, int x, int y, uint16_t color, int divisor);
  void drawSerif70TextAt(const String &text, int x, int y, uint16_t color);
  void drawSerifTextScaledAt(const String &text, int x, int y, uint16_t color,
                             uint8_t scalePercent);
  void drawTinyGlyph(int x, int y, char c, uint16_t color, int scale);
  void drawTinyTextAt(const String &text, int x, int y, uint16_t color, int scale);
  void drawTinyTextCentered(const String &text, int y, uint16_t color, int scale);
  void drawTinyTextCentered(const String &text, int y, uint16_t color, int scale, int width,
                            int xOffset);
  void drawSerif70TextCentered(const String &text, int y, uint16_t color, int width, int xOffset);
  void drawSerifTextScaledCentered(const String &text, int y, uint16_t color, uint8_t scalePercent,
                                   int width, int xOffset);
  void drawBatteryBadge();
  void drawBatteryBadge(int logicalWidth, int logicalHeight);
  void drawPreviousSentenceHint();
  void drawFooter(const String &chapterLabel, const String &statusLabel);
  void drawRsvpAnchorGuide(int anchorX, int textY, int textHeight);
  void drawWordAt(const String &word, int x, int y, uint16_t color);
  void drawRsvpWordAt(const String &word, int x, int y, int focusIndex);
  void drawRsvp70WordAt(const String &word, int x, int y, int focusIndex);
  void drawRsvpWordScaledAt(const String &word, int x, int y, int focusIndex, int divisor);
  void drawRsvpWordScaledPercentAt(const String &word, int x, int y, int focusIndex,
                                   uint8_t scalePercent);
  void drawWordLine(const String &word, int y, uint16_t color);
  void drawMenuItem(const String &item, int y, bool selected);
  void applyBrightness();
  void flushScaledFrame(int scale, int virtualWidth, int virtualHeight);
  void flushFullWidthLogicalBand(int yStart, int yEnd);
  int logicalWidth() const;
  int logicalHeight() const;
  uint16_t focusTimerBreakColor() const;

  uint16_t *virtualFrame_ = nullptr;
  uint16_t *txBuffer_ = nullptr;
  size_t txBufferBytes_ = 0;
  bool initialized_ = false;
  uint8_t brightnessPercent_ = 100;
  bool darkMode_ = true;
  bool nightMode_ = false;
  BoardConfig::UiOrientation uiOrientation_ =
      BoardConfig::UI_ROTATED_180 ? BoardConfig::UiOrientation::LandscapeFlipped
                                  : BoardConfig::UiOrientation::Landscape;
  bool tickerPlaybackFrameActive_ = false;
  String lastRenderKey_;
  String batteryLabel_;
};
