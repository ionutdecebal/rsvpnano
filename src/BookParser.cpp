#include "BookParser.h"

BookData BookParser::parse(const String& id, const String& title, const String& content) {
  BookData out;
  out.id = id;
  out.title = title;

  int start = 0;
  bool paragraphBreak = true;
  while (start < content.length()) {
    int end = content.indexOf('\n', start);
    if (end < 0) end = content.length();

    String line = content.substring(start, end);
    line.trim();

    if (line.length() == 0) {
      paragraphBreak = true;
    } else {
      if (looksLikeChapter(line)) {
        ChapterAnchor c;
        c.title = line;
        c.wordIndex = out.words.size();
        out.chapters.push_back(c);
      }
      appendLineWords(out, line, paragraphBreak);
      paragraphBreak = false;
    }

    start = end + 1;
  }

  if (out.chapters.empty()) {
    out.chapters.push_back({"Start", 0});
  }
  return out;
}

bool BookParser::looksLikeChapter(const String& line) const {
  String s = line;
  s.trim();
  String lower = s;
  lower.toLowerCase();

  if (lower.startsWith("chapter ") || lower.startsWith("book ") || lower.startsWith("part ")) {
    return true;
  }

  if (s.length() <= 30) {
    bool roman = true;
    for (size_t i = 0; i < s.length(); ++i) {
      char c = s[i];
      if (!(c == 'I' || c == 'V' || c == 'X' || c == 'L' || c == 'C' || c == 'D' || c == 'M' || c == ' ')) {
        roman = false;
        break;
      }
    }
    if (roman) return true;
  }

  return false;
}

void BookParser::appendLineWords(BookData& out, const String& line, bool paragraphBreak) {
  int i = 0;
  while (i < line.length()) {
    while (i < line.length() && isspace(line[i])) i++;
    int j = i;
    while (j < line.length() && !isspace(line[j])) j++;
    if (j <= i) break;

    String w = line.substring(i, j);
    WordToken t;
    t.text = w;
    t.paragraphBreak = paragraphBreak;
    paragraphBreak = false;

    char last = w[w.length() - 1];
    t.sentenceEnd = (last == '.' || last == '!' || last == '?');

    out.words.push_back(t);
    i = j;
  }
}
