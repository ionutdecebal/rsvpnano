#include "storage/EpubConverter.h"

#include <SD_MMC.h>
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <esp32s3/rom/miniz.h>
#include <esp_heap_caps.h>
#include <vector>

#include "text/LatinText.h"

namespace {

constexpr uint32_t kZipEocdSignature = 0x06054B50UL;
constexpr uint32_t kZipCentralFileSignature = 0x02014B50UL;
constexpr uint32_t kZipLocalFileSignature = 0x04034B50UL;
constexpr uint16_t kZipStored = 0;
constexpr uint16_t kZipDeflated = 8;
constexpr size_t kZipEocdMaxSearch = 66UL * 1024UL;
constexpr size_t kMaxOpfBytes = 256UL * 1024UL;
constexpr size_t kMaxContainerBytes = 32UL * 1024UL;
constexpr uint16_t kMaxZipEntries = 2048;
constexpr uint16_t kMaxZipNameLength = 512;
constexpr size_t kReadChunkBytes = 4096;
constexpr size_t kInflateInputChunkBytes = 4096;
constexpr size_t kMaxTagChars = 512;
constexpr size_t kMaxEntityChars = 16;
constexpr size_t kOutputWrapWidth = 96;
constexpr size_t kBufferedTextFlushThreshold = 220;
constexpr const char *kConverterVersion = "stream-v6";

enum class ContentExtractStatus {
  Complete,
  WordLimitReached,
  Unsupported,
  Failed,
};

struct ZipEntry {
  String name;
  uint16_t method = 0;
  uint16_t flags = 0;
  uint32_t compressedSize = 0;
  uint32_t uncompressedSize = 0;
  uint32_t localHeaderOffset = 0;
};

struct ManifestItem {
  String id;
  String path;
  String mediaType;
};

uint16_t readLe16(const uint8_t *data) {
  return static_cast<uint16_t>(data[0]) | (static_cast<uint16_t>(data[1]) << 8);
}

uint32_t readLe32(const uint8_t *data) {
  return static_cast<uint32_t>(data[0]) | (static_cast<uint32_t>(data[1]) << 8) |
         (static_cast<uint32_t>(data[2]) << 16) | (static_cast<uint32_t>(data[3]) << 24);
}

void serviceBackground() {
  yield();
  delay(0);
}

bool hasWordLimit(size_t maxWords) { return maxWords > 0; }

bool reachedWordLimit(size_t wordCount, size_t maxWords) {
  return hasWordLimit(maxWords) && wordCount >= maxWords;
}

bool readExact(File &file, uint8_t *buffer, size_t length) {
  size_t offset = 0;
  while (offset < length) {
    const size_t chunk = std::min(kReadChunkBytes, length - offset);
    const uint32_t beforePosition = static_cast<uint32_t>(file.position());
    const int bytesRead = file.read(buffer + offset, chunk);
    if (bytesRead != static_cast<int>(chunk)) {
      Serial.printf(
          "[epub-zip] Short read at pos=%lu wanted=%u got=%d totalWanted=%u offset=%u\n",
          static_cast<unsigned long>(beforePosition), static_cast<unsigned int>(chunk),
          bytesRead, static_cast<unsigned int>(length), static_cast<unsigned int>(offset));
      return false;
    }
    offset += chunk;
    serviceBackground();
  }

  return true;
}

void reportProgress(const EpubConverter::Options &options, const char *line1, const char *line2,
                    int progressPercent) {
  if (options.progressCallback == nullptr) {
    return;
  }

  progressPercent = std::max(0, std::min(100, progressPercent));
  options.progressCallback(options.progressContext, line1, line2, progressPercent);
  serviceBackground();
}

void *allocateBuffer(size_t bytes) {
  if (bytes == 0) {
    return nullptr;
  }

  void *buffer = heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (buffer == nullptr) {
    buffer = heap_caps_malloc(bytes, MALLOC_CAP_8BIT);
  }
  return buffer;
}

void *allocateInternalBuffer(size_t bytes) {
  if (bytes == 0) {
    return nullptr;
  }

  return heap_caps_malloc(bytes, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
}

void freeBuffer(void *buffer) {
  if (buffer != nullptr) {
    heap_caps_free(buffer);
  }
}

String toLowerCopy(String value) {
  value.toLowerCase();
  return value;
}

bool isWhitespace(char c) { return std::isspace(static_cast<unsigned char>(c)) != 0; }

bool startsWithAt(const String &text, int position, const char *needle) {
  const size_t needleLength = std::strlen(needle);
  if (position < 0 || static_cast<size_t>(position) + needleLength > text.length()) {
    return false;
  }

  for (size_t i = 0; i < needleLength; ++i) {
    if (text[static_cast<size_t>(position) + i] != needle[i]) {
      return false;
    }
  }
  return true;
}

String basenameWithoutExtension(const String &path) {
  const int separator = path.lastIndexOf('/');
  String name = separator >= 0 ? path.substring(separator + 1) : path;
  const int dot = name.lastIndexOf('.');
  if (dot > 0) {
    name = name.substring(0, dot);
  }
  name.trim();
  return name.isEmpty() ? String("Untitled") : name;
}

String normalizeZipName(String path) {
  path.replace('\\', '/');
  while (path.startsWith("/")) {
    path.remove(0, 1);
  }
  return path;
}

bool isArchiveHintEntry(const String &name) {
  const String lowered = toLowerCopy(name);
  return lowered.indexOf("container") >= 0 || lowered.endsWith(".opf") ||
         lowered.endsWith(".ncx") || lowered.endsWith(".xhtml") ||
         lowered.endsWith(".html") || lowered.endsWith(".htm");
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

String percentDecodePath(const String &path) {
  String decoded;
  decoded.reserve(path.length());

  for (size_t i = 0; i < path.length(); ++i) {
    if (path[i] == '%' && i + 2 < path.length()) {
      const int high = hexValue(path[i + 1]);
      const int low = hexValue(path[i + 2]);
      if (high >= 0 && low >= 0) {
        decoded += static_cast<char>((high << 4) | low);
        i += 2;
        continue;
      }
    }
    decoded += path[i];
  }

  return decoded;
}

String collapseZipPath(const String &path) {
  std::vector<String> parts;
  size_t start = 0;

  while (start <= path.length()) {
    int separator = path.indexOf('/', start);
    if (separator < 0) {
      separator = path.length();
    }

    String part = path.substring(start, separator);
    if (part == "..") {
      if (!parts.empty()) {
        parts.pop_back();
      }
    } else if (!part.isEmpty() && part != ".") {
      parts.push_back(part);
    }

    if (static_cast<size_t>(separator) >= path.length()) {
      break;
    }
    start = static_cast<size_t>(separator) + 1;
  }

  String collapsed;
  for (size_t i = 0; i < parts.size(); ++i) {
    if (i > 0) {
      collapsed += "/";
    }
    collapsed += parts[i];
  }
  return collapsed;
}

String directoryForPath(const String &path) {
  const int separator = path.lastIndexOf('/');
  if (separator < 0) {
    return "";
  }
  return path.substring(0, separator + 1);
}

String resolveZipPath(const String &baseDirectory, const String &href) {
  String path = href;

  int fragment = path.indexOf('#');
  if (fragment >= 0) {
    path = path.substring(0, fragment);
  }
  int query = path.indexOf('?');
  if (query >= 0) {
    path = path.substring(0, query);
  }

  path = percentDecodePath(path);
  path = normalizeZipName(path);
  if (!href.startsWith("/")) {
    path = baseDirectory + path;
  }

  return collapseZipPath(path);
}

String attributeValue(const String &tag, const char *name) {
  const String key(name);
  int position = 0;

  while (position >= 0 && static_cast<size_t>(position) < tag.length()) {
    position = tag.indexOf(key, position);
    if (position < 0) {
      return "";
    }

    const bool boundaryBefore =
        position == 0 || isWhitespace(tag[position - 1]) || tag[position - 1] == '<' ||
        tag[position - 1] == '/';
    int afterName = position + key.length();
    if (!boundaryBefore ||
        (static_cast<size_t>(afterName) < tag.length() &&
         !(isWhitespace(tag[afterName]) || tag[afterName] == '='))) {
      position = afterName;
      continue;
    }

    while (static_cast<size_t>(afterName) < tag.length() && isWhitespace(tag[afterName])) {
      ++afterName;
    }
    if (static_cast<size_t>(afterName) >= tag.length() || tag[afterName] != '=') {
      position = afterName;
      continue;
    }
    ++afterName;
    while (static_cast<size_t>(afterName) < tag.length() && isWhitespace(tag[afterName])) {
      ++afterName;
    }
    if (static_cast<size_t>(afterName) >= tag.length()) {
      return "";
    }

    const char quote = tag[afterName];
    if (quote == '"' || quote == '\'') {
      const int end = tag.indexOf(quote, afterName + 1);
      if (end < 0) {
        return "";
      }
      return tag.substring(afterName + 1, end);
    }

    int end = afterName;
    while (static_cast<size_t>(end) < tag.length() && !isWhitespace(tag[end]) && tag[end] != '>') {
      ++end;
    }
    return tag.substring(afterName, end);
  }

  return "";
}

char entityCodepointByte(uint32_t codepoint) {
  uint8_t storedByte = 0;
  if (LatinText::storageByteForCodepoint(codepoint, storedByte)) {
    return static_cast<char>(storedByte);
  }
  return ' ';
}

char entityPunctuationChar(uint32_t codepoint) {
  if (codepoint >= 0xFF01 && codepoint <= 0xFF5E) {
    return static_cast<char>(codepoint - 0xFEE0);
  }

  switch (codepoint) {
    case 0x00A0:
      return ' ';
    case 0x00AB:
    case 0x00BB:
    case 0x201C:
    case 0x201D:
    case 0x201E:
    case 0x201F:
    case 0x2033:
    case 0x2036:
    case 0x300C:
    case 0x300D:
    case 0x300E:
    case 0x300F:
      return '"';
    case 0x2018:
    case 0x2019:
    case 0x201A:
    case 0x201B:
    case 0x2032:
    case 0x2035:
    case 0x2039:
    case 0x203A:
      return '\'';
    case 0x2010:
    case 0x2011:
    case 0x2012:
    case 0x2013:
    case 0x2014:
    case 0x2015:
    case 0x2043:
    case 0x2212:
      return '-';
    case 0x2022:
    case 0x00B7:
    case 0x2219:
      return '*';
    case 0x2026:
      return '.';
    case 0x207D:
    case 0x208D:
    case 0x2768:
    case 0x276A:
      return '(';
    case 0x207E:
    case 0x208E:
    case 0x2769:
    case 0x276B:
      return ')';
    case 0x2045:
    case 0x2308:
    case 0x230A:
    case 0x3010:
    case 0x3014:
    case 0x3016:
    case 0x3018:
    case 0x301A:
      return '[';
    case 0x2046:
    case 0x2309:
    case 0x230B:
    case 0x3011:
    case 0x3015:
    case 0x3017:
    case 0x3019:
    case 0x301B:
      return ']';
    case 0x2774:
    case 0x2776:
      return '{';
    case 0x2775:
    case 0x2777:
      return '}';
    case 0x2329:
    case 0x27E8:
    case 0x3008:
    case 0x300A:
      return '<';
    case 0x232A:
    case 0x27E9:
    case 0x3009:
    case 0x300B:
      return '>';
    case 0xFF0C:
      return ',';
    case 0xFF0E:
      return '.';
    case 0xFF1A:
      return ':';
    case 0xFF1B:
      return ';';
    case 0xFF01:
      return '!';
    case 0xFF1F:
      return '?';
    default:
      return '\0';
  }
}

char decodedEntityChar(const String &entity) {
  if (entity == "amp") {
    return '&';
  }
  if (entity == "lt") {
    return '<';
  }
  if (entity == "gt") {
    return '>';
  }
  if (entity == "quot") {
    return '"';
  }
  if (entity == "apos") {
    return '\'';
  }
  if (entity == "nbsp") {
    return ' ';
  }
  if (entity == "ldquo" || entity == "rdquo" || entity == "bdquo") {
    return '"';
  }
  if (entity == "lsquo" || entity == "rsquo" || entity == "sbquo") {
    return '\'';
  }
  if (entity == "laquo" || entity == "raquo") {
    return '"';
  }
  if (entity == "lsaquo" || entity == "rsaquo") {
    return '\'';
  }
  if (entity == "lpar") {
    return '(';
  }
  if (entity == "rpar") {
    return ')';
  }
  if (entity == "lbrack") {
    return '[';
  }
  if (entity == "rbrack") {
    return ']';
  }
  if (entity == "lcub") {
    return '{';
  }
  if (entity == "rcub") {
    return '}';
  }
  if (entity == "ndash" || entity == "mdash") {
    return '-';
  }
  if (entity == "hyphen" || entity == "minus") {
    return '-';
  }
  if (entity == "hellip") {
    return '.';
  }
  if (entity == "middot" || entity == "bull") {
    return '*';
  }
  if (entity == "AElig") {
    return static_cast<char>(0xC6);
  }
  if (entity == "aelig") {
    return static_cast<char>(0xE6);
  }
  if (entity == "Aring") {
    return static_cast<char>(0xC5);
  }
  if (entity == "aring") {
    return static_cast<char>(0xE5);
  }
  if (entity == "Auml") {
    return static_cast<char>(0xC4);
  }
  if (entity == "auml") {
    return static_cast<char>(0xE4);
  }
  if (entity == "Ccedil") {
    return static_cast<char>(0xC7);
  }
  if (entity == "ccedil") {
    return static_cast<char>(0xE7);
  }
  if (entity == "ETH") {
    return static_cast<char>(0xD0);
  }
  if (entity == "eth") {
    return static_cast<char>(0xF0);
  }
  if (entity == "Ntilde") {
    return static_cast<char>(0xD1);
  }
  if (entity == "ntilde") {
    return static_cast<char>(0xF1);
  }
  if (entity == "Oslash") {
    return static_cast<char>(0xD8);
  }
  if (entity == "oslash") {
    return static_cast<char>(0xF8);
  }
  if (entity == "Ouml") {
    return static_cast<char>(0xD6);
  }
  if (entity == "ouml") {
    return static_cast<char>(0xF6);
  }
  if (entity == "THORN") {
    return static_cast<char>(0xDE);
  }
  if (entity == "thorn") {
    return static_cast<char>(0xFE);
  }
  if (entity == "Uuml") {
    return static_cast<char>(0xDC);
  }
  if (entity == "uuml") {
    return static_cast<char>(0xFC);
  }
  if (entity == "szlig") {
    return static_cast<char>(0xDF);
  }
  if (entity == "Dcaron") {
    return static_cast<char>(0x01);
  }
  if (entity == "dcaron") {
    return static_cast<char>(0x02);
  }
  if (entity == "Ecaron") {
    return static_cast<char>(0x03);
  }
  if (entity == "ecaron") {
    return static_cast<char>(0x04);
  }
  if (entity == "Ncaron") {
    return static_cast<char>(0x05);
  }
  if (entity == "ncaron") {
    return static_cast<char>(0x06);
  }
  if (entity == "Rcaron") {
    return static_cast<char>(0x07);
  }
  if (entity == "rcaron") {
    return static_cast<char>(0x08);
  }
  if (entity == "Tcaron") {
    return static_cast<char>(0x0E);
  }
  if (entity == "tcaron") {
    return static_cast<char>(0x0F);
  }
  if (entity == "Uring") {
    return static_cast<char>(0x10);
  }
  if (entity == "uring") {
    return static_cast<char>(0x11);
  }
  if (entity == "Odblac") {
    return static_cast<char>(0x12);
  }
  if (entity == "odblac") {
    return static_cast<char>(0x13);
  }
  if (entity == "Udblac") {
    return static_cast<char>(0x14);
  }
  if (entity == "udblac") {
    return static_cast<char>(0x15);
  }
  if (entity == "OElig") {
    return static_cast<char>(0x80);
  }
  if (entity == "oelig") {
    return static_cast<char>(0x81);
  }
  if (entity == "Scaron") {
    return static_cast<char>(0x86);
  }
  if (entity == "scaron") {
    return static_cast<char>(0x87);
  }
  if (entity == "Zcaron") {
    return static_cast<char>(0x88);
  }
  if (entity == "zcaron") {
    return static_cast<char>(0x89);
  }
  if (entity == "Amacr") {
    return static_cast<char>(0xA1);
  }
  if (entity == "amacr") {
    return static_cast<char>(0xA2);
  }
  if (entity == "Emacr") {
    return static_cast<char>(0xA3);
  }
  if (entity == "emacr") {
    return static_cast<char>(0xA4);
  }
  if (entity == "Gcedil" || entity == "Gcommaaccent") {
    return static_cast<char>(0xA5);
  }
  if (entity == "gcedil" || entity == "gcommaaccent") {
    return static_cast<char>(0xA6);
  }
  if (entity == "Imacr") {
    return static_cast<char>(0xA7);
  }
  if (entity == "imacr") {
    return static_cast<char>(0xA8);
  }
  if (entity == "Kcedil" || entity == "Kcommaaccent") {
    return static_cast<char>(0xA9);
  }
  if (entity == "kcedil" || entity == "kcommaaccent") {
    return static_cast<char>(0xAA);
  }
  if (entity == "Lcedil" || entity == "Lcommaaccent") {
    return static_cast<char>(0xAB);
  }
  if (entity == "lcedil" || entity == "lcommaaccent") {
    return static_cast<char>(0xAC);
  }
  if (entity == "Ncedil" || entity == "Ncommaaccent") {
    return static_cast<char>(0xAE);
  }
  if (entity == "ncedil" || entity == "ncommaaccent") {
    return static_cast<char>(0xAF);
  }
  if (entity == "Edot") {
    return static_cast<char>(0xB0);
  }
  if (entity == "edot") {
    return static_cast<char>(0xB1);
  }
  if (entity == "Iogon") {
    return static_cast<char>(0xB6);
  }
  if (entity == "iogon") {
    return static_cast<char>(0xB7);
  }
  if (entity == "Uogon") {
    return static_cast<char>(0xB8);
  }
  if (entity == "uogon") {
    return static_cast<char>(0xB9);
  }
  if (entity == "Umacr") {
    return static_cast<char>(0xBA);
  }
  if (entity == "umacr") {
    return static_cast<char>(0xBB);
  }
  if (entity == "Dstrok") {
    return static_cast<char>(0xBC);
  }
  if (entity == "dstrok") {
    return static_cast<char>(0xBD);
  }
  if (entity == "ENG") {
    return static_cast<char>(0xBE);
  }
  if (entity == "eng") {
    return static_cast<char>(0xBF);
  }
  if (entity == "Tstrok") {
    return static_cast<char>(0xD7);
  }
  if (entity == "tstrok") {
    return static_cast<char>(0xF7);
  }

  if (entity.startsWith("#")) {
    uint32_t value = 0;
    int start = 1;
    int base = 10;
    if (entity.length() > 2 && (entity[1] == 'x' || entity[1] == 'X')) {
      start = 2;
      base = 16;
    }

    for (size_t i = start; i < entity.length(); ++i) {
      const int digit = base == 16 ? hexValue(entity[i]) : (entity[i] >= '0' && entity[i] <= '9'
                                                               ? entity[i] - '0'
                                                               : -1);
      if (digit < 0 || digit >= base) {
        return ' ';
      }
      value = value * base + static_cast<uint32_t>(digit);
    }

    const char mapped = entityCodepointByte(value);
    if (mapped != ' ') {
      return mapped;
    }
    const char punctuation = entityPunctuationChar(value);
    if (punctuation != '\0') {
      return punctuation;
    }
  }

  return ' ';
}

bool isUtf8Continuation(uint8_t value) { return (value & 0xC0) == 0x80; }

bool decodeUtf8Codepoint(const String &text, size_t &index, uint32_t &codepoint) {
  const uint8_t first = static_cast<uint8_t>(text[index++]);
  if (first < 0x80) {
    codepoint = first;
    return true;
  }

  uint8_t continuationCount = 0;
  uint32_t minimumValue = 0;
  if ((first & 0xE0) == 0xC0) {
    codepoint = first & 0x1F;
    continuationCount = 1;
    minimumValue = 0x80;
  } else if ((first & 0xF0) == 0xE0) {
    codepoint = first & 0x0F;
    continuationCount = 2;
    minimumValue = 0x800;
  } else if ((first & 0xF8) == 0xF0) {
    codepoint = first & 0x07;
    continuationCount = 3;
    minimumValue = 0x10000;
  } else {
    return false;
  }

  if (index + continuationCount > text.length()) {
    return false;
  }

  for (uint8_t i = 0; i < continuationCount; ++i) {
    const uint8_t next = static_cast<uint8_t>(text[index]);
    if (!isUtf8Continuation(next)) {
      return false;
    }
    ++index;
    codepoint = (codepoint << 6) | (next & 0x3F);
  }

  if (codepoint < minimumValue || codepoint > 0x10FFFF ||
      (codepoint >= 0xD800 && codepoint <= 0xDFFF)) {
    return false;
  }

  return true;
}

void appendDisplayApproximation(String &target, uint32_t codepoint) {
  if (codepoint >= 32 && codepoint <= 126) {
    target += static_cast<char>(codepoint);
    return;
  }

  if (codepoint == 0x200B || codepoint == 0xFEFF) {
    return;
  }

  if (codepoint == 0x00A0) {
    target += ' ';
    return;
  }

  if (codepoint >= 0x2000 && codepoint <= 0x200A) {
    target += ' ';
    return;
  }

  uint8_t storedByte = 0;
  if (LatinText::storageByteForCodepoint(codepoint, storedByte)) {
    target += static_cast<char>(storedByte);
    return;
  }

  if (codepoint >= 0x3000 && codepoint <= 0x303F) {
    return;
  }

  switch (codepoint) {
    case 0x2010:
    case 0x2011:
    case 0x2012:
    case 0x2013:
    case 0x2014:
    case 0x2015:
      target += '-';
      return;
    case 0x2018:
    case 0x2019:
    case 0x201B:
      target += '\'';
      return;
    case 0x201C:
    case 0x201D:
    case 0x201F:
      target += '"';
      return;
    case 0x2026:
      target += '.';
      return;
    case 0x2032:
      target += '\'';
      return;
    case 0x2033:
      target += '"';
      return;
    case 0x2039:
      target += '<';
      return;
    case 0x203A:
      target += '>';
      return;
    default:
      return;
  }
}

String normalizeDisplayText(const String &text) {
  String normalized;
  normalized.reserve(text.length());

  size_t index = 0;
  while (index < text.length()) {
    const size_t before = index;
    uint32_t codepoint = 0;
    if (decodeUtf8Codepoint(text, index, codepoint)) {
      appendDisplayApproximation(normalized, codepoint);
      continue;
    }

    index = before + 1;
    normalized += static_cast<char>(text[before]);
  }

  String collapsed;
  collapsed.reserve(normalized.length());
  bool previousSpace = true;
  for (size_t i = 0; i < normalized.length(); ++i) {
    const uint8_t value = LatinText::byteValue(normalized[i]);
    if (value <= ' ' && !LatinText::isWordCharacter(value)) {
      if (!previousSpace) {
        collapsed += ' ';
        previousSpace = true;
      }
      continue;
    }

    collapsed += static_cast<char>(value);
    previousSpace = false;
  }

  if (!collapsed.isEmpty() && collapsed[collapsed.length() - 1] == ' ') {
    collapsed.remove(collapsed.length() - 1, 1);
  }
  return collapsed;
}

void appendNormalizedChar(String &target, char c) {
  if (c == '\r' || c == '\n' || c == '\t') {
    c = ' ';
  }

  if (isWhitespace(c)) {
    if (!target.isEmpty() && target[target.length() - 1] != ' ') {
      target += ' ';
    }
    return;
  }

  target += c;
}

String plainTextFromXmlFragment(const String &fragment) {
  String text;
  text.reserve(std::min<size_t>(fragment.length(), 160));

  for (size_t i = 0; i < fragment.length(); ++i) {
    const char c = fragment[i];
    if (c == '<') {
      const int tagEnd = fragment.indexOf('>', i + 1);
      if (tagEnd < 0) {
        break;
      }
      i = tagEnd;
      appendNormalizedChar(text, ' ');
      continue;
    }

    if (c == '&') {
      const int entityEnd = fragment.indexOf(';', i + 1);
      if (entityEnd > 0 && entityEnd - static_cast<int>(i) <= 12) {
        appendNormalizedChar(text, decodedEntityChar(fragment.substring(i + 1, entityEnd)));
        i = entityEnd;
        continue;
      }
    }

    appendNormalizedChar(text, c);
  }

  text.trim();
  return normalizeDisplayText(text);
}

bool hasReadableText(const String &token) {
  for (size_t i = 0; i < token.length(); ++i) {
    const uint8_t value = static_cast<uint8_t>(token[i]);
    if (std::isalnum(value) != 0 || value >= 0x80) {
      return true;
    }
  }
  return false;
}

bool writeBodyLine(File &output, const String &line, size_t &wordCount, size_t maxWords);

bool flushWordAlignedPrefix(File &output, String &line, size_t &wordCount, size_t maxWords) {
  line.trim();
  if (line.isEmpty()) {
    line = "";
    return true;
  }

  int split = static_cast<int>(line.length()) - 1;
  while (split >= 0 && !isWhitespace(line[split])) {
    --split;
  }

  if (split < 0) {
    return true;
  }

  String prefix = line.substring(0, split);
  String remainder = line.substring(split + 1);
  prefix.trim();
  remainder.trim();

  if (prefix.isEmpty()) {
    line = remainder;
    return true;
  }

  const bool keepGoing = writeBodyLine(output, prefix, wordCount, maxWords);
  line = remainder;
  return keepGoing;
}

bool writeBodyLine(File &output, const String &line, size_t &wordCount, size_t maxWords) {
  const String normalizedLine = normalizeDisplayText(line);
  String token;
  String outputLine;

  auto flushOutputLine = [&]() {
    if (outputLine.isEmpty()) {
      return;
    }
    if (outputLine.startsWith("@")) {
      output.print('@');
    }
    output.println(outputLine);
    outputLine = "";
  };

  for (size_t i = 0; i <= normalizedLine.length(); ++i) {
    if ((i & 0x7F) == 0) {
      serviceBackground();
    }

    const char c = i < normalizedLine.length() ? normalizedLine[i] : ' ';
    if (!isWhitespace(c)) {
      token += c;
      continue;
    }

    token.trim();
    if (!token.isEmpty() && hasReadableText(token)) {
      if (reachedWordLimit(wordCount, maxWords)) {
        flushOutputLine();
        return false;
      }

      if (outputLine.length() + token.length() + 1 > kOutputWrapWidth) {
        flushOutputLine();
      }

      if (!outputLine.isEmpty()) {
        outputLine += ' ';
      }
      outputLine += token;
      ++wordCount;
    }

    token = "";
  }

  flushOutputLine();

  return !reachedWordLimit(wordCount, maxWords);
}

String tagNameFromTag(const String &tag, bool &closing, bool &selfClosing) {
  closing = false;
  selfClosing = false;

  size_t position = 1;
  while (position < tag.length() && isWhitespace(tag[position])) {
    ++position;
  }
  if (position < tag.length() && tag[position] == '/') {
    closing = true;
    ++position;
  }
  while (position < tag.length() && isWhitespace(tag[position])) {
    ++position;
  }

  const size_t start = position;
  while (position < tag.length()) {
    const char c = tag[position];
    if (!(std::isalnum(static_cast<unsigned char>(c)) != 0 || c == ':' || c == '-' || c == '_')) {
      break;
    }
    ++position;
  }

  String name = tag.substring(start, position);
  name.toLowerCase();

  for (int i = tag.length() - 1; i >= 0; --i) {
    if (isWhitespace(tag[i]) || tag[i] == '>') {
      continue;
    }
    selfClosing = tag[i] == '/';
    break;
  }

  return name;
}

bool isSkipTag(const String &name) {
  return name == "head" || name == "script" || name == "style" || name == "svg" ||
         name == "math" || name == "nav";
}

bool isHeadingTag(const String &name) {
  return name.length() == 2 && name[0] == 'h' && name[1] >= '1' && name[1] <= '6';
}

bool isBlockTag(const String &name) {
  return name == "p" || name == "div" || name == "section" || name == "article" ||
         name == "blockquote" || name == "li" || name == "tr" || name == "br" ||
         name == "hr" || name == "dd" || name == "dt";
}

bool writeChapterMarker(File &output, const String &title, String &lastChapterTitle) {
  String cleaned = normalizeDisplayText(title);
  cleaned.trim();
  if (cleaned.isEmpty() || cleaned == lastChapterTitle) {
    return true;
  }

  output.print("@chapter ");
  output.println(cleaned);
  lastChapterTitle = cleaned;
  return true;
}

bool writeXhtmlAsRsvp(const String &html, File &output, size_t &wordCount, size_t maxWords,
                      String &lastChapterTitle, const EpubConverter::Options &options,
                      size_t itemIndex, size_t itemCount) {
  String line;
  String heading;
  bool inHeading = false;
  int skipDepth = 0;

  line.reserve(160);
  heading.reserve(80);

  auto flushLine = [&]() -> bool {
    line.trim();
    if (line.isEmpty()) {
      return true;
    }
    const bool keepGoing = writeBodyLine(output, line, wordCount, maxWords);
    line = "";
    return keepGoing;
  };

  for (size_t i = 0; i < html.length(); ++i) {
    if ((i & 0x3FF) == 0) {
      serviceBackground();
    }
    if ((i & 0x7FFF) == 0 && itemCount > 0 && html.length() > 0) {
      const int contentPercent =
          static_cast<int>((static_cast<uint32_t>(i) * 100UL) / html.length());
      const int itemPercent =
          static_cast<int>(((itemIndex * 100UL) + contentPercent) / itemCount);
      const int progressPercent = 25 + ((itemPercent * 70) / 100);
      const String detail = String(itemIndex + 1) + "/" + String(itemCount) + " " +
                            String(wordCount) + " words";
      reportProgress(options, "Parsing content", detail.c_str(), progressPercent);
    }

    const char c = html[i];

    if (c == '<') {
      if (startsWithAt(html, i, "<!--")) {
        const int commentEnd = html.indexOf("-->", i + 4);
        if (commentEnd < 0) {
          break;
        }
        i = static_cast<size_t>(commentEnd) + 2;
        continue;
      }

      const int tagEnd = html.indexOf('>', i + 1);
      if (tagEnd < 0) {
        break;
      }

      const String tag = html.substring(i, tagEnd + 1);
      bool closing = false;
      bool selfClosing = false;
      const String name = tagNameFromTag(tag, closing, selfClosing);

      if (name.isEmpty() || tag.startsWith("<!") || tag.startsWith("<?")) {
        i = tagEnd;
        continue;
      }

      if (skipDepth > 0) {
        if (!closing && isSkipTag(name) && !selfClosing) {
          ++skipDepth;
        } else if (closing && isSkipTag(name)) {
          --skipDepth;
        }
        i = tagEnd;
        continue;
      }

      if (isSkipTag(name) && !closing && !selfClosing) {
        if (!flushLine()) {
          return false;
        }
        skipDepth = 1;
        i = tagEnd;
        continue;
      }

      if (isHeadingTag(name)) {
        if (closing) {
          inHeading = false;
          const String cleanedHeading = plainTextFromXmlFragment(heading);
          if (!writeChapterMarker(output, cleanedHeading, lastChapterTitle)) {
            return false;
          }
          heading = "";
        } else if (!selfClosing) {
          if (!flushLine()) {
            return false;
          }
          inHeading = true;
          heading = "";
        }
        i = tagEnd;
        continue;
      }

      if (isBlockTag(name) && (closing || name == "br" || name == "hr" || name == "li")) {
        if (!flushLine()) {
          return false;
        }
      } else if (isBlockTag(name)) {
        appendNormalizedChar(line, ' ');
      }

      i = tagEnd;
      continue;
    }

    char decoded = c;
    if (c == '&') {
      const int entityEnd = html.indexOf(';', i + 1);
      if (entityEnd > 0 && entityEnd - static_cast<int>(i) <= 12) {
        decoded = decodedEntityChar(html.substring(i + 1, entityEnd));
        i = entityEnd;
      }
    }

    if (skipDepth > 0) {
      continue;
    }
    if (inHeading) {
      appendNormalizedChar(heading, decoded);
      continue;
    }

    appendNormalizedChar(line, decoded);
    if (line.length() > kBufferedTextFlushThreshold) {
      if (!flushWordAlignedPrefix(output, line, wordCount, maxWords)) {
        return false;
      }
    }
  }

  return flushLine();
}

class XhtmlRsvpStreamWriter {
 public:
  XhtmlRsvpStreamWriter(File &output, size_t &wordCount, size_t maxWords,
                        String &lastChapterTitle)
      : output_(output),
        wordCount_(wordCount),
        maxWords_(maxWords),
        lastChapterTitle_(lastChapterTitle) {
    line_.reserve(160);
    heading_.reserve(80);
    tag_.reserve(96);
    entity_.reserve(16);
  }

  bool write(const uint8_t *data, size_t length) {
    for (size_t i = 0; i < length; ++i) {
      if ((i & 0x3FF) == 0) {
        serviceBackground();
      }
      if (!processChar(static_cast<char>(data[i]))) {
        return false;
      }
    }

    return true;
  }

  bool finish() {
    mode_ = Mode::Text;
    return flushLine();
  }

  bool reachedWordLimit() const { return reachedWordLimit_; }

 private:
  enum class Mode {
    Text,
    Tag,
    Entity,
    Comment,
  };

  bool flushLine() {
    line_.trim();
    if (line_.isEmpty()) {
      return true;
    }

    const bool keepGoing = writeBodyLine(output_, line_, wordCount_, maxWords_);
    line_ = "";
    if (!keepGoing) {
      reachedWordLimit_ = true;
    }
    return keepGoing;
  }

  void appendToActiveText(char c) {
    if (inHeading_) {
      appendNormalizedChar(heading_, c);
      return;
    }

    appendNormalizedChar(line_, c);
  }

  bool processDecodedText(char c) {
    if (skipDepth_ > 0) {
      return true;
    }

    appendToActiveText(c);
    if (!inHeading_ && line_.length() > kBufferedTextFlushThreshold) {
      return flushWordAlignedPrefix(output_, line_, wordCount_, maxWords_);
    }

    return true;
  }

  bool processTextChar(char c) {
    if (c == '<') {
      tag_ = "<";
      mode_ = Mode::Tag;
      return true;
    }

    if (c == '&') {
      if (skipDepth_ > 0) {
        return true;
      }
      entity_ = "";
      mode_ = Mode::Entity;
      return true;
    }

    return processDecodedText(c);
  }

  bool processTag(const String &tag) {
    bool closing = false;
    bool selfClosing = false;
    const String name = tagNameFromTag(tag, closing, selfClosing);

    if (name.isEmpty() || tag.startsWith("<!") || tag.startsWith("<?")) {
      return true;
    }

    if (skipDepth_ > 0) {
      if (!closing && isSkipTag(name) && !selfClosing) {
        ++skipDepth_;
      } else if (closing && isSkipTag(name)) {
        --skipDepth_;
      }
      return true;
    }

    if (isSkipTag(name) && !closing && !selfClosing) {
      if (!flushLine()) {
        return false;
      }
      skipDepth_ = 1;
      return true;
    }

    if (isHeadingTag(name)) {
      if (closing) {
        inHeading_ = false;
        const String cleanedHeading = plainTextFromXmlFragment(heading_);
        if (!writeChapterMarker(output_, cleanedHeading, lastChapterTitle_)) {
          return false;
        }
        heading_ = "";
      } else if (!selfClosing) {
        if (!flushLine()) {
          return false;
        }
        inHeading_ = true;
        heading_ = "";
      }
      return true;
    }

    if (isBlockTag(name) && (closing || name == "br" || name == "hr" || name == "li")) {
      return flushLine();
    }
    if (isBlockTag(name)) {
      appendNormalizedChar(line_, ' ');
    }

    return true;
  }

  bool processEntityChar(char c) {
    if (c == ';') {
      mode_ = Mode::Text;
      return processDecodedText(decodedEntityChar(entity_));
    }

    if (c == '<') {
      mode_ = Mode::Text;
      if (!processDecodedText(' ')) {
        return false;
      }
      return processTextChar(c);
    }

    if (entity_.length() >= kMaxEntityChars || isWhitespace(c)) {
      mode_ = Mode::Text;
      return processDecodedText(' ');
    }

    entity_ += c;
    return true;
  }

  bool processCommentChar(char c) {
    commentTail_ += c;
    if (commentTail_.length() > 3) {
      commentTail_.remove(0, commentTail_.length() - 3);
    }

    if (commentTail_ == "-->") {
      commentTail_ = "";
      mode_ = Mode::Text;
    }

    return true;
  }

  bool processChar(char c) {
    switch (mode_) {
      case Mode::Text:
        return processTextChar(c);
      case Mode::Entity:
        return processEntityChar(c);
      case Mode::Comment:
        return processCommentChar(c);
      case Mode::Tag:
        tag_ += c;
        if (tag_ == "<!--") {
          tag_ = "";
          commentTail_ = "";
          mode_ = Mode::Comment;
          return true;
        }
        if (tag_.length() > kMaxTagChars) {
          tag_ = "";
          mode_ = Mode::Text;
          return processDecodedText(' ');
        }
        if (c == '>') {
          const String completedTag = tag_;
          tag_ = "";
          mode_ = Mode::Text;
          return processTag(completedTag);
        }
        return true;
    }

    return true;
  }

  File &output_;
  size_t &wordCount_;
  size_t maxWords_;
  String &lastChapterTitle_;
  String line_;
  String heading_;
  String tag_;
  String entity_;
  String commentTail_;
  Mode mode_ = Mode::Text;
  bool inHeading_ = false;
  bool reachedWordLimit_ = false;
  int skipDepth_ = 0;
};

bool isContentDocument(const ManifestItem &item) {
  const String mediaType = toLowerCopy(item.mediaType);
  const String path = toLowerCopy(item.path);
  return mediaType == "application/xhtml+xml" || mediaType == "text/html" ||
         path.endsWith(".xhtml") || path.endsWith(".html") || path.endsWith(".htm");
}

String parseRootfilePath(const String &containerXml) {
  int position = 0;
  while (position >= 0) {
    position = containerXml.indexOf("<rootfile", position);
    if (position < 0) {
      break;
    }

    const int end = containerXml.indexOf('>', position);
    if (end < 0) {
      break;
    }

    const String tag = containerXml.substring(position, end + 1);
    const String path = attributeValue(tag, "full-path");
    if (!path.isEmpty()) {
      return normalizeZipName(path);
    }

    position = end + 1;
  }

  return "";
}

String parseDcMetadata(const String &opfXml, const char *tagName) {
  const String openTag = String("<dc:") + tagName;
  const String closeTag = String("</dc:") + tagName;
  int position = 0;
  while (position >= 0) {
    position = opfXml.indexOf(openTag, position);
    if (position < 0) {
      break;
    }

    const int openEnd = opfXml.indexOf('>', position);
    if (openEnd < 0) {
      break;
    }
    const int closeStart = opfXml.indexOf(closeTag, openEnd + 1);
    if (closeStart < 0) {
      break;
    }

    const String value = plainTextFromXmlFragment(opfXml.substring(openEnd + 1, closeStart));
    if (!value.isEmpty()) {
      return value;
    }

    position = closeStart + 1;
  }

  return "";
}

String parseBookTitle(const String &opfXml) { return parseDcMetadata(opfXml, "title"); }

String parseBookAuthor(const String &opfXml) { return parseDcMetadata(opfXml, "creator"); }

std::vector<ManifestItem> parseManifestItems(const String &opfXml, const String &opfBaseDir) {
  std::vector<ManifestItem> items;
  int position = 0;

  while (position >= 0) {
    position = opfXml.indexOf("<item", position);
    if (position < 0) {
      break;
    }

    const int afterName = position + 5;
    if (static_cast<size_t>(afterName) < opfXml.length() &&
        !isWhitespace(opfXml[afterName]) && opfXml[afterName] != '/' &&
        opfXml[afterName] != '>') {
      position = afterName;
      continue;
    }

    const int end = opfXml.indexOf('>', position);
    if (end < 0) {
      break;
    }

    const String tag = opfXml.substring(position, end + 1);
    ManifestItem item;
    item.id = attributeValue(tag, "id");
    item.path = resolveZipPath(opfBaseDir, attributeValue(tag, "href"));
    item.mediaType = attributeValue(tag, "media-type");

    if (!item.id.isEmpty() && !item.path.isEmpty()) {
      items.push_back(item);
    }

    position = end + 1;
  }

  return items;
}

std::vector<String> parseSpineIds(const String &opfXml) {
  std::vector<String> ids;
  int position = 0;

  while (position >= 0) {
    position = opfXml.indexOf("<itemref", position);
    if (position < 0) {
      break;
    }

    const int end = opfXml.indexOf('>', position);
    if (end < 0) {
      break;
    }

    const String tag = opfXml.substring(position, end + 1);
    const String idref = attributeValue(tag, "idref");
    if (!idref.isEmpty()) {
      ids.push_back(idref);
    }

    position = end + 1;
  }

  return ids;
}

const ManifestItem *findManifestItem(const std::vector<ManifestItem> &items, const String &id) {
  for (size_t i = 0; i < items.size(); ++i) {
    if (items[i].id == id) {
      return &items[i];
    }
  }
  return nullptr;
}

void reportContentProgress(const EpubConverter::Options &options, size_t itemIndex,
                           size_t itemCount, uint32_t bytesRead, uint32_t totalBytes,
                           size_t wordCount) {
  if (itemCount == 0 || totalBytes == 0) {
    return;
  }

  const uint32_t cappedBytes = std::min(bytesRead, totalBytes);
  const int contentPercent = static_cast<int>((cappedBytes * 100ULL) / totalBytes);
  const int itemPercent = static_cast<int>(((itemIndex * 100ULL) + contentPercent) / itemCount);
  const int progressPercent = 25 + ((itemPercent * 70) / 100);
  const String detail = String(itemIndex + 1) + "/" + String(itemCount) + " " +
                        String(wordCount) + " words";
  reportProgress(options, "Extracting content", detail.c_str(), progressPercent);
}

class ZipArchive {
 public:
  bool open(const String &path) {
    archivePath_ = path;
    file_ = SD_MMC.open(path);
    if (!file_ || file_.isDirectory()) {
      Serial.printf("[epub-zip] Open failed: %s\n", path.c_str());
      close();
      return false;
    }

    Serial.printf("[epub-zip] Opened archive: %s size=%lu\n", path.c_str(),
                  static_cast<unsigned long>(file_.size()));
    if (!readCentralDirectory()) {
      Serial.printf("[epub-zip] Central directory read failed: %s\n", path.c_str());
      close();
      return false;
    }
    Serial.printf("[epub-zip] Archive ready: %u file entries\n",
                  static_cast<unsigned int>(entries_.size()));
    logArchiveHints("open");
    return true;
  }

  void close() {
    if (file_) {
      file_.close();
    }
    entries_.clear();
  }

  const ZipEntry *find(const String &name) const {
    const String normalized = normalizeZipName(name);
    for (size_t i = 0; i < entries_.size(); ++i) {
      if (entries_[i].name == normalized) {
        return &entries_[i];
      }
    }

    const String lowered = toLowerCopy(normalized);
    for (size_t i = 0; i < entries_.size(); ++i) {
      if (toLowerCopy(entries_[i].name) == lowered) {
        Serial.printf("[epub-zip] Case-insensitive ZIP match: requested=%s actual=%s\n",
                      normalized.c_str(), entries_[i].name.c_str());
        return &entries_[i];
      }
    }

    Serial.printf("[epub-zip] Entry not found: %s\n", normalized.c_str());
    logArchiveHints("missing entry");
    return nullptr;
  }

  bool extractToString(const String &name, String &output, size_t maxBytes) {
    Serial.printf("[epub-zip] Request string entry: %s\n", name.c_str());
    Serial.flush();
    const ZipEntry *entry = find(name);
    if (entry == nullptr) {
      return false;
    }
    return extractToString(*entry, output, maxBytes);
  }

  ContentExtractStatus extractContentToRsvp(const String &name, File &output, size_t &wordCount,
                                            size_t maxWords, String &lastChapterTitle,
                                            const EpubConverter::Options &options,
                                            size_t itemIndex, size_t itemCount) {
    const ZipEntry *entry = find(name);
    if (entry == nullptr) {
      Serial.printf("[epub-zip] Content entry not found: %s\n", name.c_str());
      return ContentExtractStatus::Failed;
    }
    return extractContentToRsvp(*entry, output, wordCount, maxWords, lastChapterTitle, options,
                                itemIndex, itemCount);
  }

 private:
  void logArchiveHints(const char *reason) const {
    Serial.printf("[epub-zip] Archive hints (%s): entries=%u\n",
                  reason == nullptr ? "" : reason, static_cast<unsigned int>(entries_.size()));

    size_t printed = 0;
    for (size_t i = 0; i < entries_.size() && printed < 10; ++i) {
      Serial.printf("[epub-zip]   entry[%u] %s method=%u flags=0x%04x c=%lu u=%lu local=%lu\n",
                    static_cast<unsigned int>(i), entries_[i].name.c_str(), entries_[i].method,
                    entries_[i].flags, static_cast<unsigned long>(entries_[i].compressedSize),
                    static_cast<unsigned long>(entries_[i].uncompressedSize),
                    static_cast<unsigned long>(entries_[i].localHeaderOffset));
      ++printed;
    }

    size_t hinted = 0;
    for (size_t i = 0; i < entries_.size() && hinted < 20; ++i) {
      if (!isArchiveHintEntry(entries_[i].name)) {
        continue;
      }
      Serial.printf("[epub-zip]   hint[%u] %s method=%u flags=0x%04x c=%lu u=%lu local=%lu\n",
                    static_cast<unsigned int>(i), entries_[i].name.c_str(), entries_[i].method,
                    entries_[i].flags, static_cast<unsigned long>(entries_[i].compressedSize),
                    static_cast<unsigned long>(entries_[i].uncompressedSize),
                    static_cast<unsigned long>(entries_[i].localHeaderOffset));
      ++hinted;
    }
  }

  bool readCentralDirectory() {
    const uint32_t fileSize = static_cast<uint32_t>(file_.size());
    if (fileSize < 22) {
      Serial.printf("[epub-zip] File too small for ZIP EOCD: %lu\n",
                    static_cast<unsigned long>(fileSize));
      return false;
    }

    const size_t tailSize =
        fileSize < kZipEocdMaxSearch ? static_cast<size_t>(fileSize) : kZipEocdMaxSearch;
    uint8_t *tail = static_cast<uint8_t *>(allocateBuffer(tailSize));
    if (tail == nullptr) {
      Serial.printf("[epub-zip] No memory for EOCD tail buffer: %u bytes\n",
                    static_cast<unsigned int>(tailSize));
      return false;
    }

    const uint32_t tailOffset = fileSize - static_cast<uint32_t>(tailSize);
    Serial.printf("[epub-zip] Searching EOCD: fileSize=%lu tailOffset=%lu tailSize=%u\n",
                  static_cast<unsigned long>(fileSize), static_cast<unsigned long>(tailOffset),
                  static_cast<unsigned int>(tailSize));
    bool ok = file_.seek(tailOffset) && readExact(file_, tail, tailSize);
    int eocdIndex = -1;
    if (ok) {
      for (int i = static_cast<int>(tailSize) - 22; i >= 0; --i) {
        if (readLe32(tail + i) == kZipEocdSignature) {
          eocdIndex = i;
          break;
        }
      }
    }

    if (eocdIndex < 0) {
      Serial.printf("[epub-zip] EOCD signature not found (tailRead=%s)\n", ok ? "yes" : "no");
      freeBuffer(tail);
      return false;
    }

    const uint16_t diskNumber = readLe16(tail + eocdIndex + 4);
    const uint16_t directoryDisk = readLe16(tail + eocdIndex + 6);
    const uint16_t entryCount = readLe16(tail + eocdIndex + 10);
    const uint32_t centralDirectoryOffset = readLe32(tail + eocdIndex + 16);
    const uint32_t centralDirectorySize = readLe32(tail + eocdIndex + 12);
    freeBuffer(tail);

    Serial.printf(
        "[epub-zip] EOCD found: eocdOffset=%lu entries=%u cdOffset=%lu cdSize=%lu disk=%u "
        "dirDisk=%u\n",
        static_cast<unsigned long>(tailOffset + static_cast<uint32_t>(eocdIndex)), entryCount,
        static_cast<unsigned long>(centralDirectoryOffset),
        static_cast<unsigned long>(centralDirectorySize), diskNumber, directoryDisk);

    if (diskNumber != 0 || directoryDisk != 0 || entryCount == 0 ||
        entryCount > kMaxZipEntries) {
      Serial.printf("[epub] Unsupported ZIP directory entry count: %u\n", entryCount);
      return false;
    }

    entries_.clear();
    entries_.reserve(entryCount);
    if (!file_.seek(centralDirectoryOffset)) {
      Serial.printf("[epub-zip] Could not seek to central directory offset=%lu\n",
                    static_cast<unsigned long>(centralDirectoryOffset));
      return false;
    }

    for (uint16_t i = 0; i < entryCount; ++i) {
      if ((i & 0x1F) == 0) {
        serviceBackground();
      }

      uint8_t header[46];
      if (!readExact(file_, header, sizeof(header)) ||
          readLe32(header) != kZipCentralFileSignature) {
        Serial.printf("[epub-zip] Bad central header at index=%u pos=%lu\n", i,
                      static_cast<unsigned long>(file_.position()));
        return false;
      }

      const uint16_t fileNameLength = readLe16(header + 28);
      const uint16_t extraLength = readLe16(header + 30);
      const uint16_t commentLength = readLe16(header + 32);
      if (fileNameLength == 0 || fileNameLength > kMaxZipNameLength) {
        Serial.printf("[epub] Unsupported ZIP filename length: %u\n", fileNameLength);
        return false;
      }

      char *nameBuffer = static_cast<char *>(allocateBuffer(fileNameLength + 1));
      if (nameBuffer == nullptr) {
        Serial.printf("[epub-zip] No memory for filename buffer: %u bytes\n", fileNameLength + 1);
        return false;
      }

      const bool nameRead =
          readExact(file_, reinterpret_cast<uint8_t *>(nameBuffer), fileNameLength);
      nameBuffer[fileNameLength] = '\0';

      ZipEntry entry;
      entry.name = normalizeZipName(String(nameBuffer));
      entry.method = readLe16(header + 10);
      entry.flags = readLe16(header + 8);
      entry.compressedSize = readLe32(header + 20);
      entry.uncompressedSize = readLe32(header + 24);
      entry.localHeaderOffset = readLe32(header + 42);
      freeBuffer(nameBuffer);

      if (!nameRead) {
        return false;
      }

      const uint32_t nextPosition =
          static_cast<uint32_t>(file_.position()) + extraLength + commentLength;
      if (!file_.seek(nextPosition)) {
        Serial.printf("[epub-zip] Could not seek past central extras for %s next=%lu\n",
                      entry.name.c_str(), static_cast<unsigned long>(nextPosition));
        return false;
      }

      if (!entry.name.endsWith("/")) {
        entries_.push_back(entry);
      }
    }

    Serial.printf("[epub-zip] Central directory parsed: kept=%u rawEntries=%u\n",
                  static_cast<unsigned int>(entries_.size()), entryCount);
    return true;
  }

  bool extractToString(const ZipEntry &entry, String &output, size_t maxBytes) {
    output = "";

    Serial.printf("[epub-zip] Extract string: %s method=%u flags=0x%04x c=%lu u=%lu max=%u\n",
                  entry.name.c_str(), entry.method, entry.flags,
                  static_cast<unsigned long>(entry.compressedSize),
                  static_cast<unsigned long>(entry.uncompressedSize),
                  static_cast<unsigned int>(maxBytes));
    Serial.flush();

    if (entry.uncompressedSize == 0 || entry.uncompressedSize > maxBytes ||
        entry.compressedSize == 0 || entry.compressedSize > maxBytes) {
      Serial.printf("[epub] Skipping %s (%lu compressed, %lu uncompressed bytes)\n",
                    entry.name.c_str(), static_cast<unsigned long>(entry.compressedSize),
                    static_cast<unsigned long>(entry.uncompressedSize));
      return false;
    }

    uint8_t localHeader[30];
    if (!file_.seek(entry.localHeaderOffset)) {
      Serial.printf("[epub-zip] Could not seek to local header: %s offset=%lu\n",
                    entry.name.c_str(), static_cast<unsigned long>(entry.localHeaderOffset));
      return false;
    }
    if (!readExact(file_, localHeader, sizeof(localHeader))) {
      Serial.printf("[epub-zip] Could not read local header: %s\n", entry.name.c_str());
      return false;
    }
    const uint32_t localSignature = readLe32(localHeader);
    if (localSignature != kZipLocalFileSignature) {
      Serial.printf("[epub-zip] Bad local signature for %s signature=0x%08lx\n",
                    entry.name.c_str(), static_cast<unsigned long>(localSignature));
      return false;
    }

    const uint16_t fileNameLength = readLe16(localHeader + 26);
    const uint16_t extraLength = readLe16(localHeader + 28);
    const uint32_t dataOffset = entry.localHeaderOffset + sizeof(localHeader) + fileNameLength +
                                extraLength;
    Serial.printf("[epub-zip] Local data: %s nameLen=%u extraLen=%u dataOffset=%lu\n",
                  entry.name.c_str(), fileNameLength, extraLength,
                  static_cast<unsigned long>(dataOffset));
    if (!file_.seek(dataOffset)) {
      Serial.printf("[epub-zip] Could not seek to data: %s offset=%lu\n", entry.name.c_str(),
                    static_cast<unsigned long>(dataOffset));
      return false;
    }

    if (!output.reserve(static_cast<unsigned int>(entry.uncompressedSize + 1))) {
      Serial.printf("[epub-zip] No memory to reserve string for %s (%lu bytes)\n",
                    entry.name.c_str(), static_cast<unsigned long>(entry.uncompressedSize));
      return false;
    }

    bool ok = false;
    uint32_t totalOutputBytes = 0;

    auto appendBytes = [&](const uint8_t *data, size_t length) -> bool {
      if (length == 0) {
        return true;
      }
      if (totalOutputBytes + length > maxBytes) {
        Serial.printf("[epub-zip] String extraction exceeded limit for %s\n",
                      entry.name.c_str());
        return false;
      }
      if (!output.concat(reinterpret_cast<const char *>(data), static_cast<unsigned int>(length))) {
        Serial.printf("[epub-zip] String append failed for %s length=%u\n", entry.name.c_str(),
                      static_cast<unsigned int>(length));
        return false;
      }
      totalOutputBytes += static_cast<uint32_t>(length);
      return true;
    };

    if (entry.method == kZipStored) {
      Serial.printf("[epub-zip] Reading stored string payload: %s\n", entry.name.c_str());
      uint8_t *buffer = static_cast<uint8_t *>(allocateInternalBuffer(kReadChunkBytes));
      if (buffer == nullptr) {
        Serial.printf("[epub-zip] No internal buffer for stored string: %s\n",
                      entry.name.c_str());
        return false;
      }

      uint32_t remaining = entry.uncompressedSize;
      ok = true;
      while (remaining > 0) {
        const size_t chunk = std::min(kReadChunkBytes, static_cast<size_t>(remaining));
        if (!readExact(file_, buffer, chunk) || !appendBytes(buffer, chunk)) {
          ok = false;
          break;
        }
        remaining -= static_cast<uint32_t>(chunk);
        serviceBackground();
      }
      freeBuffer(buffer);
    } else if (entry.method == kZipDeflated) {
      Serial.printf("[epub-zip] Streaming inflate string payload: %s\n", entry.name.c_str());
      uint8_t *inputBuffer = static_cast<uint8_t *>(allocateInternalBuffer(kInflateInputChunkBytes));
      uint8_t *dictionary = static_cast<uint8_t *>(allocateInternalBuffer(TINFL_LZ_DICT_SIZE));
      tinfl_decompressor *inflator =
          static_cast<tinfl_decompressor *>(allocateInternalBuffer(sizeof(tinfl_decompressor)));
      if (inputBuffer == nullptr || dictionary == nullptr || inflator == nullptr) {
        Serial.printf(
            "[epub-zip] No internal inflate buffers for string: %s input=%s dict=%s inflator=%s\n",
                      entry.name.c_str(), inputBuffer == nullptr ? "no" : "yes",
            dictionary == nullptr ? "no" : "yes", inflator == nullptr ? "no" : "yes");
      } else {
        tinfl_init(inflator);

        uint32_t compressedRemaining = entry.compressedSize;
        size_t inputAvailable = 0;
        size_t inputOffset = 0;
        tinfl_status status = TINFL_STATUS_NEEDS_MORE_INPUT;
        ok = true;

        while (status > TINFL_STATUS_DONE) {
          if (inputAvailable == 0 && compressedRemaining > 0) {
            const size_t chunk =
                std::min(kInflateInputChunkBytes, static_cast<size_t>(compressedRemaining));
            Serial.printf("[epub-zip] Reading deflate chunk: %s chunk=%u remaining=%lu\n",
                          entry.name.c_str(), static_cast<unsigned int>(chunk),
                          static_cast<unsigned long>(compressedRemaining));
            if (!readExact(file_, inputBuffer, chunk)) {
              Serial.printf("[epub-zip] Could not read deflated string payload: %s\n",
                            entry.name.c_str());
              ok = false;
              break;
            }

            compressedRemaining -= static_cast<uint32_t>(chunk);
            inputAvailable = chunk;
            inputOffset = 0;
          }

          const size_t dictionaryOffset = totalOutputBytes & (TINFL_LZ_DICT_SIZE - 1);
          uint8_t *writeCursor = dictionary + dictionaryOffset;
          size_t inSize = inputAvailable;
          size_t outSize = TINFL_LZ_DICT_SIZE - dictionaryOffset;
          const mz_uint32 flags = compressedRemaining > 0 ? TINFL_FLAG_HAS_MORE_INPUT : 0;

          status = tinfl_decompress(inflator, inputBuffer + inputOffset, &inSize, dictionary,
                                    writeCursor, &outSize, flags);
          inputAvailable -= inSize;
          inputOffset += inSize;

          if (outSize > 0 && !appendBytes(writeCursor, outSize)) {
            ok = false;
            break;
          }

          serviceBackground();

          if (status < TINFL_STATUS_DONE) {
            Serial.printf("[epub-zip] Streaming inflate failed for %s status=%d\n",
                          entry.name.c_str(), static_cast<int>(status));
            ok = false;
            break;
          }

          if (inSize == 0 && outSize == 0 && status != TINFL_STATUS_DONE &&
              inputAvailable == 0 && compressedRemaining == 0) {
            Serial.printf("[epub-zip] Streaming inflate stalled for %s status=%d\n",
                          entry.name.c_str(), static_cast<int>(status));
            ok = false;
            break;
          }
        }
      }

      freeBuffer(inputBuffer);
      freeBuffer(dictionary);
      freeBuffer(inflator);
    } else {
      Serial.printf("[epub] Unsupported ZIP method %u for %s\n", entry.method,
                    entry.name.c_str());
    }

    if (ok && totalOutputBytes != entry.uncompressedSize) {
      Serial.printf("[epub-zip] String inflate size mismatch for %s (%lu of %lu bytes)\n",
                    entry.name.c_str(), static_cast<unsigned long>(totalOutputBytes),
                    static_cast<unsigned long>(entry.uncompressedSize));
      ok = false;
    }

    if (ok) {
      Serial.printf("[epub-zip] Extracted string OK: %s textLen=%u\n", entry.name.c_str(),
                    static_cast<unsigned int>(output.length()));
    }

    return ok;
  }

  ContentExtractStatus extractContentToRsvp(const ZipEntry &entry, File &output, size_t &wordCount,
                                            size_t maxWords, String &lastChapterTitle,
                                            const EpubConverter::Options &options,
                                            size_t itemIndex, size_t itemCount) {
    Serial.printf("[epub-zip] Extract content: %s method=%u flags=0x%04x c=%lu u=%lu\n",
                  entry.name.c_str(), entry.method, entry.flags,
                  static_cast<unsigned long>(entry.compressedSize),
                  static_cast<unsigned long>(entry.uncompressedSize));

    if (entry.uncompressedSize == 0 || entry.compressedSize == 0 ||
        entry.uncompressedSize > options.maxContentBytes ||
        entry.compressedSize > options.maxContentBytes) {
      Serial.printf(
          "[epub] Skipping oversized content %s (%lu compressed, %lu uncompressed bytes)\n",
          entry.name.c_str(), static_cast<unsigned long>(entry.compressedSize),
          static_cast<unsigned long>(entry.uncompressedSize));
      return ContentExtractStatus::Unsupported;
    }

    uint8_t localHeader[30];
    if (!file_.seek(entry.localHeaderOffset)) {
      Serial.printf("[epub-zip] Could not seek to content local header: %s offset=%lu\n",
                    entry.name.c_str(), static_cast<unsigned long>(entry.localHeaderOffset));
      return ContentExtractStatus::Failed;
    }
    if (!readExact(file_, localHeader, sizeof(localHeader))) {
      Serial.printf("[epub-zip] Could not read content local header: %s\n", entry.name.c_str());
      return ContentExtractStatus::Failed;
    }
    const uint32_t localSignature = readLe32(localHeader);
    if (localSignature != kZipLocalFileSignature) {
      Serial.printf("[epub-zip] Bad content local signature for %s signature=0x%08lx\n",
                    entry.name.c_str(), static_cast<unsigned long>(localSignature));
      return ContentExtractStatus::Failed;
    }

    const uint16_t fileNameLength = readLe16(localHeader + 26);
    const uint16_t extraLength = readLe16(localHeader + 28);
    const uint32_t dataOffset = entry.localHeaderOffset + sizeof(localHeader) + fileNameLength +
                                extraLength;
    Serial.printf("[epub-zip] Content data: %s nameLen=%u extraLen=%u dataOffset=%lu\n",
                  entry.name.c_str(), fileNameLength, extraLength,
                  static_cast<unsigned long>(dataOffset));
    if (!file_.seek(dataOffset)) {
      Serial.printf("[epub-zip] Could not seek to content data: %s offset=%lu\n",
                    entry.name.c_str(), static_cast<unsigned long>(dataOffset));
      return ContentExtractStatus::Failed;
    }

    XhtmlRsvpStreamWriter writer(output, wordCount, maxWords, lastChapterTitle);
    uint32_t totalOutputBytes = 0;
    uint32_t lastProgressBytes = 0;

    auto finishWriter = [&]() -> ContentExtractStatus {
      if (!writer.finish()) {
        return writer.reachedWordLimit() ? ContentExtractStatus::WordLimitReached
                                         : ContentExtractStatus::Failed;
      }
      return ContentExtractStatus::Complete;
    };

    auto reportMaybe = [&](bool force) {
      if (!force && totalOutputBytes - lastProgressBytes < 32UL * 1024UL) {
        return;
      }
      lastProgressBytes = totalOutputBytes;
      reportContentProgress(options, itemIndex, itemCount, totalOutputBytes,
                            entry.uncompressedSize, wordCount);
    };

    if (entry.method == kZipStored) {
      uint8_t *buffer = static_cast<uint8_t *>(allocateInternalBuffer(kReadChunkBytes));
      if (buffer == nullptr) {
        Serial.printf("[epub] No internal memory for stored content buffer: %s\n",
                      entry.name.c_str());
        return ContentExtractStatus::Failed;
      }

      uint32_t remaining = entry.uncompressedSize;
      while (remaining > 0) {
        const size_t chunk =
            std::min(kReadChunkBytes, static_cast<size_t>(remaining));
        if (!readExact(file_, buffer, chunk)) {
          Serial.printf("[epub-zip] Stored content read failed: %s remaining=%lu\n",
                        entry.name.c_str(), static_cast<unsigned long>(remaining));
          freeBuffer(buffer);
          return ContentExtractStatus::Failed;
        }

        totalOutputBytes += static_cast<uint32_t>(chunk);
        if (!writer.write(buffer, chunk)) {
          freeBuffer(buffer);
          return writer.reachedWordLimit() ? ContentExtractStatus::WordLimitReached
                                           : ContentExtractStatus::Failed;
        }

        remaining -= static_cast<uint32_t>(chunk);
        reportMaybe(false);
      }

      freeBuffer(buffer);
      reportMaybe(true);
      return finishWriter();
    }

    if (entry.method != kZipDeflated) {
      Serial.printf("[epub] Unsupported ZIP method %u for %s\n", entry.method,
                    entry.name.c_str());
      return ContentExtractStatus::Unsupported;
    }

    uint8_t *inputBuffer = static_cast<uint8_t *>(allocateInternalBuffer(kInflateInputChunkBytes));
    uint8_t *dictionary = static_cast<uint8_t *>(allocateInternalBuffer(TINFL_LZ_DICT_SIZE));
    tinfl_decompressor *inflator =
        static_cast<tinfl_decompressor *>(allocateInternalBuffer(sizeof(tinfl_decompressor)));
    if (inputBuffer == nullptr || dictionary == nullptr || inflator == nullptr) {
      Serial.printf(
          "[epub] No internal memory for streaming inflate buffers: %s input=%s dict=%s "
          "inflator=%s\n",
          entry.name.c_str(), inputBuffer == nullptr ? "no" : "yes",
          dictionary == nullptr ? "no" : "yes", inflator == nullptr ? "no" : "yes");
      freeBuffer(inputBuffer);
      freeBuffer(dictionary);
      freeBuffer(inflator);
      return ContentExtractStatus::Failed;
    }

    tinfl_init(inflator);

    uint32_t compressedRemaining = entry.compressedSize;
    size_t inputAvailable = 0;
    size_t inputOffset = 0;
    tinfl_status status = TINFL_STATUS_NEEDS_MORE_INPUT;
    ContentExtractStatus result = ContentExtractStatus::Complete;

    while (status > TINFL_STATUS_DONE) {
      if (inputAvailable == 0 && compressedRemaining > 0) {
        const size_t chunk =
            std::min(kInflateInputChunkBytes, static_cast<size_t>(compressedRemaining));
        if (!readExact(file_, inputBuffer, chunk)) {
          Serial.printf("[epub-zip] Deflated content read failed: %s remaining=%lu\n",
                        entry.name.c_str(), static_cast<unsigned long>(compressedRemaining));
          result = ContentExtractStatus::Failed;
          break;
        }

        compressedRemaining -= static_cast<uint32_t>(chunk);
        inputAvailable = chunk;
        inputOffset = 0;
      }

      const size_t dictionaryOffset = totalOutputBytes & (TINFL_LZ_DICT_SIZE - 1);
      uint8_t *writeCursor = dictionary + dictionaryOffset;
      size_t inSize = inputAvailable;
      size_t outSize = TINFL_LZ_DICT_SIZE - dictionaryOffset;
      const mz_uint32 flags = compressedRemaining > 0 ? TINFL_FLAG_HAS_MORE_INPUT : 0;

      status = tinfl_decompress(inflator, inputBuffer + inputOffset, &inSize, dictionary,
                                writeCursor, &outSize, flags);
      inputAvailable -= inSize;
      inputOffset += inSize;

      if (outSize > 0) {
        totalOutputBytes += static_cast<uint32_t>(outSize);
        if (!writer.write(writeCursor, outSize)) {
          result = writer.reachedWordLimit() ? ContentExtractStatus::WordLimitReached
                                             : ContentExtractStatus::Failed;
          break;
        }
        reportMaybe(false);
      }

      serviceBackground();

      if (status < TINFL_STATUS_DONE) {
        Serial.printf("[epub] Inflate failed for %s status=%d\n", entry.name.c_str(),
                      static_cast<int>(status));
        result = ContentExtractStatus::Failed;
        break;
      }

      if (inSize == 0 && outSize == 0 && status != TINFL_STATUS_DONE &&
          inputAvailable == 0 && compressedRemaining == 0) {
        Serial.printf("[epub] Inflate stalled for %s status=%d\n", entry.name.c_str(),
                      static_cast<int>(status));
        result = ContentExtractStatus::Failed;
        break;
      }
    }

    freeBuffer(inputBuffer);
    freeBuffer(dictionary);
    freeBuffer(inflator);

    if (result != ContentExtractStatus::Complete) {
      return result;
    }

    if (totalOutputBytes != entry.uncompressedSize) {
      Serial.printf("[epub] Inflate size mismatch for %s (%lu of %lu bytes)\n",
                    entry.name.c_str(), static_cast<unsigned long>(totalOutputBytes),
                    static_cast<unsigned long>(entry.uncompressedSize));
      return ContentExtractStatus::Failed;
    }

    reportMaybe(true);
    return finishWriter();
  }

  String archivePath_;
  File file_;
  std::vector<ZipEntry> entries_;
};

bool convertEpubToRsvp(const String &epubPath, const String &tempPath, const String &rsvpPath,
                       const EpubConverter::Options &options) {
  reportProgress(options, "Opening EPUB", "Reading archive", 0);

  ZipArchive zip;
  if (!zip.open(epubPath)) {
    Serial.printf("[epub] Could not open EPUB archive: %s\n", epubPath.c_str());
    return false;
  }

  reportProgress(options, "Opening EPUB", "Reading metadata", 8);
  String containerXml;
  Serial.println("[epub] Reading META-INF/container.xml");
  Serial.flush();
  if (!zip.extractToString("META-INF/container.xml", containerXml, kMaxContainerBytes)) {
    Serial.println("[epub] EPUB container.xml not found or unreadable");
    zip.close();
    return false;
  }
  Serial.printf("[epub] container.xml loaded: %u chars\n",
                static_cast<unsigned int>(containerXml.length()));

  const String opfPath = parseRootfilePath(containerXml);
  if (opfPath.isEmpty()) {
    Serial.println("[epub] EPUB rootfile path not found");
    zip.close();
    return false;
  }
  Serial.printf("[epub] Rootfile OPF path: %s\n", opfPath.c_str());

  reportProgress(options, "Opening EPUB", "Reading package", 14);
  String opfXml;
  Serial.printf("[epub] Reading OPF package: %s\n", opfPath.c_str());
  if (!zip.extractToString(opfPath, opfXml, kMaxOpfBytes)) {
    Serial.printf("[epub] OPF file not readable: %s\n", opfPath.c_str());
    zip.close();
    return false;
  }
  Serial.printf("[epub] OPF loaded: %u chars\n", static_cast<unsigned int>(opfXml.length()));

  const String opfBaseDir = directoryForPath(opfPath);
  const std::vector<ManifestItem> manifest = parseManifestItems(opfXml, opfBaseDir);
  const std::vector<String> spineIds = parseSpineIds(opfXml);
  std::vector<String> readingOrder;
  readingOrder.reserve(spineIds.size());
  Serial.printf("[epub] Package parsed: manifest=%u spine=%u base=%s\n",
                static_cast<unsigned int>(manifest.size()),
                static_cast<unsigned int>(spineIds.size()), opfBaseDir.c_str());

  reportProgress(options, "Opening EPUB", "Building reading order", 20);
  for (size_t i = 0; i < spineIds.size(); ++i) {
    serviceBackground();
    const ManifestItem *item = findManifestItem(manifest, spineIds[i]);
    if (item != nullptr && isContentDocument(*item)) {
      readingOrder.push_back(item->path);
    }
  }

  if (readingOrder.empty()) {
    for (size_t i = 0; i < manifest.size(); ++i) {
      if (isContentDocument(manifest[i])) {
        readingOrder.push_back(manifest[i].path);
      }
    }
  }

  if (readingOrder.empty()) {
    Serial.println("[epub] No readable XHTML spine items found");
    zip.close();
    return false;
  }

  Serial.printf("[epub] Reading order contains %u content files\n",
                static_cast<unsigned int>(readingOrder.size()));
  const String foundDetail = String(readingOrder.size()) + " content files";
  reportProgress(options, "Opening EPUB", foundDetail.c_str(), 25);

  SD_MMC.remove(tempPath);
  File output = SD_MMC.open(tempPath, FILE_WRITE);
  if (!output) {
    Serial.printf("[epub] Could not create temporary RSVP file: %s\n", tempPath.c_str());
    zip.close();
    return false;
  }

  String title = parseBookTitle(opfXml);
  if (title.isEmpty()) {
    title = basenameWithoutExtension(epubPath);
  }
  const String author = parseBookAuthor(opfXml);

  output.println("@rsvp 1");
  output.print("@converter ");
  output.println(kConverterVersion);
  output.print("@title ");
  output.println(title);
  if (!author.isEmpty()) {
    output.print("@author ");
    output.println(author);
  }
  output.print("@source ");
  output.println(epubPath);
  output.println();

  size_t wordCount = 0;
  String lastChapterTitle;

  for (size_t i = 0; i < readingOrder.size() && !reachedWordLimit(wordCount, options.maxWords);
       ++i) {
    serviceBackground();
    const int startPercent = 25 + static_cast<int>((i * 70UL) / readingOrder.size());
    const String startDetail =
        String(i + 1) + "/" + String(readingOrder.size()) + " " + String(wordCount) + " words";
    reportProgress(options, "Extracting content", startDetail.c_str(), startPercent);

    const ContentExtractStatus extractStatus =
        zip.extractContentToRsvp(readingOrder[i], output, wordCount, options.maxWords,
                                 lastChapterTitle, options, i, readingOrder.size());
    const int finishPercent = 25 + static_cast<int>(((i + 1) * 70UL) / readingOrder.size());
    const String finishDetail =
        String(i + 1) + "/" + String(readingOrder.size()) + " " + String(wordCount) + " words";
    reportProgress(options, "Parsed content", finishDetail.c_str(), finishPercent);

    if (extractStatus == ContentExtractStatus::Unsupported ||
        extractStatus == ContentExtractStatus::Failed) {
      Serial.printf("[epub] Skipping unreadable content file: %s\n", readingOrder[i].c_str());
      continue;
    }

    if (extractStatus == ContentExtractStatus::WordLimitReached) {
      break;
    }
  }

  const String finishingDetail = String(wordCount) + " words";
  reportProgress(options, "Finishing EPUB", finishingDetail.c_str(), 96);
  output.close();
  zip.close();

  if (wordCount == 0) {
    Serial.printf("[epub] No readable words extracted from %s\n", epubPath.c_str());
    SD_MMC.remove(tempPath);
    return false;
  }

  SD_MMC.remove(rsvpPath);
  if (!SD_MMC.rename(tempPath, rsvpPath)) {
    Serial.printf("[epub] Could not rename %s to %s\n", tempPath.c_str(), rsvpPath.c_str());
    SD_MMC.remove(tempPath);
    return false;
  }

  Serial.printf("[epub] Converted %s -> %s (%u words)\n", epubPath.c_str(), rsvpPath.c_str(),
                static_cast<unsigned int>(wordCount));
  const String convertedDetail = String(wordCount) + " words";
  reportProgress(options, "EPUB converted", convertedDetail.c_str(), 100);
  return true;
}

void writeFailureMarker(const String &markerPath, const char *message) {
  SD_MMC.remove(markerPath);

  File marker = SD_MMC.open(markerPath, FILE_WRITE);
  if (!marker) {
    Serial.printf("[epub] Could not create failure marker: %s\n", markerPath.c_str());
    return;
  }

  marker.println(message == nullptr ? "Conversion failed" : message);
  marker.print("converter=");
  marker.println(kConverterVersion);
  marker.close();
}

bool markerWasWrittenByCurrentConverter(File &marker) {
  String content;
  while (marker.available() && content.length() < 256) {
    content += static_cast<char>(marker.read());
  }

  const String expected = String("converter=") + kConverterVersion;
  return content.indexOf(expected) >= 0;
}

bool rsvpWasWrittenByCurrentConverter(File &file) {
  if (!file || file.isDirectory()) {
    return false;
  }

  file.seek(0);
  String line;
  size_t scannedLines = 0;
  while (file.available() && scannedLines < 12) {
    const char c = static_cast<char>(file.read());
    if (c == '\r') {
      continue;
    }
    if (c == '\n') {
      line.trim();
      if (line.startsWith("@converter")) {
        const String expected = String("@converter ") + kConverterVersion;
        return line == expected;
      }
      if (!line.isEmpty() && !line.startsWith("@")) {
        break;
      }
      line = "";
      ++scannedLines;
      continue;
    }
    if (line.length() < 128) {
      line += c;
    }
  }

  line.trim();
  if (line.startsWith("@converter")) {
    const String expected = String("@converter ") + kConverterVersion;
    return line == expected;
  }

  return false;
}

}  // namespace

bool EpubConverter::isCurrentCache(const String &rsvpPath) {
  File existing = SD_MMC.open(rsvpPath);
  const bool current = rsvpWasWrittenByCurrentConverter(existing);
  if (existing) {
    existing.close();
  }
  return current;
}

bool EpubConverter::convertIfNeeded(const String &epubPath, const String &rsvpPath,
                                    const Options &options) {
  File existing = SD_MMC.open(rsvpPath);
  if (existing && !existing.isDirectory() && existing.size() > 0) {
    const bool currentCache = rsvpWasWrittenByCurrentConverter(existing);
    existing.close();
    if (currentCache) {
      return true;
    }

    Serial.printf("[epub] Rebuilding stale RSVP cache after converter update: %s\n",
                  rsvpPath.c_str());
    SD_MMC.remove(rsvpPath);
  } else if (existing) {
    existing.close();
  }

  const String tempPath = rsvpPath + ".tmp";
  const String failedPath = rsvpPath + ".failed";
  const String lockPath = rsvpPath + ".converting";

  File lock = SD_MMC.open(lockPath);
  if (lock) {
    const bool lockMarker = !lock.isDirectory();
    const bool currentLock = lockMarker && markerWasWrittenByCurrentConverter(lock);
    lock.close();
    if (lockMarker) {
      SD_MMC.remove(lockPath);
      SD_MMC.remove(tempPath);
      if (currentLock) {
        Serial.printf("[epub] Previous conversion restart detected, skipping: %s\n",
                      epubPath.c_str());
        writeFailureMarker(failedPath, "Previous conversion restarted before completion.");
        reportProgress(options, "Previous restart", "Skipping this EPUB", 100);
        return false;
      }

      Serial.printf("[epub] Retrying interrupted EPUB after converter update: %s\n",
                    epubPath.c_str());
    }
  }

  File temp = SD_MMC.open(tempPath);
  if (temp) {
    const bool interruptedTemp = !temp.isDirectory();
    temp.close();
    if (interruptedTemp) {
      Serial.printf("[epub] Removing stale temporary conversion file and retrying: %s\n",
                    epubPath.c_str());
      SD_MMC.remove(tempPath);
    }
  }

  File failed = SD_MMC.open(failedPath);
  if (failed) {
    const bool failedMarker = !failed.isDirectory();
    const bool currentFailure = failedMarker && markerWasWrittenByCurrentConverter(failed);
    failed.close();
    if (failedMarker) {
      if (currentFailure) {
        Serial.printf("[epub] Skipping EPUB with failure marker: %s\n", epubPath.c_str());
        return false;
      }

      Serial.printf("[epub] Retrying EPUB after converter update: %s\n", epubPath.c_str());
      SD_MMC.remove(failedPath);
    }
  }

  Serial.printf("[epub] Converting on device: %s\n", epubPath.c_str());
  writeFailureMarker(lockPath, "Conversion in progress. Delete this file only if retrying.");
  const bool converted = convertEpubToRsvp(epubPath, tempPath, rsvpPath, options);
  SD_MMC.remove(lockPath);
  if (!converted) {
    writeFailureMarker(failedPath, "Conversion failed. Remove this marker to retry.");
    return false;
  }

  SD_MMC.remove(failedPath);
  return true;
}
