#include "rss/FeedParser.h"

#include <algorithm>
#include <cctype>
#include <cstring>

#include "text/LatinText.h"

namespace feedparser {
namespace {

char lowerAscii(char c) {
  return (c >= 'A' && c <= 'Z') ? static_cast<char>(c + ('a' - 'A')) : c;
}

int hexValue(char c) {
  if (c >= '0' && c <= '9') {
    return c - '0';
  }
  if (c >= 'a' && c <= 'f') {
    return c - 'a' + 10;
  }
  if (c >= 'A' && c <= 'F') {
    return c - 'A' + 10;
  }
  return -1;
}

bool parseNumericEntity(const String &entity, uint32_t &codepoint) {
  if (!entity.startsWith("#")) {
    return false;
  }

  codepoint = 0;
  int base = 10;
  size_t index = 1;
  if (entity.length() > 2 && (entity[1] == 'x' || entity[1] == 'X')) {
    base = 16;
    index = 2;
  }
  if (index >= entity.length()) {
    return false;
  }

  for (; index < entity.length(); ++index) {
    const int digit =
        base == 16 ? hexValue(entity[index])
                   : (entity[index] >= '0' && entity[index] <= '9' ? entity[index] - '0' : -1);
    if (digit < 0 || digit >= base) {
      return false;
    }
    codepoint = codepoint * base + static_cast<uint32_t>(digit);
  }

  return codepoint <= 0x10FFFF && !(codepoint >= 0xD800 && codepoint <= 0xDFFF);
}

void appendText(String &target, const char *text) {
  while (*text != '\0') {
    target += *text;
    ++text;
  }
}

bool appendDecodedCodepoint(String &target, uint32_t codepoint) {
  if (codepoint == 0x00AD || codepoint == 0x200B || codepoint == 0xFEFF) {
    return true;
  }
  if (codepoint == '\t' || codepoint == '\n' || codepoint == '\r' || codepoint == 0x00A0 ||
      codepoint == 0x1680 || codepoint == 0x180E || codepoint == 0x2028 ||
      codepoint == 0x2029 || codepoint == 0x202F || codepoint == 0x205F ||
      codepoint == 0x3000 || (codepoint >= 0x2000 && codepoint <= 0x200A)) {
    target += ' ';
    return true;
  }

  uint8_t storedByte = 0;
  if (LatinText::storageByteForCodepoint(codepoint, storedByte)) {
    target += static_cast<char>(storedByte);
    return true;
  }

  if (codepoint >= 0xFF01 && codepoint <= 0xFF5E) {
    target += static_cast<char>(codepoint - 0xFEE0);
    return true;
  }

  switch (codepoint) {
    case 0x00A2:
      target += 'c';
      return true;
    case 0x00A3:
      appendText(target, "GBP");
      return true;
    case 0x00A4:
      target += '$';
      return true;
    case 0x00A5:
      target += 'Y';
      return true;
    case 0x00A9:
      appendText(target, "(c)");
      return true;
    case 0x00AE:
      appendText(target, "(r)");
      return true;
    case 0x00B0:
      appendText(target, "deg");
      return true;
    case 0x00B1:
      appendText(target, "+/-");
      return true;
    case 0x00B2:
      target += '2';
      return true;
    case 0x00B3:
      target += '3';
      return true;
    case 0x00B9:
      target += '1';
      return true;
    case 0x00BC:
      appendText(target, "1/4");
      return true;
    case 0x00BD:
      appendText(target, "1/2");
      return true;
    case 0x00BE:
      appendText(target, "3/4");
      return true;
    case 0x00D7:
      target += 'x';
      return true;
    case 0x00F7:
      target += '/';
      return true;
    case 0x2010:
    case 0x2011:
    case 0x2212:
      target += '-';
      return true;
    case 0x2012:
    case 0x2013:
    case 0x2014:
    case 0x2015:
      appendText(target, " - ");
      return true;
    case 0x2018:
    case 0x2019:
    case 0x201A:
    case 0x201B:
    case 0x2032:
    case 0x2035:
    case 0x2039:
    case 0x203A:
      target += '\'';
      return true;
    case 0x201C:
    case 0x201D:
    case 0x201E:
    case 0x201F:
    case 0x2033:
    case 0x2036:
    case 0x00AB:
    case 0x00BB:
      target += '"';
      return true;
    case 0x2022:
    case 0x00B7:
    case 0x2219:
      target += '*';
      return true;
    case 0x2026:
      appendText(target, "...");
      return true;
    default:
      return false;
  }
}

bool namedEntityCodepoint(const String &entity, uint32_t &codepoint) {
  struct NamedEntity {
    const char *name;
    uint32_t codepoint;
  };
  static constexpr NamedEntity kNamedEntities[] = {
      {"Agrave", 0x00C0}, {"Aacute", 0x00C1}, {"Acirc", 0x00C2},
      {"Atilde", 0x00C3}, {"Auml", 0x00C4},   {"Aring", 0x00C5},
      {"AElig", 0x00C6},  {"Ccedil", 0x00C7}, {"Egrave", 0x00C8},
      {"Eacute", 0x00C9}, {"Ecirc", 0x00CA},  {"Euml", 0x00CB},
      {"Igrave", 0x00CC}, {"Iacute", 0x00CD}, {"Icirc", 0x00CE},
      {"Iuml", 0x00CF},   {"ETH", 0x00D0},    {"Ntilde", 0x00D1},
      {"Ograve", 0x00D2}, {"Oacute", 0x00D3}, {"Ocirc", 0x00D4},
      {"Otilde", 0x00D5}, {"Ouml", 0x00D6},   {"Oslash", 0x00D8},
      {"Ugrave", 0x00D9}, {"Uacute", 0x00DA}, {"Ucirc", 0x00DB},
      {"Uuml", 0x00DC},   {"Yacute", 0x00DD}, {"THORN", 0x00DE},
      {"szlig", 0x00DF},  {"agrave", 0x00E0}, {"aacute", 0x00E1},
      {"acirc", 0x00E2},  {"atilde", 0x00E3}, {"auml", 0x00E4},
      {"aring", 0x00E5},  {"aelig", 0x00E6},  {"ccedil", 0x00E7},
      {"egrave", 0x00E8}, {"eacute", 0x00E9}, {"ecirc", 0x00EA},
      {"euml", 0x00EB},   {"igrave", 0x00EC}, {"iacute", 0x00ED},
      {"icirc", 0x00EE},  {"iuml", 0x00EF},   {"eth", 0x00F0},
      {"ntilde", 0x00F1}, {"ograve", 0x00F2}, {"oacute", 0x00F3},
      {"ocirc", 0x00F4},  {"otilde", 0x00F5}, {"ouml", 0x00F6},
      {"oslash", 0x00F8}, {"ugrave", 0x00F9}, {"uacute", 0x00FA},
      {"ucirc", 0x00FB},  {"uuml", 0x00FC},   {"yacute", 0x00FD},
      {"thorn", 0x00FE},  {"yuml", 0x00FF},   {"iexcl", 0x00A1},
      {"iquest", 0x00BF}, {"copy", 0x00A9},   {"reg", 0x00AE},
      {"deg", 0x00B0},    {"plusmn", 0x00B1}, {"sup2", 0x00B2},
      {"sup3", 0x00B3},   {"sup1", 0x00B9},   {"frac14", 0x00BC},
      {"frac12", 0x00BD}, {"frac34", 0x00BE}, {"laquo", 0x00AB},
      {"raquo", 0x00BB},  {"middot", 0x00B7}, {"bull", 0x2022},
      {"times", 0x00D7},  {"divide", 0x00F7},
  };

  for (const NamedEntity &entry : kNamedEntities) {
    if (entity == entry.name) {
      codepoint = entry.codepoint;
      return true;
    }
  }
  return false;
}

bool decodeXmlEntity(const String &entity, String &decoded) {
  decoded = "";
  if (entity == "amp") {
    decoded += '&';
    return true;
  }
  if (entity == "lt") {
    decoded += '<';
    return true;
  }
  if (entity == "gt") {
    decoded += '>';
    return true;
  }
  if (entity == "quot") {
    decoded += '"';
    return true;
  }
  if (entity == "apos") {
    decoded += '\'';
    return true;
  }
  if (entity == "nbsp") {
    decoded += ' ';
    return true;
  }
  if (entity == "ndash" || entity == "mdash") {
    appendText(decoded, " - ");
    return true;
  }
  if (entity == "hellip") {
    appendText(decoded, "...");
    return true;
  }
  if (entity == "rsquo" || entity == "lsquo" || entity == "sbquo") {
    decoded += '\'';
    return true;
  }
  if (entity == "rdquo" || entity == "ldquo" || entity == "bdquo") {
    decoded += '"';
    return true;
  }

  uint32_t codepoint = 0;
  if ((parseNumericEntity(entity, codepoint) || namedEntityCodepoint(entity, codepoint)) &&
      appendDecodedCodepoint(decoded, codepoint)) {
    return true;
  }

  return false;
}

String decodeXmlEntitiesOnce(const String &value) {
  String output;
  output.reserve(value.length());

  for (size_t i = 0; i < value.length(); ++i) {
    const char c = value[i];
    if (c != '&') {
      output += c;
      continue;
    }

    const int entityEnd = value.indexOf(';', i + 1);
    if (entityEnd <= 0 || entityEnd - static_cast<int>(i) > 32) {
      output += c;
      continue;
    }

    String decoded;
    const String entity = value.substring(i + 1, entityEnd);
    if (!decodeXmlEntity(entity, decoded)) {
      output += c;
      continue;
    }

    output += decoded;
    i = static_cast<size_t>(entityEnd);
  }

  return output;
}

bool matchesIgnoreCaseAt(const String &text, size_t index, const char *needle) {
  for (size_t i = 0; needle[i] != '\0'; ++i) {
    if (index + i >= text.length() || lowerAscii(text[index + i]) != lowerAscii(needle[i])) {
      return false;
    }
  }
  return true;
}

int indexOfIgnoreCase(const String &text, const char *needle, size_t start, size_t limit) {
  const size_t needleLength = strlen(needle);
  if (needleLength == 0 || start >= text.length()) {
    return -1;
  }
  limit = std::min(limit, static_cast<size_t>(text.length()));
  if (limit < needleLength) {
    return -1;
  }
  for (size_t i = start; i + needleLength <= limit; ++i) {
    if (matchesIgnoreCaseAt(text, i, needle)) {
      return static_cast<int>(i);
    }
  }
  return -1;
}

int tagEndIndex(const String &text, size_t start, size_t limit) {
  limit = std::min(limit, static_cast<size_t>(text.length()));
  for (size_t i = start; i < limit; ++i) {
    if (text[i] == '>') {
      return static_cast<int>(i);
    }
  }
  return -1;
}

String valueBetween(const String &text, const String &openTag, const String &closeTag,
                    size_t start, size_t end) {
  const int open = indexOfIgnoreCase(text, openTag.c_str(), start, end);
  if (open < 0 || static_cast<size_t>(open) >= end) {
    return "";
  }
  const int valueStart = tagEndIndex(text, static_cast<size_t>(open), end);
  if (valueStart < 0 || static_cast<size_t>(valueStart) >= end) {
    return "";
  }
  const int close = indexOfIgnoreCase(text, closeTag.c_str(), valueStart + 1, end);
  if (close < 0 || static_cast<size_t>(close) > end) {
    return "";
  }
  return text.substring(valueStart + 1, close);
}

String attributeValue(const String &text, const String &tagPrefix, const String &attribute,
                      size_t start, size_t end) {
  int tagStart = indexOfIgnoreCase(text, tagPrefix.c_str(), start, end);
  while (tagStart >= 0 && static_cast<size_t>(tagStart) < end) {
    const int tagEnd = tagEndIndex(text, static_cast<size_t>(tagStart), end);
    if (tagEnd < 0 || static_cast<size_t>(tagEnd) > end) {
      return "";
    }

    const String needle = attribute + "=";
    int attrIndex = indexOfIgnoreCase(text, needle.c_str(), tagStart, static_cast<size_t>(tagEnd));
    if (attrIndex >= 0) {
      int valueStart = attrIndex + needle.length();
      while (valueStart < tagEnd && isspace(static_cast<unsigned char>(text[valueStart]))) {
        ++valueStart;
      }
      if (valueStart < tagEnd) {
        const char quote = text[valueStart];
        if (quote == '"' || quote == '\'') {
          ++valueStart;
          for (int i = valueStart; i < tagEnd; ++i) {
            if (text[i] == quote) {
              return text.substring(valueStart, i);
            }
          }
        } else {
          int valueEnd = valueStart;
          while (valueEnd < tagEnd && !isspace(static_cast<unsigned char>(text[valueEnd])) &&
                 text[valueEnd] != '>') {
            ++valueEnd;
          }
          if (valueEnd > valueStart) {
            return text.substring(valueStart, valueEnd);
          }
        }
      }
    }

    tagStart = indexOfIgnoreCase(text, tagPrefix.c_str(), static_cast<size_t>(tagEnd + 1), end);
  }
  return "";
}

String stripHtml(const String &html) {
  String output;
  output.reserve(std::min(static_cast<size_t>(html.length()), kMaxArticleChars));
  bool inTag = false;
  for (size_t i = 0; i < html.length(); ++i) {
    const char c = html[i];
    if (c == '<') {
      inTag = true;
      if (!output.endsWith(" ") && !output.endsWith("\n")) {
        output += ' ';
      }
      continue;
    }
    if (c == '>') {
      inTag = false;
      continue;
    }
    if (!inTag) {
      output += c;
    }
    if (output.length() >= kMaxArticleChars) {
      break;
    }
  }
  return output;
}

String xmlDecode(String value) {
  value = decodeXmlEntitiesOnce(value);
  if (value.indexOf('&') >= 0) {
    value = decodeXmlEntitiesOnce(value);
  }
  return value;
}

String cleanText(String value) {
  value.replace("<![CDATA[", "");
  value.replace("]]>", "");
  value = stripHtml(value);
  value = xmlDecode(value);
  value.replace("\r", "\n");
  while (value.indexOf("\n\n\n") >= 0) {
    value.replace("\n\n\n", "\n\n");
  }
  value.trim();
  return value;
}

}  // namespace

String hostLabelForUrl(const String &url) {
  int start = url.indexOf("://");
  start = start < 0 ? 0 : start + 3;
  int end = url.indexOf('/', start);
  if (end < 0) {
    end = url.length();
  }
  String host = url.substring(start, end);
  if (host.startsWith("www.")) {
    host.remove(0, 4);
  }
  return host;
}

String sourceLabelForItem(const FeedItem &item) {
  String source = item.link;
  if (source.isEmpty()) {
    return "RSS";
  }

  source = hostLabelForUrl(source);
  if (source.isEmpty()) {
    return "RSS";
  }
  return source;
}

bool parseNextItem(const String &feedBody, size_t &searchStart, FeedItem &item) {
  int itemStart = indexOfIgnoreCase(feedBody, "<item", searchStart, feedBody.length());
  bool atom = false;
  if (itemStart < 0) {
    itemStart = indexOfIgnoreCase(feedBody, "<entry", searchStart, feedBody.length());
    atom = itemStart >= 0;
  }
  if (itemStart < 0) {
    return false;
  }

  const String closeTag = atom ? "</entry>" : "</item>";
  const int itemEnd = indexOfIgnoreCase(feedBody, closeTag.c_str(), itemStart, feedBody.length());
  if (itemEnd < 0) {
    return false;
  }
  searchStart = static_cast<size_t>(itemEnd + closeTag.length());

  const size_t start = static_cast<size_t>(itemStart);
  const size_t end = static_cast<size_t>(itemEnd);
  item.title = cleanText(valueBetween(feedBody, "<title", "</title>", start, end));
  item.link = cleanText(valueBetween(feedBody, "<link>", "</link>", start, end));
  if (item.link.isEmpty()) {
    item.link = cleanText(attributeValue(feedBody, "<link", "href", start, end));
  }
  if (item.link.isEmpty()) {
    item.link = cleanText(valueBetween(feedBody, "<guid", "</guid>", start, end));
  }
  item.author = cleanText(valueBetween(feedBody, "<author", "</author>", start, end));
  if (item.author.isEmpty()) {
    item.author = cleanText(valueBetween(feedBody, "<dc:creator", "</dc:creator>", start, end));
  }
  if (item.author.isEmpty()) {
    item.author = sourceLabelForItem(item);
  }

  item.body = cleanText(valueBetween(feedBody, "<content:encoded", "</content:encoded>", start, end));
  if (item.body.isEmpty()) {
    item.body = cleanText(valueBetween(feedBody, "<content", "</content>", start, end));
  }
  if (item.body.isEmpty()) {
    item.body = cleanText(valueBetween(feedBody, "<description", "</description>", start, end));
  }
  if (item.body.isEmpty()) {
    item.body = cleanText(valueBetween(feedBody, "<summary", "</summary>", start, end));
  }
  if (item.body.isEmpty()) {
    item.body = item.link;
  }

  if (item.title.isEmpty()) {
    item.title = item.link.isEmpty() ? "RSS Article" : item.link;
  }
  return !item.body.isEmpty();
}

}  // namespace feedparser
