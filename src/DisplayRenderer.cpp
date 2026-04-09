#include "DisplayRenderer.h"

#include "BoardConfig.h"
#include <Adafruit_ST77xx.h>

namespace {
uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
  return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}
}

void DisplayRenderer::begin(Adafruit_GFX* gfx) {
  gfx_ = gfx;
}

void DisplayRenderer::drawLoading(const String& msg) {
  if (!gfx_) return;
  gfx_->fillScreen(ST77XX_BLACK);
  gfx_->setTextColor(ST77XX_WHITE);
  gfx_->setTextSize(2);
  gfx_->setCursor(20, BoardConfig::SCREEN_H / 2 - 16);
  gfx_->println(msg);
}

void DisplayRenderer::drawWord(const String& word, AppState state, float pauseAnimProgress) {
  if (!gfx_) return;

  uint16_t bg = ST77XX_BLACK;
  if (state == AppState::PAUSED) {
    uint8_t dim = (uint8_t)(15 * pauseAnimProgress);
    bg = rgb565(dim, dim, dim);
  }

  gfx_->fillScreen(bg);

  int textSize = 5;
  if (word.length() > 10) textSize = 4;
  if (word.length() > 14) textSize = 3;
  gfx_->setTextSize(textSize);
  gfx_->setTextColor(ST77XX_WHITE);

  int16_t x1, y1;
  uint16_t w, h;
  gfx_->getTextBounds(word, 0, 0, &x1, &y1, &w, &h);

  int cx = (BoardConfig::SCREEN_W - w) / 2;
  int cy = (BoardConfig::SCREEN_H - h) / 2;
  gfx_->setCursor(max(4, cx), max(4, cy));
  gfx_->print(word);
}

void DisplayRenderer::drawOverlayWpm(int wpm) {
  if (!gfx_) return;
  overlayUntilMs_ = millis() + 900;

  const int boxW = 150;
  const int boxH = 44;
  const int x = BoardConfig::SCREEN_W - boxW - 12;
  const int y = 12;

  gfx_->fillRoundRect(x, y, boxW, boxH, 8, rgb565(20, 20, 20));
  gfx_->drawRoundRect(x, y, boxW, boxH, 8, rgb565(120, 120, 120));
  gfx_->setTextColor(ST77XX_WHITE);
  gfx_->setTextSize(2);
  gfx_->setCursor(x + 14, y + 14);
  gfx_->printf("%d WPM", wpm);
}

void DisplayRenderer::drawStatus(const String& bookTitle, int wpm, size_t idx, size_t total) {
  if (!gfx_) return;
  gfx_->setTextSize(1);
  gfx_->setTextColor(rgb565(140, 140, 140));
  gfx_->setCursor(6, 6);
  gfx_->print(bookTitle);

  int pct = total > 0 ? (int)((idx * 100) / total) : 0;
  gfx_->setCursor(6, BoardConfig::SCREEN_H - 14);
  gfx_->printf("%d WPM  %d%%", wpm, pct);

  if (millis() > overlayUntilMs_) return;
}

void DisplayRenderer::drawMenu(const std::vector<String>& items, int selected) {
  if (!gfx_) return;
  gfx_->fillScreen(rgb565(10, 10, 10));
  gfx_->setTextColor(ST77XX_WHITE);
  gfx_->setTextSize(2);
  gfx_->setCursor(20, 16);
  gfx_->print("Menu");

  for (size_t i = 0; i < items.size(); ++i) {
    int y = 54 + i * 30;
    uint16_t c = (i == (size_t)selected) ? rgb565(120, 220, 255) : ST77XX_WHITE;
    gfx_->setTextColor(c);
    gfx_->setCursor(28, y);
    gfx_->print(items[i]);
  }
}

void DisplayRenderer::drawChapterList(const std::vector<ChapterAnchor>& chapters, int selected) {
  if (!gfx_) return;
  gfx_->fillScreen(rgb565(10, 10, 10));
  gfx_->setTextColor(ST77XX_WHITE);
  gfx_->setTextSize(2);
  gfx_->setCursor(20, 16);
  gfx_->print("Chapters");

  int start = max(0, selected - 5);
  for (int i = 0; i < 8; ++i) {
    int idx = start + i;
    if (idx >= (int)chapters.size()) break;

    int y = 54 + i * 22;
    uint16_t c = (idx == selected) ? rgb565(120, 220, 255) : ST77XX_WHITE;
    gfx_->setTextColor(c);
    gfx_->setTextSize(1);
    gfx_->setCursor(20, y);
    gfx_->print(chapters[idx].title);
  }
}
