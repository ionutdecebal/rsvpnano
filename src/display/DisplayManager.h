#pragma once

#include <Arduino.h>
#include <vector>

class DisplayManager {
 public:
  struct TypographyConfig {
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

  ~DisplayManager();

  bool begin();
  void setBatteryLabel(const String &label);
  void setBrightnessPercent(uint8_t percent);
  void setDarkMode(bool darkMode);
  void setNightMode(bool nightMode);
  void setTypographyConfig(const TypographyConfig &config);
  TypographyConfig typographyConfig() const;
  bool darkMode() const;
  bool nightMode() const;
  void prepareForSleep();
  bool wakeFromSleep();
  void renderCenteredWord(const String &word, uint16_t color = 0xFFFF);
  void renderRsvpWord(const String &word, const String &chapterLabel = "",
                      uint8_t progressPercent = 0, bool showFooter = true);
  void renderRsvpWordWithWpm(const String &word, uint16_t wpm, const String &chapterLabel = "",
                             uint8_t progressPercent = 0, bool showFooter = true);
  void renderPhantomRsvpWord(const String &beforeText, const String &word, const String &afterText,
                             uint8_t fontSizeLevel, const String &chapterLabel = "",
                             uint8_t progressPercent = 0, bool showFooter = true);
  void renderPhantomRsvpWordWithWpm(const String &beforeText, const String &word,
                                    const String &afterText, uint8_t fontSizeLevel, uint16_t wpm,
                                    const String &chapterLabel = "",
                                    uint8_t progressPercent = 0, bool showFooter = true);
  void renderTypographyPreview(const String &beforeText, const String &word, const String &afterText,
                               uint8_t fontSizeLevel, const String &title,
                               const String &line1 = "", const String &line2 = "");
  void renderContextView(const std::vector<ContextWord> &words, const String &chapterLabel = "",
                         uint8_t progressPercent = 0);
  void renderMenu(const char *const *items, size_t itemCount, size_t selectedIndex);
  void renderMenu(const std::vector<String> &items, size_t selectedIndex);
  void renderLibrary(const std::vector<LibraryItem> &items, size_t selectedIndex);
  void renderStatus(const String &title, const String &line1 = "", const String &line2 = "");
  void renderProgress(const String &title, const String &line1 = "", const String &line2 = "",
                      int progressPercent = -1);

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
  String fitTinyText(const String &text, int maxWidth, int scale) const;
  void drawGlyph(int x, int y, char c, uint16_t color);
  void drawSerifGlyphScaled(int x, int y, char c, uint16_t color, int divisor);
  void drawSerif70Glyph(int x, int y, char c, uint16_t color);
  void drawSerifGlyphScaledPercent(int x, int y, char c, uint16_t color, uint8_t scalePercent);
  void fillVirtualRect(int x, int y, int width, int height, uint16_t color);
  void drawSerifTextAt(const String &text, int x, int y, uint16_t color, int divisor);
  void drawSerif70TextAt(const String &text, int x, int y, uint16_t color);
  void drawSerifTextScaledAt(const String &text, int x, int y, uint16_t color,
                             uint8_t scalePercent);
  void drawTinyGlyph(int x, int y, char c, uint16_t color, int scale);
  void drawTinyTextAt(const String &text, int x, int y, uint16_t color, int scale);
  void drawTinyTextCentered(const String &text, int y, uint16_t color, int scale);
  void drawBatteryBadge();
  void drawFooter(const String &chapterLabel, uint8_t progressPercent);
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

  uint16_t *virtualFrame_ = nullptr;
  uint16_t *txBuffer_ = nullptr;
  size_t txBufferBytes_ = 0;
  bool initialized_ = false;
  uint8_t brightnessPercent_ = 100;
  bool darkMode_ = true;
  bool nightMode_ = false;
  String lastRenderKey_;
  String batteryLabel_;
};
