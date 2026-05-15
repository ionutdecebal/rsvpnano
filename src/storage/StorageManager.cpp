#include "storage/StorageManager.h"

#include <SD_MMC.h>
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <driver/sdmmc_types.h>
#include <esp_heap_caps.h>
#include <new>
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
constexpr const char *kBookFilesPath = "/books/books";
constexpr const char *kArticleFilesPath = "/books/articles";
constexpr size_t kMaxBookWords = static_cast<size_t>(RSVP_MAX_BOOK_WORDS);
constexpr size_t kMaxChapterTitleChars = 64;
constexpr size_t kMaxBookLineChars = 4096;
constexpr size_t kInitialWordReserveMax = 800000;
constexpr size_t kParseMemoryCheckWordInterval = 512;
constexpr size_t kParseMinFreeHeapBytes = 32 * 1024;
constexpr size_t kParseMinLargestHeapBlockBytes = 8 * 1024;
constexpr int kSdFrequenciesKhz[] = {
    SDMMC_FREQ_DEFAULT,
    10000,
    SDMMC_FREQ_PROBING,
};

bool hasBookWordLimit() { return kMaxBookWords > 0; }

bool reachedBookWordLimit(size_t wordCount) {
  return hasBookWordLimit() && wordCount >= kMaxBookWords;
}

struct ParseStats {
  size_t malformedUtf8 = 0;
  size_t nonAsciiCodepoints = 0;
  size_t longLineSplits = 0;
  bool memoryLow = false;
};

bool parseMemoryLow() {
  return heap_caps_get_free_size(MALLOC_CAP_8BIT) < kParseMinFreeHeapBytes ||
         heap_caps_get_largest_free_block(MALLOC_CAP_8BIT) < kParseMinLargestHeapBlockBytes;
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

String cardTypeLabel(uint8_t cardType) {
  switch (cardType) {
    case CARD_MMC:
      return "MMC";
    case CARD_SD:
      return "SDSC";
    case CARD_SDHC:
      return "SDHC/SDXC";
    default:
      return "Unknown";
  }
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

String siblingPathWithExtension(const String &path, const char *extension) {
  String siblingPath = path;
  const int dot = siblingPath.lastIndexOf('.');
  if (dot > 0) {
    siblingPath = siblingPath.substring(0, dot);
  }
  siblingPath += extension;
  return siblingPath;
}

String epubSiblingPathForRsvp(const String &rsvpPath) {
  return siblingPathWithExtension(rsvpPath, ".epub");
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
  return siblingPathWithExtension(epubPath, ".rsvp");
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
  return fileExistsAndHasBytes(rsvpPath) && EpubConverter::isCurrentCache(rsvpPath);
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

struct DirectoryEntryInfo {
  String path;
  String loweredPath;
  size_t bytes = 0;
};

void appendLibraryDirectoryEntries(const char *directoryPath, std::vector<DirectoryEntryInfo> &entries) {
  File dir = SD_MMC.open(directoryPath);
  if (!dir || !dir.isDirectory()) {
    if (dir) {
      dir.close();
    }
    return;
  }

  File entry = dir.openNextFile();
  while (entry) {
    if (!entry.isDirectory()) {
      const String name = displayNameForPath(String(entry.name()));
      if (!name.isEmpty()) {
        DirectoryEntryInfo info;
        info.path = String(directoryPath) + "/" + name;
        info.loweredPath = info.path;
        info.loweredPath.toLowerCase();
        info.bytes = static_cast<size_t>(entry.size());
        entries.push_back(info);
      }
    }
    entry.close();
    entry = dir.openNextFile();
  }

  dir.close();
}

std::vector<DirectoryEntryInfo> inventoryBookDirectory() {
  std::vector<DirectoryEntryInfo> entries;

  appendLibraryDirectoryEntries(kBooksPath, entries);
  appendLibraryDirectoryEntries(kBookFilesPath, entries);
  appendLibraryDirectoryEntries(kArticleFilesPath, entries);
  return entries;
}

bool inventoryHasFileWithBytes(const std::vector<DirectoryEntryInfo> &entries, const String &path) {
  String loweredPath = path;
  loweredPath.toLowerCase();
  for (const DirectoryEntryInfo &entry : entries) {
    if (entry.loweredPath == loweredPath) {
      return entry.bytes > 0;
    }
  }
  return false;
}

std::vector<String> collectBookPaths() {
  std::vector<String> bookPaths;
  const uint32_t startedMs = millis();
  const std::vector<DirectoryEntryInfo> entries = inventoryBookDirectory();
  size_t cacheProbeCount = 0;

  for (const DirectoryEntryInfo &entry : entries) {
    const String &path = entry.path;
    if (isHiddenOrSidecarPath(path)) {
      continue;
    }

    bool staleGeneratedRsvp = false;
    if (hasRsvpExtension(path) &&
        inventoryHasFileWithBytes(entries, epubSiblingPathForRsvp(path))) {
      ++cacheProbeCount;
      staleGeneratedRsvp = !EpubConverter::isCurrentCache(path);
    }

    const bool readableText =
        hasTextExtension(path) &&
        !inventoryHasFileWithBytes(entries, siblingPathWithExtension(path, ".rsvp"));

    bool pendingEpub = false;
    if (RSVP_ON_DEVICE_EPUB_CONVERSION && hasEpubExtension(path)) {
      const String rsvpPath = rsvpCachePathForEpub(path);
      const bool hasCache = inventoryHasFileWithBytes(entries, rsvpPath);
      if (!hasCache) {
        pendingEpub = true;
      } else {
        ++cacheProbeCount;
        pendingEpub = !EpubConverter::isCurrentCache(rsvpPath);
      }
    }

    if ((!staleGeneratedRsvp && hasRsvpExtension(path)) || readableText || pendingEpub) {
      bookPaths.push_back(path);
    }
  }

  std::sort(bookPaths.begin(), bookPaths.end(), [](const String &left, const String &right) {
    String leftKey = displayNameForPath(left);
    String rightKey = displayNameForPath(right);
    leftKey.toLowerCase();
    rightKey.toLowerCase();
    return leftKey < rightKey;
  });

  Serial.printf("[storage] Directory inventory: %u files, %u books, %u cache probes in %lu ms\n",
                static_cast<unsigned int>(entries.size()),
                static_cast<unsigned int>(bookPaths.size()),
                static_cast<unsigned int>(cacheProbeCount),
                static_cast<unsigned long>(millis() - startedMs));

  return bookPaths;
}

size_t countUnsupportedBookFiles() {
  size_t unsupported = 0;

  const std::vector<DirectoryEntryInfo> entries = inventoryBookDirectory();
  for (const DirectoryEntryInfo &entry : entries) {
    const String &path = entry.path;
    const bool hidden = isHiddenOrSidecarPath(path);
    if (!hidden && !hasRsvpExtension(path) && !hasTextExtension(path) && !hasEpubExtension(path)) {
      ++unsupported;
    }
  }

  return unsupported;
}

bool writeDiagnosticProbeFile() {
  const String path = String(kBooksPath) + "/.sdcheck.tmp";
  SD_MMC.remove(path);
  File file = SD_MMC.open(path, FILE_WRITE);
  if (!file) {
    return false;
  }
  const size_t written = file.print("rsvp-nano sd check\n");
  file.close();
  const bool removed = SD_MMC.remove(path);
  return written > 0 && removed;
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
      target += 'A';
      return;
    case 0x0101:
    case 0x0103:
      target += 'a';
      return;
    case 0x0108:
    case 0x010A:
    case 0x010C:
      target += 'C';
      return;
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
    case 0x011A:
      target += 'E';
      return;
    case 0x0113:
    case 0x0115:
    case 0x0117:
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
      target += 'L';
      return;
    case 0x013A:
    case 0x013C:
    case 0x013E:
    case 0x0140:
      target += 'l';
      return;
    case 0x0145:
    case 0x0147:
      target += 'N';
      return;
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
    case 0x015C:
    case 0x015E:
    case 0x0160:
      target += 'S';
      return;
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
    case 0x017D:
      target += 'Z';
      return;
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
      target += static_cast<char>(0x96);
      return;
    case 0xA2:
      target += 'c';
      return;
    case 0xA3:
      target += static_cast<char>(0x82);
      return;
    case 0xA4:
      target += '$';
      return;
    case 0xA5:
      target += 'Y';
      return;
    case 0xA6:
      target += static_cast<char>(0x9E);
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
      target += static_cast<char>(0xB4);
      return;
    case 0xAF:
      target += static_cast<char>(0xB2);
      return;
    case 0xB0:
      appendText(target, "deg");
      return;
    case 0xB1:
      target += static_cast<char>(0x97);
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
      target += static_cast<char>(0x83);
      return;
    case 0xB4:
      target += '\'';
      return;
    case 0xB5:
      target += 'u';
      return;
    case 0xB6:
      target += static_cast<char>(0x9F);
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
      target += static_cast<char>(0xB5);
      return;
    case 0xBF:
      target += static_cast<char>(0xB3);
      return;
    case 0xC6:
      target += static_cast<char>(0x9A);
      return;
    case 0xCA:
      target += static_cast<char>(0x98);
      return;
    case 0xD1:
      target += static_cast<char>(0x9C);
      return;
    case 0xD7:
      target += 'x';
      return;
    case 0xE6:
      target += static_cast<char>(0x9B);
      return;
    case 0xEA:
      target += static_cast<char>(0x99);
      return;
    case 0xF1:
      target += static_cast<char>(0x9D);
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

String normalizeDisplayText(const String &text, ParseStats *stats = nullptr) {
  String normalized;
  normalized.reserve(text.length());

  size_t index = 0;
  while (index < text.length()) {
    const size_t before = index;
    uint32_t codepoint = 0;
    if (decodeUtf8Codepoint(text, index, codepoint)) {
      if (stats != nullptr && codepoint > 0x7F) {
        ++stats->nonAsciiCodepoints;
      }
      appendDisplayApproximation(normalized, codepoint);
      continue;
    }

    if (stats != nullptr && static_cast<uint8_t>(text[before]) >= 0x80) {
      ++stats->malformedUtf8;
    }
    index = before + 1;
    const uint8_t rawByte = static_cast<uint8_t>(text[before]);
    if (LatinText::isWordCharacter(rawByte) || LatinText::isLowCustomSlotByte(rawByte)) {
      normalized += static_cast<char>(rawByte);
    } else {
      appendSingleByteApproximation(normalized, rawByte);
    }
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

bool pushCleanWord(String token, std::vector<String> &words, ParseStats *stats) {
  trimAsciiWhitespace(token);

  if (token.length() >= 3 && static_cast<uint8_t>(token[0]) == 0xEF &&
      static_cast<uint8_t>(token[1]) == 0xBB && static_cast<uint8_t>(token[2]) == 0xBF) {
    token.remove(0, 3);
  }

  trimAsciiWhitespace(token);

  bool hasAlphaNumeric = false;
  for (size_t i = 0; i < token.length(); ++i) {
    if (LatinText::isWordCharacter(LatinText::byteValue(token[i]))) {
      hasAlphaNumeric = true;
      break;
    }
  }

  if (token.isEmpty() || !hasAlphaNumeric) {
    return true;
  }

  if ((words.size() % kParseMemoryCheckWordInterval) == 0 && words.size() > 0 &&
      parseMemoryLow()) {
    if (stats != nullptr) {
      stats->memoryLow = true;
    }
    return false;
  }

  words.push_back(token);
  return true;
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

bool appendLineWords(const String &line, std::vector<String> &words, ParseStats *stats) {
  const String normalizedLine = normalizeDisplayText(line, stats);
  String currentWord;
  currentWord.reserve(32);

  for (size_t i = 0; i < normalizedLine.length(); ++i) {
    const char c = normalizedLine[i];
    if (isWordBoundary(c)) {
      if (!currentWord.isEmpty()) {
        if (!pushCleanWord(currentWord, words, stats)) {
          return false;
        }
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
    if (!pushCleanWord(currentWord, words, stats)) {
      return false;
    }
  }

  return !reachedBookWordLimit(words.size());
}

bool processBookLine(const String &line, BookContent &book, bool &paragraphPending,
                     ParseStats *stats) {
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
  return appendLineWords(line, book.words, stats);
}

bool processRsvpLine(const String &line, BookContent &book, bool &paragraphPending,
                     ParseStats *stats) {
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
    return appendLineWords(trimmed, book.words, stats);
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
  return appendLineWords(line, book.words, stats);
}

struct RsvpDirectiveValues {
  String title;
  String author;
};

RsvpDirectiveValues readRsvpDirectiveValues(const String &path) {
  RsvpDirectiveValues values;
  if (!hasRsvpExtension(path)) {
    return values;
  }

  File file = SD_MMC.open(path);
  if (!file || file.isDirectory()) {
    if (file) {
      file.close();
    }
    return values;
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
    if (values.title.isEmpty() && prefixHasBoundary(lowered, "@title")) {
      values.title = directiveValue(trimmed, "@title");
    } else if (values.author.isEmpty() && prefixHasBoundary(lowered, "@author")) {
      values.author = directiveValue(trimmed, "@author");
    } else if (!trimmed.startsWith("@")) {
      break;
    }

    if (!values.title.isEmpty() && !values.author.isEmpty()) {
      break;
    }
    line = "";
  }

  file.close();
  return values;
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
  clearBookCache();

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
      notifyStatus("SD", "Scanning books", "EPUB converts on open", 10);
      refreshBookPaths(false);
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
  clearBookCache();
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

  Serial.println("[storage] Listing /books, /books/books, /books/articles (.rsvp/.txt/.epub pending conversion):");
  for (const String &path : bookPaths_) {
    File entry = SD_MMC.open(path);
    if (!entry || entry.isDirectory()) {
      if (entry) {
        entry.close();
      }
      continue;
    }

    Serial.printf("  %s (%lu bytes)\n", path.c_str(),
                  static_cast<unsigned long>(entry.size()));
    entry.close();
  }
}

void StorageManager::refreshBooks(bool includeMetadata) {
  refreshBookPaths(includeMetadata);
}

bool StorageManager::loadFirstBookWords(std::vector<String> &words, String *loadedPath) {
  return loadBookWords(0, words, loadedPath);
}

size_t StorageManager::bookCount() const { return bookPaths_.size(); }

String StorageManager::bookPath(size_t index) const {
  if (index >= bookPaths_.size()) {
    return "";
  }
  return bookPaths_[index];
}

bool StorageManager::bookIsArticle(size_t index) const {
  const String path = bookPath(index);
  return path.startsWith(String(kArticleFilesPath) + "/");
}

String StorageManager::bookDisplayName(size_t index) const {
  const String path = bookPath(index);
  if (path.isEmpty()) {
    return "";
  }

  if (index < bookTitles_.size() && !bookTitles_[index].isEmpty()) {
    return bookTitles_[index];
  }

  return normalizeDisplayText(displayNameWithoutExtension(path));
}

String StorageManager::bookAuthorName(size_t index) const {
  const String path = bookPath(index);
  if (path.isEmpty()) {
    return "";
  }

  if (index < bookAuthors_.size()) {
    return bookAuthors_[index];
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
                                     size_t *loadedIndex) {
  book.clear();

  if (!mounted_) {
    Serial.println("[storage] SD not mounted, cannot load book");
    return false;
  }

  if (!booksDirectoryExists()) {
    Serial.println("[storage] /books directory not found");
    return false;
  }

  if (bookPaths_.empty()) {
    refreshBookPaths(false);
  }
  if (bookPaths_.empty()) {
    Serial.println("[storage] No readable .rsvp, .txt, or .epub books found under /books");
    return false;
  }

  if (index >= bookPaths_.size()) {
    Serial.printf("[storage] Book index %u out of range\n", static_cast<unsigned int>(index));
    return false;
  }

  for (size_t offset = 0; offset < bookPaths_.size(); ++offset) {
    const size_t candidateIndex = (index + offset) % bookPaths_.size();
    String path = bookPaths_[candidateIndex];
    size_t parsedIndex = candidateIndex;

    if (hasEpubExtension(path)) {
      String rsvpPath;
      if (!ensureEpubConverted(path, rsvpPath)) {
        return false;
      }

      refreshBookPaths();
      const int convertedIndex = pathIndexIn(bookPaths_, rsvpPath);
      if (convertedIndex < 0) {
        Serial.printf("[storage] Converted RSVP not found in refreshed library: %s\n",
                      rsvpPath.c_str());
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

    const uint32_t parseStartedMs = millis();
    const bool parsed = parseFile(entry, book, hasRsvpExtension(path));
    const uint32_t parseElapsedMs = millis() - parseStartedMs;
    if (parsed) {
      if (book.title.isEmpty()) {
        book.title = normalizeDisplayText(displayNameWithoutExtension(path));
      }
      Serial.printf("[storage] Loaded %u words and %u chapters from %s in %lu ms\n",
                    static_cast<unsigned int>(book.words.size()),
                    static_cast<unsigned int>(book.chapters.size()), path.c_str(),
                    static_cast<unsigned long>(parseElapsedMs));
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

  Serial.println("[storage] No readable book files found under /books");
  return false;
}

bool StorageManager::loadBookWords(size_t index, std::vector<String> &words, String *loadedPath,
                                   size_t *loadedIndex) {
  BookContent book;
  if (!loadBookContent(index, book, loadedPath, loadedIndex)) {
    words.clear();
    return false;
  }

  words = std::move(book.words);
  return true;
}

StorageManager::DiagnosticResult StorageManager::diagnoseSdCard() {
  DiagnosticResult result;
  notifyStatus("SD check", "Mounting card", "", 5);

  if (!mounted_) {
    if (!SD_MMC.setPins(BoardConfig::PIN_SD_CLK, BoardConfig::PIN_SD_CMD, BoardConfig::PIN_SD_D0)) {
      result.summary = "Pin setup failed";
      result.detail = "Check SD wiring";
      Serial.println("[sd-check] SD_MMC pin setup failed");
      return result;
    }

    for (int frequencyKhz : kSdFrequenciesKhz) {
      Serial.printf("[sd-check] trying mount at %d kHz\n", frequencyKhz);
      SD_MMC.end();
      mounted_ = SD_MMC.begin(kMountPoint, true, false, frequencyKhz, 5);
      if (mounted_) {
        break;
      }
    }
  }

  result.mounted = mounted_;
  if (!mounted_) {
    result.summary = "Card not mounted";
    result.detail = "FAT32? seated fully?";
    Serial.println("[sd-check] mount failed; likely format, seating, or card fault");
    return result;
  }

  result.sizeMb = SD_MMC.cardSize() / (1024ULL * 1024ULL);
  result.cardType = cardTypeLabel(SD_MMC.cardType());
  Serial.printf("[sd-check] mounted type=%s size=%llu MB\n", result.cardType.c_str(),
                result.sizeMb);

  result.booksDirectory = booksDirectoryExists();
  if (!result.booksDirectory) {
    result.summary = "Missing /books";
    result.detail = "Create folder at root";
    Serial.println("[sd-check] /books directory missing");
    return result;
  }

  notifyStatus("SD check", "Scanning /books", "", 45);
  bookPaths_ = collectBookPaths();
  rebuildBookMetadataCache();
  result.bookCount = bookPaths_.size();
  result.unsupportedCount = countUnsupportedBookFiles();

  notifyStatus("SD check", "Testing write", "", 70);
  result.writable = writeDiagnosticProbeFile();
  if (!result.writable) {
    result.summary = "Write test failed";
    result.detail = "Try FAT32/new card";
    Serial.println("[sd-check] write/delete probe failed");
    return result;
  }

  if (result.bookCount == 0) {
    result.summary = "No books found";
    if (result.unsupportedCount > 0) {
      result.detail = "Use .rsvp .txt .epub";
    } else {
      result.detail = "Upload to /books";
    }
    Serial.printf("[sd-check] no supported books; unsupported=%u\n",
                  static_cast<unsigned int>(result.unsupportedCount));
    return result;
  }

  result.summary = String(result.bookCount) + " books OK";
  result.detail = result.cardType + " " + String(static_cast<unsigned int>(result.sizeMb)) + " MB";
  Serial.printf("[sd-check] OK books=%u unsupported=%u writable=%u\n",
                static_cast<unsigned int>(result.bookCount),
                static_cast<unsigned int>(result.unsupportedCount), result.writable ? 1 : 0);
  return result;
}

void StorageManager::refreshBookPaths(bool includeMetadata) {
  if (!mounted_) {
    clearBookCache();
    return;
  }

  notifyStatus("SD", "Reading library", "", 96);
  bookPaths_ = collectBookPaths();
  if (includeMetadata) {
    rebuildBookMetadataCache();
  } else {
    bookTitles_.clear();
    bookAuthors_.clear();
    Serial.printf("[storage] Metadata cache skipped for %u entries\n",
                  static_cast<unsigned int>(bookPaths_.size()));
  }

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

void StorageManager::rebuildBookMetadataCache() {
  bookTitles_.clear();
  bookAuthors_.clear();
  bookTitles_.reserve(bookPaths_.size());
  bookAuthors_.reserve(bookPaths_.size());

  const uint32_t startedMs = millis();
  size_t rsvpMetadataCount = 0;
  for (const String &path : bookPaths_) {
    String title;
    String author;

    if (hasRsvpExtension(path)) {
      const RsvpDirectiveValues values = readRsvpDirectiveValues(path);
      title = values.title;
      author = values.author;
      ++rsvpMetadataCount;
    } else if (hasEpubExtension(path)) {
      author = epubLibraryLabel(path);
    }

    bookTitles_.push_back(title);
    bookAuthors_.push_back(author);
  }

  Serial.printf("[storage] Metadata cache: %u entries (%u rsvp) in %lu ms\n",
                static_cast<unsigned int>(bookPaths_.size()),
                static_cast<unsigned int>(rsvpMetadataCount),
                static_cast<unsigned long>(millis() - startedMs));
}

void StorageManager::clearBookCache() {
  bookPaths_.clear();
  bookTitles_.clear();
  bookAuthors_.clear();
}

bool StorageManager::parseFile(File &file, BookContent &book, bool rsvpFormat) {
  book.clear();
  const size_t fileBytes = file.size();
  Serial.printf("[storage] parseFile start size=%lu rsvp=%d free8=%lu largest8=%lu\n",
                static_cast<unsigned long>(fileBytes), rsvpFormat ? 1 : 0,
                static_cast<unsigned long>(heap_caps_get_free_size(MALLOC_CAP_8BIT)),
                static_cast<unsigned long>(heap_caps_get_largest_free_block(MALLOC_CAP_8BIT)));
  Serial.flush();
  // Estimate words generously (~5 bytes/word for ASCII text) and reserve up-front
  // so the vector never has to do a contiguous re-allocation mid-parse. Doubling
  // re-allocations fragment PSRAM and fail well before total free is exhausted.
  const size_t estimatedWords = static_cast<size_t>(fileBytes / 5);
  const size_t initialReserve = std::min(kInitialWordReserveMax, estimatedWords);
  String line;
  line.reserve(256);
  bool paragraphPending = true;
  bool keepReading = true;
  bool parseFailed = false;
  ParseStats stats;

  constexpr size_t kBufSize = 4096;
  static uint8_t buf[kBufSize];
  size_t totalBytesRead = 0;
  size_t nextHeapLogBytes = 256 * 1024;

  try {
    if (initialReserve > 0) {
      book.words.reserve(initialReserve);
    }
  } catch (const std::bad_alloc &) {
    Serial.printf("[storage] reserve(%u) failed free8=%lu largest8=%lu\n",
                  static_cast<unsigned int>(initialReserve),
                  static_cast<unsigned long>(heap_caps_get_free_size(MALLOC_CAP_8BIT)),
                  static_cast<unsigned long>(heap_caps_get_largest_free_block(MALLOC_CAP_8BIT)));
    book.clear();
    notifyStatus("Book too large", "Memory reserve failed", "Try converter/app", 100);
    return false;
  }

  try {
  while (keepReading && file.available()) {
    const size_t bytesRead = file.read(buf, kBufSize);
    if (bytesRead == 0) {
      break;
    }
    totalBytesRead += bytesRead;
    if (totalBytesRead >= nextHeapLogBytes) {
      Serial.printf("[storage] parse progress bytes=%lu/%lu words=%u free8=%lu largest8=%lu\n",
                    static_cast<unsigned long>(totalBytesRead),
                    static_cast<unsigned long>(fileBytes),
                    static_cast<unsigned int>(book.words.size()),
                    static_cast<unsigned long>(heap_caps_get_free_size(MALLOC_CAP_8BIT)),
                    static_cast<unsigned long>(heap_caps_get_largest_free_block(MALLOC_CAP_8BIT)));
      Serial.flush();
      nextHeapLogBytes += 256 * 1024;
    }
    yield();

    for (size_t i = 0; i < bytesRead && keepReading; ++i) {
      const char c = static_cast<char>(buf[i]);

      if (c == '\r') {
        continue;
      }

      if (c == '\n') {
        keepReading = rsvpFormat ? processRsvpLine(line, book, paragraphPending, &stats)
                                 : processBookLine(line, book, paragraphPending, &stats);
        if (!keepReading && hasBookWordLimit()) {
          Serial.printf("[storage] Reached %lu word limit, truncating book\n",
                        static_cast<unsigned long>(kMaxBookWords));
        } else if (!keepReading && stats.memoryLow) {
          parseFailed = true;
          Serial.printf("[storage] Book load stopped: low memory free8=%lu largest8=%lu\n",
                        static_cast<unsigned long>(heap_caps_get_free_size(MALLOC_CAP_8BIT)),
                        static_cast<unsigned long>(
                            heap_caps_get_largest_free_block(MALLOC_CAP_8BIT)));
        }
        line = "";
        continue;
      }

      line += c;
      if (line.length() >= kMaxBookLineChars) {
        keepReading = rsvpFormat ? processRsvpLine(line, book, paragraphPending, &stats)
                                 : processBookLine(line, book, paragraphPending, &stats);
        ++stats.longLineSplits;
        if (!keepReading && stats.memoryLow) {
          parseFailed = true;
          Serial.printf("[storage] Book load stopped: low memory free8=%lu largest8=%lu\n",
                        static_cast<unsigned long>(heap_caps_get_free_size(MALLOC_CAP_8BIT)),
                        static_cast<unsigned long>(
                            heap_caps_get_largest_free_block(MALLOC_CAP_8BIT)));
        }
        line = "";
      }
    }
  }

  if (!line.isEmpty() && keepReading && !reachedBookWordLimit(book.words.size())) {
    if (rsvpFormat) {
      keepReading = processRsvpLine(line, book, paragraphPending, &stats);
    } else {
      keepReading = processBookLine(line, book, paragraphPending, &stats);
    }
    if (!keepReading && stats.memoryLow) {
      parseFailed = true;
    }
  }
  } catch (const std::bad_alloc &) {
    Serial.printf("[storage] parse bad_alloc words=%u bytes=%lu free8=%lu largest8=%lu\n",
                  static_cast<unsigned int>(book.words.size()),
                  static_cast<unsigned long>(totalBytesRead),
                  static_cast<unsigned long>(heap_caps_get_free_size(MALLOC_CAP_8BIT)),
                  static_cast<unsigned long>(heap_caps_get_largest_free_block(MALLOC_CAP_8BIT)));
    book.clear();
    notifyStatus("Book too large", "Out of memory while parsing", "Try converter/app", 100);
    return false;
  }

  if (stats.longLineSplits > 0 || stats.malformedUtf8 > 0 || stats.nonAsciiCodepoints > 0) {
    Serial.printf("[storage] Parse cleanup: long_lines=%u malformed_utf8=%u non_ascii=%u\n",
                  static_cast<unsigned int>(stats.longLineSplits),
                  static_cast<unsigned int>(stats.malformedUtf8),
                  static_cast<unsigned int>(stats.nonAsciiCodepoints));
  }

  if (parseFailed) {
    book.clear();
    notifyStatus("Book too large", "Memory limit reached", "Try converter/app", 100);
    return false;
  }

  if (!book.words.empty() && book.paragraphStarts.empty()) {
    book.paragraphStarts.push_back(0);
  }

  return !book.words.empty();
}
