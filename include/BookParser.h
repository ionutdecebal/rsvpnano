#pragma once

#include "Types.h"

class BookParser {
 public:
  BookData parse(const String& id, const String& title, const String& content);

 private:
  bool looksLikeChapter(const String& line) const;
  void appendLineWords(BookData& out, const String& line, bool paragraphBreak);
};
