#include "DisplayRenderer.h"

#include "BoardConfig.h"

void DisplayRenderer::begin(Arduino_GFX* gfx) {
  gfx_ = gfx;
}

void DisplayRenderer::drawLoading(const String& msg) {
  if (!gfx_) return;
  gfx_->fillScreen(BLACK);
  gfx_->setTextColor(WHITE);
  gfx_->setTextSize(2);
  gfx_->setCursor(20, BoardConfig::SCREEN_H / 2 - 16);
  gfx_->println(msg);
}

void DisplayRenderer::drawWord(const String& word, AppState state, float pauseAnimProgress) {
  if (!gfx_) return;

  uint16_t bg = BLACK;
  if (state == AppState::PAUSED) {
    uint8_t dim = (uint8_t)(15 * pauseAnimProgress);
    bg = gfx_->color565(dim, dim, dim);
  }

  gfx_->fillScreen(bg);

  int textSize = 5;
  if (word.length() > 10) textSize = 4;
  if (word.length() > 14) textSize = 3;
  gfx_->setTextSize(textSize);
  gfx_->setTextColor(WHITE);

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

  gfx_->fillRoundRect(x, y, boxW, boxH, 8, gfx_->color565(20, 20, 20));
  gfx_->drawRoundRect(x, y, boxW, boxH, 8, gfx_->color565(120, 120, 120));
  gfx_->setTextColor(WHITE);
  gfx_->setTextSize(2);
  gfx_->setCursor(x + 14, y + 14);
  gfx_->printf("%d WPM", wpm);
}

void DisplayRenderer::drawStatus(const String& bookTitle, int wpm, size_t idx, size_t total) {
  if (!gfx_) return;
  gfx_->setTextSize(1);
  gfx_->setTextColor(gfx_->color565(140, 140, 140));
  gfx_->setCursor(6, 6);
  gfx_->print(bookTitle);

  int pct = total > 0 ? (int)((idx * 100) / total) : 0;
  gfx_->setCursor(6, BoardConfig::SCREEN_H - 14);
  gfx_->printf("%d WPM  %d%%", wpm, pct);

  if (millis() > overlayUntilMs_) return;
}

void DisplayRenderer::drawMenu(const std::vector<String>& items, int selected) {
  if (!gfx_) return;
  gfx_->fillScreen(gfx_->color565(10, 10, 10));
  gfx_->setTextColor(WHITE);
  gfx_->setTextSize(2);
  gfx_->setCursor(20, 16);
  gfx_->print("Menu");

  for (size_t i = 0; i < items.size(); ++i) {
    int y = 54 + i * 30;
    uint16_t c = (i == (size_t)selected) ? gfx_->color565(120, 220, 255) : WHITE;
    gfx_->setTextColor(c);
    gfx_->setCursor(28, y);
    gfx_->print(items[i]);
  }
}

void DisplayRenderer::drawChapterList(const std::vector<ChapterAnchor>& chapters, int selected) {
  if (!gfx_) return;
  gfx_->fillScreen(gfx_->color565(10, 10, 10));
  gfx_->setTextColor(WHITE);
  gfx_->setTextSize(2);
  gfx_->setCursor(20, 16);
  gfx_->print("Chapters");

  int start = max(0, selected - 5);
  for (int i = 0; i < 8; ++i) {
    int idx = start + i;
    if (idx >= (int)chapters.size()) break;

    int y = 54 + i * 22;
    uint16_t c = (idx == selected) ? gfx_->color565(120, 220, 255) : WHITE;
    gfx_->setTextColor(c);
    gfx_->setTextSize(1);
    gfx_->setCursor(20, y);
    gfx_->print(chapters[idx].title);
  }
}
