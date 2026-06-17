#include "rss/RssFeedManager.h"

#include <HTTPClient.h>
#include <SD_MMC.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <algorithm>
#include <vector>

#include "net/WifiConnection.h"
#include "rss/FeedParser.h"
#include "storage/fs/StorageFiles.h"
#include "storage/fs/StoragePaths.h"
#include "text/AsciiText.h"
#include "text/LatinText.h"

namespace {

constexpr const char *kConfigPaths[] = {
    "/config/rss.conf",
    "/rss.conf",
};
constexpr const char *kStatusTitle = "RSS";
constexpr uint32_t kFeedRequestTimeoutMs = 30000;
constexpr uint32_t kFeedTotalTimeoutMs = 90000;
constexpr uint32_t kFeedIdleTimeoutMs = 15000;
constexpr uint32_t kFeedProgressIntervalMs = 1000;
constexpr size_t kMaxFeedBytes = 4UL * 1024UL * 1024UL;
constexpr uint8_t kMaxFeedsPerCheck = 8;
constexpr uint8_t kMaxItemsPerFeed = 5;
constexpr uint8_t kMaxArticlesPerCheck = 12;
constexpr uint8_t kMaxFeedRedirects = 3;

bool ensureArticleDirectory() {
  return StorageFiles::ensureDirectory(StoragePaths::kBooksPath, "rss") &&
         StorageFiles::ensureDirectory(StoragePaths::kArticleFilesPath, "rss");
}

String trimCopy(String value) {
  value.trim();
  return value;
}

bool startsWithHttp(const String &url) {
  String lowered = trimCopy(url);
  lowered.toLowerCase();
  return lowered.startsWith("http://") || lowered.startsWith("https://");
}

bool isSafeFilenameChar(char c) {
  return AsciiText::isAlphaNumeric(c) || c == '-' || c == '_' || c == ' ' ||
         c == '.';
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
        base == 16 ? AsciiText::hexValue(entity[index])
                   : (AsciiText::isDigit(entity[index]) ? entity[index] - '0' : -1);
    if (digit < 0 || digit >= base) {
      return false;
    }
    codepoint = codepoint * base + static_cast<uint32_t>(digit);
  }

  return codepoint <= 0x10FFFF && !(codepoint >= 0xD800 && codepoint <= 0xDFFF);
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
      target += "GBP";
      return true;
    case 0x00A4:
      target += '$';
      return true;
    case 0x00A5:
      target += 'Y';
      return true;
    case 0x00A9:
      target += "(c)";
      return true;
    case 0x00AE:
      target += "(r)";
      return true;
    case 0x00B0:
      target += "deg";
      return true;
    case 0x00B1:
      target += "+/-";
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
      target += "1/4";
      return true;
    case 0x00BD:
      target += "1/2";
      return true;
    case 0x00BE:
      target += "3/4";
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
      target += " - ";
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
      target += "...";
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
    decoded += " - ";
    return true;
  }
  if (entity == "hellip") {
    decoded += "...";
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
    if (index + i >= text.length() ||
        AsciiText::toLower(text[index + i]) != AsciiText::toLower(needle[i])) {
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

String userAgent() { return String("RSVP-Nano-RSS/1.0"); }

String feedProgressLabel(uint8_t feedIndex, uint8_t feedCount) {
  return "Feed " + String(feedIndex) + "/" + String(feedCount);
}

bool isRedirectStatus(int statusCode) {
  return statusCode == HTTP_CODE_MOVED_PERMANENTLY || statusCode == HTTP_CODE_FOUND ||
         statusCode == HTTP_CODE_SEE_OTHER || statusCode == HTTP_CODE_TEMPORARY_REDIRECT ||
         statusCode == HTTP_CODE_PERMANENT_REDIRECT;
}

bool hasCompleteFeedCloseTag(const String &body, size_t searchStart = 0) {
  return indexOfIgnoreCase(body, "</rss>", searchStart, body.length()) >= 0 ||
         indexOfIgnoreCase(body, "</feed>", searchStart, body.length()) >= 0;
}

bool advancePastCompleteFeedItem(const String &body, size_t &searchStart) {
  const int itemStart = indexOfIgnoreCase(body, "<item", searchStart, body.length());
  const int entryStart = indexOfIgnoreCase(body, "<entry", searchStart, body.length());
  if (itemStart < 0 && entryStart < 0) {
    return false;
  }

  const bool atom = itemStart < 0 || (entryStart >= 0 && entryStart < itemStart);
  const int blockStart = atom ? entryStart : itemStart;
  const char *closeTag = atom ? "</entry>" : "</item>";
  const int blockEnd = indexOfIgnoreCase(body, closeTag, blockStart, body.length());
  if (blockEnd < 0) {
    return false;
  }

  searchStart = static_cast<size_t>(blockEnd + strlen(closeTag));
  return true;
}

String friendlyHttpError(int statusCode) {
  switch (statusCode) {
    case HTTPC_ERROR_CONNECTION_REFUSED:
      return "Could not reach feed";
    case HTTPC_ERROR_SEND_HEADER_FAILED:
    case HTTPC_ERROR_SEND_PAYLOAD_FAILED:
      return "Connection failed";
    case HTTPC_ERROR_NOT_CONNECTED:
      return "Wi-Fi dropped out";
    case HTTPC_ERROR_CONNECTION_LOST:
      return "Connection was lost";
    case HTTPC_ERROR_NO_STREAM:
      return "No data from site";
    case HTTPC_ERROR_NO_HTTP_SERVER:
      return "Not a web feed";
    case HTTPC_ERROR_TOO_LESS_RAM:
      return "Feed is too large";
    case HTTPC_ERROR_ENCODING:
      return "Feed format not supported";
    case HTTPC_ERROR_STREAM_WRITE:
      return "Could not read feed";
    case HTTPC_ERROR_READ_TIMEOUT:
      return "Site took too long";
  }

  switch (statusCode) {
    case HTTP_CODE_BAD_REQUEST:
      return "Feed link looks wrong";
    case HTTP_CODE_UNAUTHORIZED:
      return "Feed needs login";
    case HTTP_CODE_FORBIDDEN:
      return "Site blocked reader";
    case HTTP_CODE_NOT_FOUND:
      return "Feed not found";
    case HTTP_CODE_REQUEST_TIMEOUT:
      return "Site took too long";
    case HTTP_CODE_TOO_MANY_REQUESTS:
      return "Site says try later";
  }

  if (statusCode >= 500 && statusCode < 600) {
    return "Site is having trouble";
  }
  if (statusCode >= 300 && statusCode < 400) {
    return "Feed moved unexpectedly";
  }
  return "Could not download feed";
}

String urlScheme(const String &url) {
  const int marker = url.indexOf("://");
  if (marker < 0) {
    return "http";
  }
  return url.substring(0, marker);
}

String urlOrigin(const String &url) {
  const int marker = url.indexOf("://");
  const int hostStart = marker < 0 ? 0 : marker + 3;
  int hostEnd = url.indexOf('/', hostStart);
  if (hostEnd < 0) {
    hostEnd = url.length();
  }
  return url.substring(0, hostEnd);
}

String resolveRedirectUrl(const String &baseUrl, String location) {
  location.trim();
  if (location.startsWith("http://") || location.startsWith("https://")) {
    return location;
  }
  if (location.startsWith("//")) {
    return urlScheme(baseUrl) + ":" + location;
  }
  if (location.startsWith("/")) {
    return urlOrigin(baseUrl) + location;
  }

  int slash = baseUrl.lastIndexOf('/');
  const int marker = baseUrl.indexOf("://");
  if (slash <= marker + 2) {
    return urlOrigin(baseUrl) + "/" + location;
  }
  return baseUrl.substring(0, slash + 1) + location;
}

}  // namespace

RssFeedManager::Result RssFeedManager::checkFeeds(const OtaUpdater::Config &wifiConfig,
                                                  Preferences &preferences,
                                                  StatusCallback callback, void *context) {
  Result result;
  if (trimCopy(wifiConfig.wifiSsid).isEmpty()) {
    result.summary = "Wi-Fi not set";
    result.detail = "Settings -> Wi-Fi";
    return result;
  }

  if (!connectWiFi(wifiConfig, callback, context)) {
    disconnectWiFi();
    result.summary = "Wi-Fi failed";
    result.detail = "Check credentials";
    return result;
  }

  File config;
  for (const char *path : kConfigPaths) {
    config = SD_MMC.open(path);
    if (config && !config.isDirectory()) {
      break;
    }
    if (config) {
      config.close();
    }
  }

  if (!config || config.isDirectory()) {
    if (config) {
      config.close();
    }
    disconnectWiFi();
    result.summary = "No feeds";
    result.detail = "/config/rss.conf";
    return result;
  }

  std::vector<String> feeds;
  feeds.reserve(kMaxFeedsPerCheck);
  while (config.available() && feeds.size() < kMaxFeedsPerCheck) {
    String line = config.readStringUntil('\n');
    line.trim();
    if (line.isEmpty() || line.startsWith("#")) {
      continue;
    }
    if (line.startsWith("feed=")) {
      line = line.substring(5);
      line.trim();
    }
    if (!startsWithHttp(line)) {
      continue;
    }

    feeds.push_back(line);
  }
  config.close();

  if (feeds.empty()) {
    disconnectWiFi();
    result.summary = "No feed URLs";
    result.detail = "/config/rss.conf";
    return result;
  }

  uint8_t feedFailures = 0;
  String firstFeedError;
  bool mixedFeedErrors = false;

  for (uint8_t feedIndex = 0;
       feedIndex < feeds.size() && result.articlesSaved < kMaxArticlesPerCheck; ++feedIndex) {
    const String &line = feeds[feedIndex];
    const uint8_t displayIndex = feedIndex + 1;
    const uint8_t feedCount = feeds.size();
    report(callback, context, feedProgressLabel(displayIndex, feedCount),
           "Downloading " + feedparser::hostLabelForUrl(line), 15 + displayIndex * 8);

    String feedBody;
    String error;
    if (!fetchUrl(line, feedBody, error, displayIndex, feedCount, callback, context)) {
      Serial.printf("[rss] feed failed url=%s error=%s\n", line.c_str(), error.c_str());
      ++feedFailures;
      if (firstFeedError.isEmpty()) {
        firstFeedError = error;
      } else if (error != firstFeedError) {
        mixedFeedErrors = true;
      }
      report(callback, context, feedProgressLabel(displayIndex, feedCount), "Skipped: " + error,
             15 + displayIndex * 8);
      delay(600);
      continue;
    }

    ++result.feedsChecked;
    processFeed(line, feedBody, preferences, result, displayIndex, feedCount, callback, context);
  }

  disconnectWiFi();

  if (result.feedsChecked == 0) {
    result.summary = "Feeds unavailable";
    result.detail =
        mixedFeedErrors || firstFeedError.isEmpty() ? "Check feed URLs" : firstFeedError;
  } else if (result.articlesSaved == 0) {
    result.summary = "No new articles";
    result.detail = String(result.feedsChecked) + " checked";
    if (feedFailures > 0) {
      result.detail += ", " + String(feedFailures) + " failed";
    }
  } else {
    result.summary = String(result.articlesSaved) + " article" +
                     (result.articlesSaved == 1 ? "" : "s") + " saved";
    result.detail = String(result.feedsChecked) + " checked";
    if (feedFailures > 0) {
      result.detail += ", " + String(feedFailures) + " failed";
    }
  }
  return result;
}

bool RssFeedManager::connectWiFi(const OtaUpdater::Config &wifiConfig, StatusCallback callback,
                                 void *context) {
  return net::connectStation(wifiConfig.wifiSsid, wifiConfig.wifiPassword, [&](int percent) {
    report(callback, context, "Connecting Wi-Fi", wifiConfig.wifiSsid, percent);
  });
}

void RssFeedManager::disconnectWiFi() { net::disconnect(); }

bool RssFeedManager::fetchUrl(const String &url, String &body, String &error,
                              uint8_t feedIndex, uint8_t feedCount,
                              StatusCallback callback, void *context) {
  String currentUrl = url;
  for (uint8_t redirectCount = 0; redirectCount <= kMaxFeedRedirects; ++redirectCount) {
    WiFiClientSecure secureClient;
    WiFiClient plainClient;
    secureClient.setInsecure();
    secureClient.setHandshakeTimeout(15);

    HTTPClient http;
    http.setUserAgent(userAgent());
    http.setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS);
    http.setTimeout(kFeedRequestTimeoutMs);
    const char *headers[] = {"Location"};
    http.collectHeaders(headers, 1);

    const bool ok = currentUrl.startsWith("https://") ? http.begin(secureClient, currentUrl)
                                                      : http.begin(plainClient, currentUrl);
    if (!ok) {
      error = "Feed link did not open";
      Serial.printf("[rss] begin failed url=%s\n", currentUrl.c_str());
      return false;
    }

    report(callback, context, feedProgressLabel(feedIndex, feedCount),
           "Requesting " + feedparser::hostLabelForUrl(currentUrl), 18 + feedIndex * 7);
    const int statusCode = http.GET();
    if (isRedirectStatus(statusCode)) {
      String location = http.header("Location");
      http.end();
      if (location.isEmpty()) {
        error = "Feed moved but gave no link";
        Serial.printf("[rss] redirect missing location status=%d url=%s\n", statusCode,
                      currentUrl.c_str());
        return false;
      }
      currentUrl = resolveRedirectUrl(currentUrl, location);
      Serial.printf("[rss] redirect %u url=%s\n", static_cast<unsigned int>(statusCode),
                    currentUrl.c_str());
      report(callback, context, feedProgressLabel(feedIndex, feedCount),
             "Redirecting to " + feedparser::hostLabelForUrl(currentUrl), 18 + feedIndex * 7);
      delay(250);
      continue;
    }
    if (statusCode != HTTP_CODE_OK) {
      error = friendlyHttpError(statusCode);
      Serial.printf("[rss] http failed status=%d message=%s url=%s\n", statusCode, error.c_str(),
                    currentUrl.c_str());
      http.end();
      return false;
    }

    WiFiClient *stream = http.getStreamPtr();
    if (stream == nullptr) {
      error = "No data from site";
      Serial.printf("[rss] no stream url=%s\n", currentUrl.c_str());
      http.end();
      return false;
    }

    uint8_t buffer[512];
    size_t totalRead = 0;
    size_t completeItemSearchStart = 0;
    uint8_t completeItemsRead = 0;
    bool stoppedAfterItems = false;
    bool acceptedPartialFeed = false;
    const int reportedSize = http.getSize();
    const size_t reserveBytes =
        reportedSize > 0 ? std::min(static_cast<size_t>(reportedSize), kMaxFeedBytes) : 8192;
    body = "";
    body.reserve(reserveBytes);
    const uint32_t startedMs = millis();
    uint32_t lastByteMs = startedMs;
    uint32_t lastReportMs = 0;
    while (http.connected() || stream->available()) {
      const uint32_t nowMs = millis();
      if (nowMs - startedMs > kFeedTotalTimeoutMs) {
        if (completeItemsRead > 0) {
          acceptedPartialFeed = true;
          Serial.printf("[rss] total timeout after usable items url=%s bytes=%u items=%u\n",
                        currentUrl.c_str(), static_cast<unsigned int>(totalRead),
                        static_cast<unsigned int>(completeItemsRead));
          break;
        }
        error = "Site took too long";
        Serial.printf("[rss] total timeout url=%s bytes=%u\n", currentUrl.c_str(),
                      static_cast<unsigned int>(totalRead));
        http.end();
        return false;
      }
      if (nowMs - lastByteMs > kFeedIdleTimeoutMs) {
        if (completeItemsRead > 0) {
          acceptedPartialFeed = true;
          Serial.printf("[rss] idle after usable items url=%s bytes=%u items=%u\n",
                        currentUrl.c_str(), static_cast<unsigned int>(totalRead),
                        static_cast<unsigned int>(completeItemsRead));
          break;
        }
        if (totalRead > 0 && hasCompleteFeedCloseTag(body)) {
          Serial.printf("[rss] idle after complete feed url=%s bytes=%u\n", currentUrl.c_str(),
                        static_cast<unsigned int>(totalRead));
          break;
        }
        error = "Site stopped sending data";
        Serial.printf("[rss] idle timeout url=%s bytes=%u\n", currentUrl.c_str(),
                      static_cast<unsigned int>(totalRead));
        http.end();
        return false;
      }
      if (nowMs - lastReportMs >= kFeedProgressIntervalMs) {
        lastReportMs = nowMs;
        report(callback, context, feedProgressLabel(feedIndex, feedCount),
               "Downloaded " + String(static_cast<unsigned int>(totalRead / 1024)) + " KB",
               20 + feedIndex * 7);
      }
      if (reportedSize > 0 && totalRead >= static_cast<size_t>(reportedSize)) {
        break;
      }
      const int available = stream->available();
      if (available <= 0) {
        delay(1);
        continue;
      }
      const size_t remaining = kMaxFeedBytes - totalRead;
      if (remaining == 0) {
        break;
      }
      const size_t chunkSize =
          std::min(remaining, std::min(sizeof(buffer), static_cast<size_t>(available)));
      const int bytesRead = stream->readBytes(buffer, chunkSize);
      if (bytesRead <= 0) {
        break;
      }
      lastByteMs = millis();
      const size_t previousRead = totalRead;
      totalRead += static_cast<size_t>(bytesRead);
      for (int i = 0; i < bytesRead; ++i) {
        body += static_cast<char>(buffer[i]);
      }
      while (completeItemsRead < kMaxItemsPerFeed &&
             advancePastCompleteFeedItem(body, completeItemSearchStart)) {
        ++completeItemsRead;
      }
      if (completeItemsRead >= kMaxItemsPerFeed) {
        stoppedAfterItems = true;
        Serial.printf("[rss] downloaded item limit url=%s bytes=%u items=%u\n",
                      currentUrl.c_str(), static_cast<unsigned int>(totalRead),
                      static_cast<unsigned int>(completeItemsRead));
        break;
      }
      const size_t closeSearchStart = previousRead > 16 ? previousRead - 16 : 0;
      if (hasCompleteFeedCloseTag(body, closeSearchStart)) {
        break;
      }
    }
    http.end();

    if (body.isEmpty()) {
      error = "Feed was empty";
      Serial.printf("[rss] empty response url=%s\n", currentUrl.c_str());
      return false;
    }
    if (totalRead >= kMaxFeedBytes) {
      Serial.printf("[rss] feed capped url=%s bytes=%u\n", currentUrl.c_str(),
                    static_cast<unsigned int>(totalRead));
      report(callback, context, feedProgressLabel(feedIndex, feedCount),
             "Reached " + String(static_cast<unsigned int>(kMaxFeedBytes / 1024)) + " KB cap",
             20 + feedIndex * 7);
      delay(500);
    } else if (stoppedAfterItems) {
      report(callback, context, feedProgressLabel(feedIndex, feedCount),
             "Downloaded " + String(static_cast<unsigned int>(completeItemsRead)) + " items",
             20 + feedIndex * 7);
    } else if (acceptedPartialFeed) {
      report(callback, context, feedProgressLabel(feedIndex, feedCount),
             "Downloaded partial feed", 20 + feedIndex * 7);
    } else {
      report(callback, context, feedProgressLabel(feedIndex, feedCount),
             "Downloaded " + String(static_cast<unsigned int>(totalRead / 1024)) + " KB",
             20 + feedIndex * 7);
    }
    return true;
  }

  error = "Feed redirected too often";
  Serial.printf("[rss] too many redirects url=%s\n", url.c_str());
  return false;
}

bool RssFeedManager::processFeed(const String &feedUrl, const String &feedBody,
                                 Preferences &preferences, Result &result,
                                 uint8_t feedIndex, uint8_t feedCount,
                                 StatusCallback callback, void *context) {
  size_t searchStart = 0;
  uint8_t itemCount = 0;
  uint8_t savedBefore = result.articlesSaved;
  uint8_t skippedBefore = result.articlesSkipped;
  report(callback, context, feedProgressLabel(feedIndex, feedCount), "Parsing items",
         24 + feedIndex * 7);
  while (itemCount < kMaxItemsPerFeed && result.articlesSaved < kMaxArticlesPerCheck) {
    feedparser::FeedItem item;
    if (!feedparser::parseNextItem(feedBody, searchStart, item)) {
      break;
    }
    ++itemCount;
    if (itemAlreadySeen(item, preferences)) {
      ++result.articlesSkipped;
      report(callback, context, feedProgressLabel(feedIndex, feedCount),
             "Already synced " + String(itemCount) + "/" + String(kMaxItemsPerFeed),
             24 + feedIndex * 7);
      continue;
    }
    report(callback, context, "Saving article " + String(itemCount), item.title,
           24 + feedIndex * 7);
    saveItem(item, preferences, result);
  }
  const uint8_t savedHere = result.articlesSaved - savedBefore;
  const uint8_t skippedHere = result.articlesSkipped - skippedBefore;
  if (itemCount == 0) {
    report(callback, context, feedProgressLabel(feedIndex, feedCount), "No usable items",
           24 + feedIndex * 7);
  } else {
    report(callback, context, feedProgressLabel(feedIndex, feedCount),
           String(savedHere) + " saved, " + String(skippedHere) + " skipped",
           24 + feedIndex * 7);
  }
  Serial.printf("[rss] feed url=%s items=%u saved=%u skipped=%u\n", feedUrl.c_str(),
                static_cast<unsigned int>(itemCount), static_cast<unsigned int>(savedHere),
                static_cast<unsigned int>(skippedHere));
  delay(600);
  return itemCount > 0;
}

bool RssFeedManager::saveItem(const feedparser::FeedItem &item, Preferences &preferences,
                              Result &result) {
  if (!ensureArticleDirectory()) {
    Serial.println("[rss] article folder unavailable");
    return false;
  }
  const String finalPath =
      String(StoragePaths::kArticleFilesPath) + "/" + filenameForItem(item);
  const String tmpPath = finalPath + ".tmp";
  SD_MMC.remove(tmpPath);

  File file = SD_MMC.open(tmpPath, FILE_WRITE);
  if (!file) {
    Serial.printf("[rss] could not create %s\n", tmpPath.c_str());
    return false;
  }

  file.println("@rsvp 1");
  file.print("@title ");
  file.println(metadataSafe(item.title));
  file.print("@author ");
  file.println(
      metadataSafe(item.author.isEmpty() ? feedparser::sourceLabelForItem(item) : item.author));
  if (!item.link.isEmpty()) {
    file.print("@source ");
    file.println(metadataSafe(item.link));
  }
  file.println();

  String body = item.body;
  if (body.length() > feedparser::kMaxArticleChars) {
    body = body.substring(0, feedparser::kMaxArticleChars);
    body += "\n\n[Article truncated on device.]";
  }
  file.println(body);
  file.close();

  SD_MMC.remove(finalPath);
  if (!SD_MMC.rename(tmpPath, finalPath)) {
    SD_MMC.remove(tmpPath);
    Serial.printf("[rss] rename failed %s\n", finalPath.c_str());
    return false;
  }

  markItemSeen(item, preferences);
  ++result.articlesSaved;
  Serial.printf("[rss] saved %s\n", finalPath.c_str());
  return true;
}

bool RssFeedManager::itemAlreadySeen(const feedparser::FeedItem &item, Preferences &preferences) {
  return preferences.getBool(seenKeyForItem(item).c_str(), false);
}

void RssFeedManager::markItemSeen(const feedparser::FeedItem &item, Preferences &preferences) {
  preferences.putBool(seenKeyForItem(item).c_str(), true);
}

String RssFeedManager::seenKeyForItem(const feedparser::FeedItem &item) const {
  char key[16];
  std::snprintf(key, sizeof(key), "rss%08lx", static_cast<unsigned long>(fnv1a(itemIdentity(item))));
  return String(key);
}

String RssFeedManager::itemIdentity(const feedparser::FeedItem &item) const {
  return item.link.isEmpty() ? item.title : item.link;
}

String RssFeedManager::filenameForItem(const feedparser::FeedItem &item) const {
  String name = item.title;  // already HTML-stripped and entity-decoded by FeedParser
  String cleaned;
  cleaned.reserve(80);
  for (size_t i = 0; i < name.length() && cleaned.length() < 72; ++i) {
    const char c = name[i];
    cleaned += isSafeFilenameChar(c) ? c : '-';
  }
  cleaned.trim();
  while (cleaned.indexOf("--") >= 0) {
    cleaned.replace("--", "-");
  }
  if (cleaned.isEmpty()) {
    cleaned = "rss-article";
  }
  char suffix[16];
  std::snprintf(suffix, sizeof(suffix), "-%08lx", static_cast<unsigned long>(fnv1a(itemIdentity(item))));
  return cleaned + suffix + ".rsvp";
}

String RssFeedManager::metadataSafe(String value) const {
  value.replace("\r", " ");
  value.replace("\n", " ");
  value.trim();
  return value;
}

uint32_t RssFeedManager::fnv1a(const String &value) const {
  uint32_t hash = 2166136261UL;
  for (size_t i = 0; i < value.length(); ++i) {
    hash ^= static_cast<uint8_t>(value[i]);
    hash *= 16777619UL;
  }
  return hash;
}

void RssFeedManager::report(StatusCallback callback, void *context, const String &line1,
                            const String &line2, int progressPercent) {
  if (callback == nullptr) {
    return;
  }
  callback(context, kStatusTitle, line1.c_str(), line2.c_str(), progressPercent);
}
