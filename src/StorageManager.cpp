#include "StorageManager.h"

#include <ArduinoJson.h>

bool StorageManager::begin() {
  ready_ = SD_MMC.begin("/sdcard", true, false, SDMMC_FREQ_DEFAULT);
  if (!ready_) return false;
  return ensureDirs();
}

bool StorageManager::ensureDirs() {
  if (!SD_MMC.exists("/books")) SD_MMC.mkdir("/books");
  if (!SD_MMC.exists("/rsvp")) SD_MMC.mkdir("/rsvp");
  if (!SD_MMC.exists("/rsvp/progress")) SD_MMC.mkdir("/rsvp/progress");
  return true;
}

std::vector<BookFile> StorageManager::listBooks() {
  std::vector<BookFile> out;
  if (!ready_) return out;

  File dir = SD_MMC.open("/books");
  if (!dir || !dir.isDirectory()) return out;

  File f = dir.openNextFile();
  while (f) {
    String name = f.name();
    if (!f.isDirectory() && name.endsWith(".txt")) {
      BookFile bf;
      bf.path = name;
      int slash = name.lastIndexOf('/');
      bf.title = (slash >= 0) ? name.substring(slash + 1) : name;
      bf.id = bf.title;
      bf.id.replace(".txt", "");
      out.push_back(bf);
    }
    f = dir.openNextFile();
  }

  return out;
}

bool StorageManager::readBook(const BookFile& f, String& out) {
  if (!ready_) return false;
  File file = SD_MMC.open(f.path, FILE_READ);
  if (!file) return false;
  out = file.readString();
  return true;
}

bool StorageManager::loadSettings(String& selectedBookId, int& wpm) {
  if (!ready_ || !SD_MMC.exists(settingsPath_)) return false;
  File f = SD_MMC.open(settingsPath_, FILE_READ);
  if (!f) return false;

  JsonDocument doc;
  if (deserializeJson(doc, f) != DeserializationError::Ok) return false;

  selectedBookId = doc["selectedBookId"] | "";
  wpm = doc["wpm"] | 280;
  return true;
}

bool StorageManager::saveSettings(const String& selectedBookId, int wpm) {
  if (!ready_) return false;
  File f = SD_MMC.open(settingsPath_, FILE_WRITE);
  if (!f) return false;

  JsonDocument doc;
  doc["selectedBookId"] = selectedBookId;
  doc["wpm"] = wpm;
  serializeJson(doc, f);
  return true;
}

String StorageManager::progressPathFor(const String& bookId) const {
  return "/rsvp/progress/" + bookId + ".json";
}

bool StorageManager::loadProgress(const String& bookId, BookProgress& p) {
  if (!ready_) return false;
  String path = progressPathFor(bookId);
  if (!SD_MMC.exists(path)) return false;

  File f = SD_MMC.open(path, FILE_READ);
  if (!f) return false;
  JsonDocument doc;
  if (deserializeJson(doc, f) != DeserializationError::Ok) return false;

  p.wordIndex = doc["wordIndex"] | 0;
  p.wpm = doc["wpm"] | 280;
  return true;
}

bool StorageManager::saveProgress(const String& bookId, const BookProgress& p) {
  if (!ready_) return false;
  String path = progressPathFor(bookId);
  File f = SD_MMC.open(path, FILE_WRITE);
  if (!f) return false;

  JsonDocument doc;
  doc["wordIndex"] = p.wordIndex;
  doc["wpm"] = p.wpm;
  serializeJson(doc, f);
  return true;
}
