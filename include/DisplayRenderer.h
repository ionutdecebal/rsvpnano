#pragma once

#include <Arduino.h>
#include <vector>

#include "DisplayCompat.h"
#include "Types.h"

class DisplayRenderer {
 public:
  void begin(DisplaySurface* gfx);

  void drawLoading(const String& msg);
  void drawWord(const String& word, AppState state, float pauseAnimProgress);
  void drawOverlayWpm(int wpm);
  void drawStatus(const String& bookTitle, int wpm, size_t idx, size_t total);
  void drawMenu(const std::vector<String>& items, int selected);
  void drawChapterList(const std::vector<ChapterAnchor>& chapters, int selected);

 private:
  DisplaySurface* gfx_ = nullptr;
  uint32_t overlayUntilMs_ = 0;
};
