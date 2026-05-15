#pragma once

#include <Arduino.h>
#include <FS.h>
#include <vector>

#include "reader/BookContent.h"

class StorageManager {
 public:
  using StatusCallback = void (*)(void *context, const char *title, const char *line1,
                                  const char *line2, int progressPercent);

  struct DiagnosticResult {
    bool mounted = false;
    bool booksDirectory = false;
    bool writable = false;
    size_t bookCount = 0;
    size_t unsupportedCount = 0;
    uint64_t sizeMb = 0;
    String cardType;
    String summary;
    String detail;
  };

  void setStatusCallback(StatusCallback callback, void *context);
  bool begin();
  void end();
  void listBooks();
  void refreshBooks(bool includeMetadata = true);
  bool loadBookContent(size_t index, BookContent &book, String *loadedPath = nullptr,
                       size_t *loadedIndex = nullptr);
  size_t bookCount() const;
  String bookPath(size_t index) const;
  bool bookIsArticle(size_t index) const;
  String bookDisplayName(size_t index) const;
  String bookAuthorName(size_t index) const;
  DiagnosticResult diagnoseSdCard();

 private:
  bool parseFile(File &file, BookContent &book, bool rsvpFormat);
  bool ensureEpubConverted(const String &epubPath, String &rsvpPath);
  void refreshBookPaths(bool includeMetadata = true);
  void rebuildBookMetadataCache();
  void clearBookCache();
  void notifyStatus(const char *title, const char *line1 = "", const char *line2 = "",
                    int progressPercent = -1);

  bool mounted_ = false;
  bool listedOnce_ = false;
  StatusCallback statusCallback_ = nullptr;
  void *statusContext_ = nullptr;
  std::vector<String> bookPaths_;
  std::vector<String> bookTitles_;
  std::vector<String> bookAuthors_;
};
