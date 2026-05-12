#include "storage/StorageManager.h"

#include <SD_MMC.h>
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <driver/sdmmc_types.h>
#include <esp_heap_caps.h>
#include <utility>

#include "board/BoardConfig.h"
#include "storage/EpubConverter.h"
#include "text/LatinText.h"

#ifndef RSVP_ON_DEVICE_EPUB_CONVERSION
#define RSVP_ON_DEVICE_EPUB_CONVERSION 0
#endif

#ifndef RSVP_MAX_BOOK_WORDS
#define RSVP_MAX_BOOK_WORDS 0
#endif

namespace {

constexpr const char *kMountPoint = "/sdcard";
constexpr const char *kBooksPath = "/books";
constexpr size_t kMaxBookWords = static_cast<size_t>(RSVP_MAX_BOOK_WORDS);
constexpr size_t kMaxChapterTitleChars = 64;
constexpr int kSdFrequenciesKhz[] = {
    SDMMC_FREQ_DEFAULT,
    10000,
    SDMMC_FREQ_PROBING,
};

bool hasBookWordLimit() { return kMaxBookWords > 0; }

bool reachedBookWordLimit(size_t wordCount) {
  return hasBookWordLimit() && wordCount >= kMaxBookWords;
}

bool isAsciiTrimWhitespace(char c) {
  switch (c) {
    case ' ':
    case '\t':
    case '\n':
    case '\r':
    case '\f':
    case '\v':
      return true;
    default:
      return false;
  }
}

void trimAsciiWhitespace(String &text) {
  size_t start = 0;
  while (start < text.length() && isAsciiTrimWhitespace(text[start])) {
    ++start;
  }

  size_t end = text.length();
  while (end > start && isAsciiTrimWhitespace(text[end - 1])) {
    --end;
  }

  if (end < text.length()) {
    text.remove(end);
  }
  if (start > 0) {
    text.remove(0, start);
  }
}

bool isWordBoundary(char c) {
  const uint8_t value = LatinText::byteValue(c);
  return value <= ' ' && !LatinText::isWordCharacter(value);
}

bool prefixHasBoundary(const String &lowered, const char *prefix) {
  const size_t prefixLength = std::strlen(prefix);
  if (!lowered.startsWith(prefix)) {
    return false;
  }
  if (lowered.length() == prefixLength) {
    return true;
  }

  const char next = lowered[prefixLength];
  const uint8_t nextValue = LatinText::byteValue(next);
  return (nextValue <= ' ' && !LatinText::isWordCharacter(nextValue)) || next == ':' ||
         next == '.' || next == '-';
}

bool booksDirectoryExists() {
  File dir = SD_MMC.open(kBooksPath);
  const bool exists = dir && dir.isDirectory();
  if (dir) {
    dir.close();
  }
  return exists;
}

bool hasTextExtension(const String &path) {
  String lowered = path;
  lowered.toLowerCase();
  return lowered.endsWith(".txt");
}

bool hasRsvpExtension(const String &path) {
  String lowered = path;
  lowered.toLowerCase();
  return lowered.endsWith(".rsvp");
}

bool hasEpubExtension(const String &path) {
  String lowered = path;
  lowered.toLowerCase();
  return lowered.endsWith(".epub");
}

bool hasRsvpSibling(const String &path) {
  String siblingPath = path;
  const int dot = siblingPath.lastIndexOf('.');
  if (dot > 0) {
    siblingPath = siblingPath.substring(0, dot);
  }
  siblingPath += ".rsvp";

  File sibling = SD_MMC.open(siblingPath);
  const bool exists = sibling && !sibling.isDirectory() && sibling.size() > 0;
  if (sibling) {
    sibling.close();
  }
  return exists;
}

String epubSiblingPathForRsvp(const String &rsvpPath) {
  String epubPath = rsvpPath;
  const int dot = epubPath.lastIndexOf('.');
  if (dot > 0) {
    epubPath = epubPath.substring(0, dot);
  }
  epubPath += ".epub";
  return epubPath;
}

String normalizeBookPath(const String &path) {
  if (path.startsWith("/")) {
    return path;
  }
  return String(kBooksPath) + "/" + path;
}

String displayNameForPath(const String &path) {
  const int separator = path.lastIndexOf('/');
  if (separator < 0) {
    return path;
  }
  return path.substring(separator + 1);
}

bool isHiddenOrSidecarPath(const String &path) {
  const String name = displayNameForPath(path);
  if (name.length() == 0) {
    return true;
  }

  String lowered = name;
  lowered.toLowerCase();
  if (lowered.startsWith(".")) {
    return true;
  }

  return lowered == "thumbs.db" || lowered == "desktop.ini";
}

String displayNameWithoutExtension(const String &path) {
  String name = displayNameForPath(path);
  String lowered = name;
  lowered.toLowerCase();
  if (lowered.endsWith(".txt")) {
    name.remove(name.length() - 4);
  } else if (lowered.endsWith(".rsvp")) {
    name.remove(name.length() - 5);
  } else if (lowered.endsWith(".epub")) {
    name.remove(name.length() - 5);
  }
  return name;
}

String rsvpCachePathForEpub(const String &epubPath) {
  String outputPath = epubPath;
  const int dot = outputPath.lastIndexOf('.');
  if (dot > 0) {
    outputPath = outputPath.substring(0, dot);
  }
  outputPath += ".rsvp";
  return outputPath;
}

struct EpubProgressContext {
  StorageManager::StatusCallback statusCallback = nullptr;
  void *statusContext = nullptr;
  String title;
  String label;
  int basePercent = 0;
  int spanPercent = 100;
};

void handleEpubProgress(void *rawContext, const char *line1, const char *line2,
                        int progressPercent) {
  EpubProgressContext *context = static_cast<EpubProgressContext *>(rawContext);
  if (context == nullptr) {
    return;
  }

  progressPercent = std::max(0, std::min(100, progressPercent));
  const int overallPercent =
      context->basePercent + ((context->spanPercent * progressPercent) / 100);
  const String detail = String(line1 == nullptr ? "" : line1) + " - " +
                        String(line2 == nullptr ? "" : line2);
  const char *title = context->title.isEmpty() ? "EPUB" : context->title.c_str();
  Serial.printf("[epub-progress] %d%% %s | %s | %s\n", overallPercent, title,
                context->label.c_str(), detail.c_str());

  // Keep the display on the static "Converting EPUB" screen while ZIP work is active.
  // Full-screen redraws from inside this callback have proven risky while the SD archive is open.
  yield();
  delay(0);
}

bool fileExistsAndHasBytes(const String &path) {
  File file = SD_MMC.open(path);
  const bool exists = file && !file.isDirectory() && file.size() > 0;
  if (file) {
    file.close();
  }
  return exists;
}

bool hasCurrentEpubCache(const String &epubPath) {
  const String rsvpPath = rsvpCachePathForEpub(epubPath);
  return fileExistsAndHasBytes(rsvpPath);
}

bool markerExists(const String &path) {
  File file = SD_MMC.open(path);
  const bool exists = file && !file.isDirectory();
  if (file) {
    file.close();
  }
  return exists;
}

String epubLibraryLabel(const String &path) {
  const String rsvpPath = rsvpCachePathForEpub(path);
  if (markerExists(rsvpPath + ".failed")) {
    return "EPUB failed - check serial";
  }
  if (markerExists(rsvpPath + ".converting") || markerExists(rsvpPath + ".tmp")) {
    return "EPUB interrupted";
  }
  return "EPUB - converts on open";
}

int pathIndexIn(const std::vector<String> &paths, const String &target) {
  for (size_t i = 0; i < paths.size(); ++i) {
    if (paths[i] == target) {
      return static_cast<int>(i);
    }
  }
  return -1;
}

void logHeapSnapshot(const char *label) {
  Serial.printf("[heap] %s free8=%lu free_spiram=%lu largest8=%lu largest_spiram=%lu\n",
                label == nullptr ? "" : label,
                static_cast<unsigned long>(heap_caps_get_free_size(MALLOC_CAP_8BIT)),
                static_cast<unsigned long>(heap_caps_get_free_size(MALLOC_CAP_SPIRAM)),
                static_cast<unsigned long>(heap_caps_get_largest_free_block(MALLOC_CAP_8BIT)),
                static_cast<unsigned long>(heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM)));
}

std::vector<String> collectBookPaths() {
  std::vector<String> bookPaths;

  File dir = SD_MMC.open(kBooksPath);
  if (!dir || !dir.isDirectory()) {
    if (dir) {
      dir.close();
    }
    return bookPaths;
  }

  File entry = dir.openNextFile();
  while (entry) {
    if (!entry.isDirectory()) {
      const String path = normalizeBookPath(String(entry.name()));
      if (isHiddenOrSidecarPath(path)) {
        entry.close();
        entry = dir.openNextFile();
        continue;
      }
      const bool readableText = hasTextExtension(path) && !hasRsvpSibling(path);
      const bool pendingEpub =
          RSVP_ON_DEVICE_EPUB_CONVERSION && hasEpubExtension(path) && !hasCurrentEpubCache(path);
      if (hasRsvpExtension(path) || readableText || pendingEpub) {
        bookPaths.push_back(path);
      }
    }
    entry.close();
    entry = dir.openNextFile();
  }

  dir.close();

  std::sort(bookPaths.begin(), bookPaths.end(), [](const String &left, const String &right) {
    String leftKey = displayNameForPath(left);
    String rightKey = displayNameForPath(right);
    leftKey.toLowerCase();
    rightKey.toLowerCase();
    return leftKey < rightKey;
  });

  return bookPaths;
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

void appendText(String &target, const char *text) {
  while (*text != '\0') {
    target += *text;
    ++text;
  }
}

void appendDisplayApproximation(String &target, uint32_t codepoint) {
  if (codepoint >= 32 && codepoint <= 126) {
    target += static_cast<char>(codepoint);
    return;
  }

  if (codepoint == '\t' || codepoint == '\n' || codepoint == '\r' || codepoint == 0x00A0 ||
      codepoint == 0x1680 || codepoint == 0x180E || codepoint == 0x2028 ||
      codepoint == 0x2029 || codepoint == 0x202F || codepoint == 0x205F ||
      codepoint == 0x3000 || (codepoint >= 0x2000 && codepoint <= 0x200A)) {
    target += ' ';
    return;
  }

  if (codepoint == 0x00AD) {
    return;
  }

  uint8_t storedByte = 0;
  if (LatinText::storageByteForCodepoint(codepoint, storedByte)) {
    target += static_cast<char>(storedByte);
    return;
  }

  if (codepoint >= 0xFF01 && codepoint <= 0xFF5E) {
    target += static_cast<char>(codepoint - 0xFEE0);
    return;
  }

  switch (codepoint) {
    case 0x00A1:
      target += '!';
      return;
    case 0x00A2:
      target += 'c';
      return;
    case 0x00A3:
      appendText(target, "GBP");
      return;
    case 0x00A4:
      target += '$';
      return;
    case 0x00A5:
      target += 'Y';
      return;
    case 0x00A6:
      target += '|';
      return;
    case 0x00A7:
      target += 'S';
      return;
    case 0x00A8:
      target += '"';
      return;
    case 0x00A9:
      appendText(target, "(c)");
      return;
    case 0x00AA:
      target += 'a';
      return;
    case 0x00AB:
      target += '"';
      return;
    case 0x00AC:
      target += '!';
      return;
    case 0x00AE:
      appendText(target, "(r)");
      return;
    case 0x00AF:
      target += '-';
      return;
    case 0x00B0:
      appendText(target, "deg");
      return;
    case 0x00B1:
      appendText(target, "+/-");
      return;
    case 0x00B2:
      target += '2';
      return;
    case 0x00B3:
      target += '3';
      return;
    case 0x00B4:
      target += '\'';
      return;
    case 0x00B5:
      target += 'u';
      return;
    case 0x00B6:
      target += 'P';
      return;
    case 0x00B7:
      target += '*';
      return;
    case 0x00B8:
      target += ',';
      return;
    case 0x00B9:
      target += '1';
      return;
    case 0x00BA:
      target += 'o';
      return;
    case 0x00BB:
      target += '"';
      return;
    case 0x2039:
    case 0x203A:
      target += '\'';
      return;
    case 0x00BC:
      appendText(target, "1/4");
      return;
    case 0x00BD:
      appendText(target, "1/2");
      return;
    case 0x00BE:
      appendText(target, "3/4");
      return;
    case 0x00BF:
      target += '?';
      return;
    case 0x2018:
    case 0x2019:
    case 0x201A:
    case 0x201B:
    case 0x2032:
    case 0x2035:
      target += '\'';
      return;
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
      target += '"';
      return;
    case 0x207D:
    case 0x208D:
    case 0x2768:
    case 0x276A:
    case 0xFF08:
      target += '(';
      return;
    case 0x207E:
    case 0x208E:
    case 0x2769:
    case 0x276B:
    case 0xFF09:
      target += ')';
      return;
    case 0x2045:
    case 0x2308:
    case 0x230A:
    case 0x3010:
    case 0x3014:
    case 0x3016:
    case 0x3018:
    case 0x301A:
    case 0xFF3B:
      target += '[';
      return;
    case 0x2046:
    case 0x2309:
    case 0x230B:
    case 0x3011:
    case 0x3015:
    case 0x3017:
    case 0x3019:
    case 0x301B:
    case 0xFF3D:
      target += ']';
      return;
    case 0x2774:
    case 0x2776:
    case 0xFF5B:
      target += '{';
      return;
    case 0x2775:
    case 0x2777:
    case 0xFF5D:
      target += '}';
      return;
    case 0x2329:
    case 0x27E8:
    case 0x3008:
    case 0x300A:
      target += '<';
      return;
    case 0x232A:
    case 0x27E9:
    case 0x3009:
    case 0x300B:
      target += '>';
      return;
    case 0x2010:
    case 0x2011:
    case 0x2012:
    case 0x2013:
    case 0x2014:
    case 0x2015:
    case 0x2043:
    case 0x2212:
      target += '-';
      return;
    case 0x2026:
      appendText(target, "...");
      return;
    case 0x2022:
    case 0x2219:
      target += '*';
      return;
    case 0xFF0C:
      target += ',';
      return;
    case 0xFF0E:
      target += '.';
      return;
    case 0xFF1A:
      target += ':';
      return;
    case 0xFF1B:
      target += ';';
      return;
    case 0xFF01:
      target += '!';
      return;
    case 0xFF1F:
      target += '?';
      return;
    case 0x2122:
      appendText(target, "TM");
      return;
    case 0x00D7:
      target += 'x';
      return;
    case 0x00F7:
      target += '/';
      return;
    case 0x0100:
    case 0x0102:
    case 0x0104:
      target += 'A';
      return;
    case 0x0101:
    case 0x0103:
    case 0x0105:
      target += 'a';
      return;
    case 0x0106:
    case 0x0108:
    case 0x010A:
    case 0x010C:
      target += 'C';
      return;
    case 0x0107:
    case 0x0109:
    case 0x010B:
    case 0x010D:
      target += 'c';
      return;
    case 0x010E:
    case 0x0110:
      target += 'D';
      return;
    case 0x010F:
    case 0x0111:
      target += 'd';
      return;
    case 0x0112:
    case 0x0114:
    case 0x0116:
    case 0x0118:
    case 0x011A:
      target += 'E';
      return;
    case 0x0113:
    case 0x0115:
    case 0x0117:
    case 0x0119:
    case 0x011B:
      target += 'e';
      return;
    case 0x011C:
    case 0x011E:
    case 0x0120:
    case 0x0122:
      target += 'G';
      return;
    case 0x011D:
    case 0x011F:
    case 0x0121:
    case 0x0123:
      target += 'g';
      return;
    case 0x0124:
    case 0x0126:
      target += 'H';
      return;
    case 0x0125:
    case 0x0127:
      target += 'h';
      return;
    case 0x0128:
    case 0x012A:
    case 0x012C:
    case 0x012E:
    case 0x0130:
      target += 'I';
      return;
    case 0x0129:
    case 0x012B:
    case 0x012D:
    case 0x012F:
    case 0x0131:
      target += 'i';
      return;
    case 0x0134:
      target += 'J';
      return;
    case 0x0135:
      target += 'j';
      return;
    case 0x0136:
      target += 'K';
      return;
    case 0x0137:
      target += 'k';
      return;
    case 0x0139:
    case 0x013B:
    case 0x013D:
    case 0x013F:
    case 0x0141:
      target += 'L';
      return;
    case 0x013A:
    case 0x013C:
    case 0x013E:
    case 0x0140:
    case 0x0142:
      target += 'l';
      return;
    case 0x0143:
    case 0x0145:
    case 0x0147:
      target += 'N';
      return;
    case 0x0144:
    case 0x0146:
    case 0x0148:
      target += 'n';
      return;
    case 0x014C:
    case 0x014E:
    case 0x0150:
      target += 'O';
      return;
    case 0x014D:
    case 0x014F:
    case 0x0151:
      target += 'o';
      return;
    case 0x0152:
      appendText(target, "OE");
      return;
    case 0x0153:
      appendText(target, "oe");
      return;
    case 0x0154:
    case 0x0156:
    case 0x0158:
      target += 'R';
      return;
    case 0x0155:
    case 0x0157:
    case 0x0159:
      target += 'r';
      return;
    case 0x015A:
    case 0x015C:
    case 0x015E:
    case 0x0160:
      target += 'S';
      return;
    case 0x015B:
    case 0x015D:
    case 0x015F:
    case 0x0161:
      target += 's';
      return;
    case 0x0162:
    case 0x0164:
    case 0x0166:
      target += 'T';
      return;
    case 0x0163:
    case 0x0165:
    case 0x0167:
      target += 't';
      return;
    case 0x0168:
    case 0x016A:
    case 0x016C:
    case 0x016E:
    case 0x0170:
    case 0x0172:
      target += 'U';
      return;
    case 0x0169:
    case 0x016B:
    case 0x016D:
    case 0x016F:
    case 0x0171:
    case 0x0173:
      target += 'u';
      return;
    case 0x0174:
      target += 'W';
      return;
    case 0x0175:
      target += 'w';
      return;
    case 0x0176:
    case 0x0178:
      target += 'Y';
      return;
    case 0x0177:
      target += 'y';
      return;
    case 0x0179:
    case 0x017B:
    case 0x017D:
      target += 'Z';
      return;
    case 0x017A:
    case 0x017C:
    case 0x017E:
      target += 'z';
      return;
    case 0x01E2:
    case 0x01FC:
      appendText(target, "AE");
      return;
    case 0x01E3:
    case 0x01FD:
      appendText(target, "ae");
      return;
    case 0xFB00:
      appendText(target, "ff");
      return;
    case 0xFB01:
      appendText(target, "fi");
      return;
    case 0xFB02:
      appendText(target, "fl");
      return;
    case 0xFB03:
      appendText(target, "ffi");
      return;
    case 0xFB04:
      appendText(target, "ffl");
      return;
    case 0xFB05:
    case 0xFB06:
      appendText(target, "st");
      return;
    default:
      return;
  }
}

void appendSingleByteApproximation(String &target, uint8_t value) {
  switch (value) {
    case 0xA0:
      target += ' ';
      return;
    case 0xA1:
      target += '!';
      return;
    case 0xA2:
      target += 'c';
      return;
    case 0xA3:
      appendText(target, "GBP");
      return;
    case 0xA4:
      target += '$';
      return;
    case 0xA5:
      target += 'Y';
      return;
    case 0xA6:
      target += '|';
      return;
    case 0xA7:
      target += 'S';
      return;
    case 0xA8:
      target += '"';
      return;
    case 0xA9:
      appendText(target, "(c)");
      return;
    case 0xAA:
      target += 'a';
      return;
    case 0xAB:
      target += '"';
      return;
    case 0xAD:
      return;
    case 0xAC:
      target += '!';
      return;
    case 0xAE:
      appendText(target, "(r)");
      return;
    case 0xAF:
      target += '-';
      return;
    case 0xB0:
      appendText(target, "deg");
      return;
    case 0xB1:
      appendText(target, "+/-");
      return;
    case 0x80:
      appendText(target, "EUR");
      return;
    case 0x8A:
      target += static_cast<char>(0x86);
      return;
    case 0x8C:
      target += static_cast<char>(0x80);
      return;
    case 0x8E:
      target += static_cast<char>(0x88);
      return;
    case 0x82:
    case 0x91:
    case 0x92:
      target += '\'';
      return;
    case 0x84:
    case 0x93:
    case 0x94:
      target += '"';
      return;
    case 0x85:
      appendText(target, "...");
      return;
    case 0x95:
      target += '*';
      return;
    case 0x96:
    case 0x97:
      target += '-';
      return;
    case 0x99:
      appendText(target, "TM");
      return;
    case 0x9A:
      target += static_cast<char>(0x87);
      return;
    case 0x9C:
      target += static_cast<char>(0x81);
      return;
    case 0x9E:
      target += static_cast<char>(0x89);
      return;
    case 0x9F:
      target += 'Y';
      return;
    case 0xB2:
      target += '2';
      return;
    case 0xB3:
      target += '3';
      return;
    case 0xB4:
      target += '\'';
      return;
    case 0xB5:
      target += 'u';
      return;
    case 0xB6:
      target += 'P';
      return;
    case 0xB7:
      target += '*';
      return;
    case 0xB8:
      target += ',';
      return;
    case 0xB9:
      target += '1';
      return;
    case 0xBA:
      target += 'o';
      return;
    case 0xBB:
      target += '"';
      return;
    case 0xBC:
      appendText(target, "1/4");
      return;
    case 0xBD:
      appendText(target, "1/2");
      return;
    case 0xBE:
      appendText(target, "3/4");
      return;
    case 0xBF:
      target += '?';
      return;
    case 0xD7:
      target += 'x';
      return;
    case 0xF7:
      target += '/';
      return;
    default:
      if (value >= 0xA0) {
        target += static_cast<char>(value);
      }
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
    appendSingleByteApproximation(normalized, static_cast<uint8_t>(text[before]));
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

void pushCleanWord(String token, WordStore &words) {
  trimAsciiWhitespace(token);

  if (token.length() >= 3 && static_cast<uint8_t>(token[0]) == 0xEF &&
      static_cast<uint8_t>(token[1]) == 0xBB && static_cast<uint8_t>(token[2]) == 0xBF) {
    token.remove(0, 3);
  }

  token = normalizeDisplayText(token);
  trimAsciiWhitespace(token);

  bool hasAlphaNumeric = false;
  for (size_t i = 0; i < token.length(); ++i) {
    if (LatinText::isWordCharacter(LatinText::byteValue(token[i]))) {
      hasAlphaNumeric = true;
      break;
    }
  }

  if (!token.isEmpty() && hasAlphaNumeric) {
    words.add(std::move(token));
  }
}

String stripBom(String text) {
  trimAsciiWhitespace(text);
  if (text.length() >= 3 && static_cast<uint8_t>(text[0]) == 0xEF &&
      static_cast<uint8_t>(text[1]) == 0xBB && static_cast<uint8_t>(text[2]) == 0xBF) {
    text.remove(0, 3);
    trimAsciiWhitespace(text);
  }
  return text;
}

bool chapterTitleFromLine(const String &line, String &title) {
  String trimmed = normalizeDisplayText(stripBom(line));
  trimAsciiWhitespace(trimmed);
  if (trimmed.isEmpty() || trimmed.length() > kMaxChapterTitleChars) {
    return false;
  }

  if (trimmed.startsWith("#")) {
    size_t prefixLength = 0;
    while (prefixLength < trimmed.length() && trimmed[prefixLength] == '#') {
      ++prefixLength;
    }
    title = trimmed.substring(prefixLength);
    trimAsciiWhitespace(title);
    return !title.isEmpty();
  }

  String lowered = trimmed;
  lowered.toLowerCase();
  if (prefixHasBoundary(lowered, "chapter") || prefixHasBoundary(lowered, "part") ||
      prefixHasBoundary(lowered, "book")) {
    title = trimmed;
    return true;
  }

  return false;
}

void addChapterMarker(BookContent &book, const String &title) {
  if (title.isEmpty()) {
    return;
  }

  ChapterMarker marker;
  marker.title = title;
  marker.wordIndex = book.words.size();

  if (!book.chapters.empty() && book.chapters.back().wordIndex == marker.wordIndex) {
    book.chapters.back() = marker;
    return;
  }

  book.chapters.push_back(marker);
}

void addParagraphMarker(BookContent &book) {
  const size_t wordIndex = book.words.size();
  if (!book.paragraphStarts.empty() && book.paragraphStarts.back() == wordIndex) {
    return;
  }

  book.paragraphStarts.push_back(wordIndex);
}

String directiveValue(const String &line, const char *directive) {
  String value = line.substring(std::strlen(directive));
  trimAsciiWhitespace(value);
  if (!value.isEmpty() && (value[0] == ':' || value[0] == '-' || value[0] == '.')) {
    value.remove(0, 1);
    trimAsciiWhitespace(value);
  }
  return normalizeDisplayText(value);
}

bool appendLineWords(const String &line, WordStore &words) {
  const String normalizedLine = normalizeDisplayText(line);
  String currentWord;

  for (size_t i = 0; i < normalizedLine.length(); ++i) {
    const char c = normalizedLine[i];
    if (isWordBoundary(c)) {
      if (!currentWord.isEmpty()) {
        pushCleanWord(currentWord, words);
        currentWord = "";
        if (reachedBookWordLimit(words.size())) {
          return false;
        }
      }
      continue;
    }

    currentWord += c;
  }

  if (!currentWord.isEmpty() && !reachedBookWordLimit(words.size())) {
    pushCleanWord(currentWord, words);
  }

  return !reachedBookWordLimit(words.size());
}

bool processBookLine(const String &line, BookContent &book, bool &paragraphPending) {
  const String trimmed = stripBom(line);
  if (trimmed.isEmpty()) {
    paragraphPending = true;
    return true;
  }

  String chapterTitle;
  if (chapterTitleFromLine(line, chapterTitle)) {
    addChapterMarker(book, chapterTitle);
    paragraphPending = true;
  }

  if (paragraphPending) {
    addParagraphMarker(book);
    paragraphPending = false;
  }
  return appendLineWords(line, book.words);
}

bool processRsvpLine(const String &line, BookContent &book, bool &paragraphPending) {
  String trimmed = stripBom(line);
  if (trimmed.isEmpty()) {
    paragraphPending = true;
    return true;
  }

  if (trimmed.startsWith("@@")) {
    trimmed.remove(0, 1);
    if (paragraphPending) {
      addParagraphMarker(book);
      paragraphPending = false;
    }
    return appendLineWords(trimmed, book.words);
  }

  if (trimmed.startsWith("@")) {
    String lowered = trimmed;
    lowered.toLowerCase();
    if (prefixHasBoundary(lowered, "@para")) {
      paragraphPending = true;
      return true;
    }
    if (prefixHasBoundary(lowered, "@chapter")) {
      String title = directiveValue(trimmed, "@chapter");
      if (title.isEmpty()) {
        title = "Chapter";
      }
      addChapterMarker(book, title);
      paragraphPending = true;
      return true;
    }
    if (prefixHasBoundary(lowered, "@title")) {
      book.title = directiveValue(trimmed, "@title");
      return true;
    }
    if (prefixHasBoundary(lowered, "@author")) {
      book.author = directiveValue(trimmed, "@author");
      return true;
    }
    return true;
  }

  if (paragraphPending) {
    addParagraphMarker(book);
    paragraphPending = false;
  }
  return appendLineWords(line, book.words);
}

String readRsvpDirectiveValue(const String &path, const char *directive) {
  if (!hasRsvpExtension(path)) {
    return "";
  }

  File file = SD_MMC.open(path);
  if (!file || file.isDirectory()) {
    if (file) {
      file.close();
    }
    return "";
  }

  String line;
  while (file.available()) {
    const char c = static_cast<char>(file.read());
    if (c == '\r') {
      continue;
    }

    if (c != '\n') {
      line += c;
      if (line.length() > kMaxChapterTitleChars + 16) {
        line = "";
        break;
      }
      continue;
    }

    String trimmed = stripBom(line);
    if (trimmed.isEmpty()) {
      line = "";
      continue;
    }

    String lowered = trimmed;
    lowered.toLowerCase();
    if (prefixHasBoundary(lowered, directive)) {
      file.close();
      return directiveValue(trimmed, directive);
    }

    if (!trimmed.startsWith("@")) {
      break;
    }
    line = "";
  }

  file.close();
  return "";
}

}  // namespace

void StorageManager::setStatusCallback(StatusCallback callback, void *context) {
  statusCallback_ = callback;
  statusContext_ = context;
}

void StorageManager::notifyStatus(const char *title, const char *line1, const char *line2,
                                  int progressPercent) {
  Serial.printf("[storage-status] %d%% %s | %s | %s\n", progressPercent,
                title == nullptr ? "" : title, line1 == nullptr ? "" : line1,
                line2 == nullptr ? "" : line2);
  if (statusCallback_ != nullptr) {
    statusCallback_(statusContext_, title, line1, line2, progressPercent);
  }
}

bool StorageManager::begin() {
  mounted_ = false;
  listedOnce_ = false;
  bookPaths_.clear();

  if (!SD_MMC.setPins(BoardConfig::PIN_SD_CLK, BoardConfig::PIN_SD_CMD, BoardConfig::PIN_SD_D0)) {
    Serial.println("[storage] SD_MMC pin setup failed");
    return false;
  }

  for (int frequencyKhz : kSdFrequenciesKhz) {
    notifyStatus("SD", "Mounting card", "", 5);
    Serial.printf("[storage] Trying SD_MMC mount at %d kHz\n", frequencyKhz);
    SD_MMC.end();
    mounted_ = SD_MMC.begin(kMountPoint, true, false, frequencyKhz, 5);
    if (mounted_) {
      const uint64_t sizeMb = SD_MMC.cardSize() / (1024ULL * 1024ULL);
      Serial.printf("[storage] SD initialized (%llu MB) at %d kHz\n", sizeMb, frequencyKhz);
      return true;
    }
  }

  Serial.println("[storage] SD init failed after retries");
  return false;
}

void StorageManager::end() {
  if (mounted_) {
    SD_MMC.end();
  }
  mounted_ = false;
  listedOnce_ = false;
  bookPaths_.clear();
}

void StorageManager::listBooks() {
  if (!mounted_ || listedOnce_) {
    return;
  }
  listedOnce_ = true;

  if (!booksDirectoryExists()) {
    Serial.println("[storage] /books directory not found");
    return;
  }

  if (bookPaths_.empty()) {
    refreshBookPaths();
  }
  if (bookPaths_.empty()) {
    Serial.println("[storage] No readable .rsvp, .txt, or .epub books found under /books");
    return;
  }

  Serial.println("[storage] Listing /books (.rsvp/.txt/.epub pending conversion):");
  for (const String &path : bookPaths_) {
    File entry = SD_MMC.open(path);
    if (!entry || entry.isDirectory()) {
      if (entry) {
        entry.close();
      }
      continue;
    }

    Serial.printf("  %s (%lu bytes)\n", displayNameForPath(path).c_str(),
                  static_cast<unsigned long>(entry.size()));
    entry.close();
  }
}

void StorageManager::refreshBooks() {
  refreshBookPaths();
}

bool StorageManager::loadFirstBookWords(WordStore &words, String *loadedPath) {
  return loadBookWords(0, words, loadedPath, nullptr, true, false);
}

size_t StorageManager::bookCount() const { return bookPaths_.size(); }

String StorageManager::bookPath(size_t index) const {
  if (index >= bookPaths_.size()) {
    return "";
  }
  return bookPaths_[index];
}

String StorageManager::bookDisplayName(size_t index) const {
  const String path = bookPath(index);
  if (path.isEmpty()) {
    return "";
  }

  const String title = readRsvpDirectiveValue(path, "@title");
  if (!title.isEmpty()) {
    return title;
  }

  return normalizeDisplayText(displayNameWithoutExtension(path));
}

String StorageManager::bookAuthorName(size_t index) const {
  const String path = bookPath(index);
  if (path.isEmpty()) {
    return "";
  }

  if (hasEpubExtension(path)) {
    return epubLibraryLabel(path);
  }

  return readRsvpDirectiveValue(path, "@author");
}

bool StorageManager::ensureEpubConverted(const String &epubPath, String &rsvpPath) {
  rsvpPath = rsvpCachePathForEpub(epubPath);

  if (!RSVP_ON_DEVICE_EPUB_CONVERSION) {
    Serial.printf("[storage] EPUB conversion disabled at build time: %s\n", epubPath.c_str());
    notifyStatus("EPUB unsupported", displayNameForPath(epubPath).c_str(),
                 "Build flag is disabled", 100);
    return false;
  }

  if (!fileExistsAndHasBytes(epubPath)) {
    Serial.printf("[storage] EPUB source missing or empty: %s\n", epubPath.c_str());
    notifyStatus("Preparing book", displayNameForPath(epubPath).c_str(), "EPUB missing", 100);
    return false;
  }

  if (fileExistsAndHasBytes(rsvpPath) && EpubConverter::isCurrentCache(rsvpPath)) {
    Serial.printf("[storage] EPUB cache hit: %s -> %s\n", epubPath.c_str(), rsvpPath.c_str());
    return true;
  }

  if (fileExistsAndHasBytes(rsvpPath)) {
    Serial.printf("[storage] EPUB cache stale after converter update: %s\n", rsvpPath.c_str());
  }

  File epubFile = SD_MMC.open(epubPath);
  const size_t epubBytes = epubFile ? static_cast<size_t>(epubFile.size()) : 0;
  if (epubFile) {
    epubFile.close();
  }

  Serial.printf("[storage] Preparing EPUB conversion: source=%s output=%s size=%lu bytes\n",
                epubPath.c_str(), rsvpPath.c_str(), static_cast<unsigned long>(epubBytes));
  logHeapSnapshot("before EPUB conversion");
  notifyStatus("Preparing book", displayNameForPath(epubPath).c_str(), "Converting EPUB", 0);

  EpubConverter::Options options;
  options.maxWords = kMaxBookWords;
  options.progressCallback = handleEpubProgress;
  EpubProgressContext progressContext;
  progressContext.statusCallback = statusCallback_;
  progressContext.statusContext = statusContext_;
  progressContext.title = "Preparing book";
  progressContext.label = displayNameForPath(epubPath);
  options.progressContext = &progressContext;

  const uint32_t startedMs = millis();
  const bool converted = EpubConverter::convertIfNeeded(epubPath, rsvpPath, options);
  const uint32_t elapsedMs = millis() - startedMs;
  logHeapSnapshot("after EPUB conversion");

  if (!converted || !fileExistsAndHasBytes(rsvpPath)) {
    Serial.printf("[storage] EPUB conversion failed after %lu ms: %s\n",
                  static_cast<unsigned long>(elapsedMs), epubPath.c_str());
    notifyStatus("Preparing book", "EPUB conversion failed", "Check serial monitor", 100);
    return false;
  }

  Serial.printf("[storage] EPUB conversion ready after %lu ms: %s\n",
                static_cast<unsigned long>(elapsedMs), rsvpPath.c_str());
  notifyStatus("Preparing book", displayNameForPath(rsvpPath).c_str(), "Conversion complete",
               100);
  return true;
}

bool StorageManager::loadBookContent(size_t index, BookContent &book, String *loadedPath,
                                     size_t *loadedIndex, bool allowFallback,
                                     bool allowEpubConversion) {
  book.clear();

  if (!mounted_) {
    Serial.println("[storage] SD not mounted, cannot load book");
    return false;
  }

  if (!booksDirectoryExists()) {
    Serial.println("[storage] /books directory not found");
    return false;
  }

  refreshBookPaths();
  if (bookPaths_.empty()) {
    Serial.println("[storage] No readable .rsvp, .txt, or .epub books found under /books");
    return false;
  }

  if (index >= bookPaths_.size()) {
    Serial.printf("[storage] Book index %u out of range\n", static_cast<unsigned int>(index));
    return false;
  }

  const String requestedPath = bookPaths_[index];
  const size_t attempts = allowFallback ? bookPaths_.size() : 1;
  for (size_t offset = 0; offset < attempts; ++offset) {
    const size_t candidateIndex = (index + offset) % bookPaths_.size();
    String path = bookPaths_[candidateIndex];
    size_t parsedIndex = candidateIndex;

    if (hasEpubExtension(path)) {
      if (!allowEpubConversion) {
        book.clear();
        continue;
      }

      String rsvpPath;
      if (!ensureEpubConverted(path, rsvpPath)) {
        if (allowFallback) {
          book.clear();
          continue;
        }
        return false;
      }

      refreshBookPaths();
      const int convertedIndex = pathIndexIn(bookPaths_, rsvpPath);
      if (convertedIndex < 0) {
        Serial.printf("[storage] Converted RSVP not found in refreshed library: %s\n",
                      rsvpPath.c_str());
        if (allowFallback) {
          book.clear();
          continue;
        }
        return false;
      }

      path = rsvpPath;
      parsedIndex = static_cast<size_t>(convertedIndex);
    }

    File entry = SD_MMC.open(path);
    if (!entry || entry.isDirectory()) {
      if (entry) {
        entry.close();
      }
      continue;
    }

    if (parseFile(entry, book, hasRsvpExtension(path))) {
      if (book.title.isEmpty()) {
        book.title = normalizeDisplayText(displayNameWithoutExtension(path));
      }
      Serial.printf("[storage] Loaded %u words and %u chapters from %s\n",
                    static_cast<unsigned int>(book.words.size()),
                    static_cast<unsigned int>(book.chapters.size()), path.c_str());
      if (loadedPath != nullptr) {
        *loadedPath = path;
      }
      if (loadedIndex != nullptr) {
        *loadedIndex = parsedIndex;
      }
      entry.close();
      return true;
    }

    book.clear();
    entry.close();
  }

  if (allowFallback) {
    Serial.println("[storage] No readable book files found under /books");
  } else {
    Serial.printf("[storage] Selected book is not readable: %s\n",
                  requestedPath.c_str());
  }
  return false;
}

bool StorageManager::loadBookWords(size_t index, WordStore &words, String *loadedPath,
                                   size_t *loadedIndex, bool allowFallback,
                                   bool allowEpubConversion) {
  BookContent book;
  if (!loadBookContent(index, book, loadedPath, loadedIndex, allowFallback,
                       allowEpubConversion)) {
    words.clear();
    return false;
  }

  words = std::move(book.words);
  return true;
}

void StorageManager::refreshBookPaths() {
  if (!mounted_) {
    bookPaths_.clear();
    return;
  }

  notifyStatus("SD", "Reading library", "", 96);
  bookPaths_ = collectBookPaths();

  size_t rsvpCount = 0;
  size_t textCount = 0;
  size_t pendingEpubCount = 0;
  for (const String &path : bookPaths_) {
    if (hasRsvpExtension(path)) {
      ++rsvpCount;
    } else if (hasTextExtension(path)) {
      ++textCount;
    } else if (hasEpubExtension(path)) {
      ++pendingEpubCount;
    }
  }

  Serial.printf("[storage] Library scan: %u books (%u rsvp, %u txt, %u pending epub)\n",
                static_cast<unsigned int>(bookPaths_.size()),
                static_cast<unsigned int>(rsvpCount), static_cast<unsigned int>(textCount),
                static_cast<unsigned int>(pendingEpubCount));
}

bool StorageManager::parseFile(File &file, BookContent &book, bool rsvpFormat) {
  book.clear();
  constexpr size_t kInitialReserveWordLimit = 300000;
  const size_t estimatedWords = static_cast<size_t>(file.size() / 8);
  const size_t reserveWords =
      hasBookWordLimit() ? std::min(estimatedWords, kMaxBookWords) : estimatedWords;
  book.words.reserve(std::min(reserveWords, kInitialReserveWordLimit), static_cast<size_t>(file.size()));
  String line;
  line.reserve(256);
  bool paragraphPending = true;
  bool keepReading = true;

  constexpr size_t kBufSize = 512;
  constexpr size_t kMaxBufferedLineBytes = 4096;
  static uint8_t buf[kBufSize];

  while (keepReading && file.available()) {
    const size_t bytesRead = file.read(buf, kBufSize);
    if (bytesRead == 0) {
      break;
    }
    yield();

    for (size_t i = 0; i < bytesRead && keepReading; ++i) {
      const char c = static_cast<char>(buf[i]);

      if (c == '\r') {
        continue;
      }

      if (c == '\n') {
        keepReading = rsvpFormat ? processRsvpLine(line, book, paragraphPending)
                                 : processBookLine(line, book, paragraphPending);
        if (!keepReading && hasBookWordLimit()) {
          Serial.printf("[storage] Reached %lu word limit, truncating book\n",
                        static_cast<unsigned long>(kMaxBookWords));
        }
        line = "";
        continue;
      }

      line += c;

      if (line.length() >= kMaxBufferedLineBytes && isWordBoundary(c)) {
        keepReading = rsvpFormat ? processRsvpLine(line, book, paragraphPending)
                                 : processBookLine(line, book, paragraphPending);
        if (!keepReading && hasBookWordLimit()) {
          Serial.printf("[storage] Reached %lu word limit, truncating book\n",
                        static_cast<unsigned long>(kMaxBookWords));
        }
        line = "";
      }
    }
  }

  if (!line.isEmpty() && keepReading && !reachedBookWordLimit(book.words.size())) {
    if (rsvpFormat) {
      processRsvpLine(line, book, paragraphPending);
    } else {
      processBookLine(line, book, paragraphPending);
    }
  }

  if (!book.words.empty() && book.paragraphStarts.empty()) {
    book.paragraphStarts.push_back(0);
  }

  return !book.words.empty();
}
