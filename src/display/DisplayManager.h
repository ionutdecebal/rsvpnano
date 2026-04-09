#pragma once

#include <Arduino.h>
#include <TFT_eSPI.h>

class DisplayManager {
 public:
  bool begin();
  void renderCenteredWord(const String &word, uint16_t color = TFT_WHITE);

 private:
  TFT_eSPI tft_ = TFT_eSPI();
  String lastWord_;
};
