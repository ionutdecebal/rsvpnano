#include "storage/SettingsFile.h"

#include <ArduinoJson.h>
#include <SD_MMC.h>

bool SettingsFile::load(SettingsData &out) {
  if (SD_MMC.exists(kTempPath)) {
    SD_MMC.remove(kTempPath);
    Serial.println("[settings] removed stale settings.tmp");
  }

  if (!SD_MMC.exists(kPath)) {
    return false;
  }

  File f = SD_MMC.open(kPath, FILE_READ);
  if (!f) {
    Serial.println("[settings] failed to open settings.json");
    return false;
  }

  JsonDocument doc;
  const DeserializationError err = deserializeJson(doc, f);
  f.close();

  if (err) {
    Serial.printf("[settings] JSON parse error: %s\n", err.c_str());
    return false;
  }

  JsonObjectConst s = doc["settings"];
  if (!s.isNull()) {
    out.brightness = s["brightness"] | out.brightness;
    out.darkMode = s["dark_mode"] | out.darkMode;
    out.nightMode = s["night_mode"] | out.nightMode;
    out.uiLanguage = s["ui_language"] | out.uiLanguage;
    out.readerMode = s["reader_mode"] | out.readerMode;
    out.handedness = s["handedness"] | out.handedness;
    out.phantomWords = s["phantom_words"] | out.phantomWords;
    out.footerMetric = s["footer_metric"] | out.footerMetric;
    out.fontSize = s["font_size"] | out.fontSize;
    out.typeface = s["typeface"] | out.typeface;
    out.focusHighlight = s["focus_highlight"] | out.focusHighlight;
    out.tracking = s["tracking"] | out.tracking;
    out.anchorPercent = s["anchor_percent"] | out.anchorPercent;
    out.guideWidth = s["guide_width"] | out.guideWidth;
    out.guideGap = s["guide_gap"] | out.guideGap;
    out.pacingLongMs = s["pacing_long_ms"] | out.pacingLongMs;
    out.pacingComplexMs = s["pacing_complex_ms"] | out.pacingComplexMs;
    out.pacingPunctuationMs = s["pacing_punctuation_ms"] | out.pacingPunctuationMs;
    out.wpm = s["wpm"] | out.wpm;
    if (s["wifi_ssid"].is<const char *>()) out.wifiSsid = s["wifi_ssid"].as<String>();
    if (s["wifi_pass"].is<const char *>()) out.wifiPass = s["wifi_pass"].as<String>();
    out.otaAuto = s["ota_auto"] | out.otaAuto;
    if (s["ota_owner"].is<const char *>()) out.otaOwner = s["ota_owner"].as<String>();
  }

  if (doc["current_book"].is<const char *>()) {
    out.currentBook = doc["current_book"].as<String>();
  }
  out.recentSequence = doc["recent_sequence"] | out.recentSequence;

  JsonArrayConst books = doc["books"];
  for (JsonObjectConst b : books) {
    if (!b["path"].is<const char *>()) continue;
    const String path = b["path"].as<String>();
    if (path.isEmpty()) continue;
    SettingsData::BookPos pos;
    pos.path = path;
    pos.position = b["position"] | 0u;
    pos.wordCount = b["word_count"] | 0u;
    pos.recentSeq = b["recent_seq"] | 0u;
    out.books.push_back(std::move(pos));
  }

  Serial.printf("[settings] loaded from SD (%u books)\n",
                static_cast<unsigned int>(out.books.size()));
  return true;
}

bool SettingsFile::save(const SettingsData &data) {
  File f = SD_MMC.open(kTempPath, FILE_WRITE);
  if (!f) {
    Serial.println("[settings] failed to open settings.tmp for write");
    return false;
  }

  JsonDocument doc;
  doc["version"] = 1;

  JsonObject s = doc["settings"].to<JsonObject>();
  s["brightness"] = data.brightness;
  s["dark_mode"] = data.darkMode;
  s["night_mode"] = data.nightMode;
  s["ui_language"] = data.uiLanguage;
  s["reader_mode"] = data.readerMode;
  s["handedness"] = data.handedness;
  s["phantom_words"] = data.phantomWords;
  s["footer_metric"] = data.footerMetric;
  s["font_size"] = data.fontSize;
  s["typeface"] = data.typeface;
  s["focus_highlight"] = data.focusHighlight;
  s["tracking"] = data.tracking;
  s["anchor_percent"] = data.anchorPercent;
  s["guide_width"] = data.guideWidth;
  s["guide_gap"] = data.guideGap;
  s["pacing_long_ms"] = data.pacingLongMs;
  s["pacing_complex_ms"] = data.pacingComplexMs;
  s["pacing_punctuation_ms"] = data.pacingPunctuationMs;
  s["wpm"] = data.wpm;
  s["wifi_ssid"] = data.wifiSsid;
  s["wifi_pass"] = data.wifiPass;
  s["ota_auto"] = data.otaAuto;
  s["ota_owner"] = data.otaOwner;

  doc["current_book"] = data.currentBook;
  doc["recent_sequence"] = data.recentSequence;

  JsonArray books = doc["books"].to<JsonArray>();
  for (const auto &b : data.books) {
    JsonObject entry = books.add<JsonObject>();
    entry["path"] = b.path;
    entry["position"] = b.position;
    entry["word_count"] = b.wordCount;
    entry["recent_seq"] = b.recentSeq;
  }

  serializeJsonPretty(doc, f);
  f.close();

  SD_MMC.remove(kPath);
  if (!SD_MMC.rename(kTempPath, kPath)) {
    Serial.println("[settings] rename failed");
    SD_MMC.remove(kTempPath);
    return false;
  }

  Serial.printf("[settings] saved to SD (%u books)\n",
                static_cast<unsigned int>(data.books.size()));
  return true;
}
