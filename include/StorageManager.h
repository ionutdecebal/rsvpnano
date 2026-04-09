#pragma once

#include <Arduino.h>
#include <FS.h>
#include <SD_MMC.h>
#include <vector>
#include "Types.h"

struct BookFile {
  String id;
  String title;
  String path;
};

class StorageManager {
 public:
  bool begin();
  std::vector<BookFile> listBooks();
  bool readBook(const BookFile& f, String& out);

  bool loadSettings(String& selectedBookId, int& wpm);
  bool saveSettings(const String& selectedBookId, int wpm);
  bool loadProgress(const String& bookId, BookProgress& p);
  bool saveProgress(const String& bookId, const BookProgress& p);

 private:
  String settingsPath_ = "/rsvp/settings.json";
  String progressPathFor(const String& bookId) const;
  bool ensureDirs();
  bool ready_ = false;
};
