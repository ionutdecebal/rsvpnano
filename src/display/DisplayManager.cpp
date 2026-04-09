#include "display/DisplayManager.h"

bool DisplayManager::begin() {
  tft_.init();
  tft_.setRotation(0);
  tft_.fillScreen(TFT_BLACK);
  tft_.setTextDatum(MC_DATUM);
  tft_.setTextFont(4);
  tft_.setTextColor(TFT_WHITE, TFT_BLACK);
  return true;
}

void DisplayManager::renderCenteredWord(const String &word, uint16_t color) {
  if (word == lastWord_) {
    return;
  }

  lastWord_ = word;
  tft_.fillScreen(TFT_BLACK);
  tft_.setTextColor(color, TFT_BLACK);
  tft_.drawString(word, tft_.width() / 2, tft_.height() / 2);
}
