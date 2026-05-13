#pragma once

#include <Arduino.h>
#include <vector>

// All persistent settings, including per-book reading positions.
// Serialized to settings.json on the SD card on every change.
struct SettingsData {
  // Display & UI
  uint8_t brightness = 4;
  bool darkMode = true;
  bool nightMode = false;
  uint8_t uiLanguage = 0;
  uint8_t readerMode = 0;
  uint8_t handedness = 0;
  bool phantomWords = true;
  uint8_t footerMetric = 0;
  uint8_t fontSize = 0;
  // Typography
  uint8_t typeface = 0;
  bool focusHighlight = true;
  int8_t tracking = 0;
  uint8_t anchorPercent = 35;
  uint8_t guideWidth = 20;
  uint8_t guideGap = 4;
  // Pacing
  uint16_t pacingLongMs = 200;
  uint16_t pacingComplexMs = 200;
  uint16_t pacingPunctuationMs = 200;
  // Reading speed
  uint16_t wpm = 300;
  // Network / OTA
  String wifiSsid;
  String wifiPass;  // Note: stored in plaintext on SD card
  bool otaAuto = false;
  String otaOwner;

  // Per-book reading positions
  struct BookPos {
    String path;
    uint32_t position = 0;
    uint32_t wordCount = 0;
    uint32_t recentSeq = 0;
  };
  String currentBook;
  uint32_t recentSequence = 0;
  std::vector<BookPos> books;
};

class SettingsFile {
 public:
  // Reads /settings.json from the SD card and merges values into out (missing
  // keys keep whatever value out already has). Returns true on success.
  bool load(SettingsData &out);

  // Writes data to /settings.json on the SD card atomically via a temp file.
  // Returns true on success.
  bool save(const SettingsData &data);

 private:
  static constexpr const char *kPath = "/settings.json";
  static constexpr const char *kTempPath = "/settings.tmp";
};
