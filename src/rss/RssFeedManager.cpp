#include "rss/RssFeedManager.h"

#include <HTTPClient.h>
#include <SD_MMC.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <algorithm>
#include <vector>

namespace {

constexpr const char *kConfigPaths[] = {
    "/config/rss.conf",
    "/rss.conf",
};
constexpr const char *kBooksPath = "/books";
constexpr const char *kArticleFilesPath = "/books/articles";
constexpr const char *kStatusTitle = "RSS";
constexpr uint32_t kWifiConnectTimeoutMs = 15000;
constexpr uint32_t kWifiConnectPollMs = 250;
constexpr uint32_t kFeedTotalTimeoutMs = 30000;
constexpr uint32_t kFeedIdleTimeoutMs = 5000;
constexpr uint32_t kFeedProgressIntervalMs = 1000;
constexpr size_t kMaxFeedBytes = 393216;
constexpr size_t kMaxArticleChars = 24000;
constexpr uint8_t kMaxFeedsPerCheck = 8;
constexpr uint8_t kMaxItemsPerFeed = 5;
constexpr uint8_t kMaxArticlesPerCheck = 12;
constexpr uint8_t kMaxFeedRedirects = 3;

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
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
         c == '-' || c == '_' || c == ' ' || c == '.';
}

char lowerAscii(char c) {
  return (c >= 'A' && c <= 'Z') ? static_cast<char>(c + ('a' - 'A')) : c;
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

String userAgent() { return String("RSVP-Nano-RSS/1.0"); }

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

String feedProgressLabel(uint8_t feedIndex, uint8_t feedCount) {
  return "Feed " + String(feedIndex) + "/" + String(feedCount);
}

bool isRedirectStatus(int statusCode) {
  return statusCode == HTTP_CODE_MOVED_PERMANENTLY || statusCode == HTTP_CODE_FOUND ||
         statusCode == HTTP_CODE_SEE_OTHER || statusCode == HTTP_CODE_TEMPORARY_REDIRECT ||
         statusCode == HTTP_CODE_PERMANENT_REDIRECT;
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

  for (uint8_t feedIndex = 0;
       feedIndex < feeds.size() && result.articlesSaved < kMaxArticlesPerCheck; ++feedIndex) {
    const String &line = feeds[feedIndex];
    const uint8_t displayIndex = feedIndex + 1;
    const uint8_t feedCount = feeds.size();
    report(callback, context, feedProgressLabel(displayIndex, feedCount),
           "Downloading " + hostLabelForUrl(line), 15 + displayIndex * 8);

    String feedBody;
    String error;
    if (!fetchUrl(line, feedBody, error, displayIndex, feedCount, callback, context)) {
      Serial.printf("[rss] feed failed url=%s error=%s\n", line.c_str(), error.c_str());
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
    result.summary = "No feeds checked";
    result.detail = "/config/rss.conf";
  } else if (result.articlesSaved == 0) {
    result.summary = "No new articles";
    result.detail = String(result.feedsChecked) + " feeds checked";
  } else {
    result.summary = String(result.articlesSaved) + " article" +
                     (result.articlesSaved == 1 ? "" : "s") + " saved";
    result.detail = String(result.feedsChecked) + " feeds checked";
  }
  return result;
}

bool RssFeedManager::connectWiFi(const OtaUpdater::Config &wifiConfig, StatusCallback callback,
                                 void *context) {
  WiFi.persistent(false);
  WiFi.setAutoReconnect(false);
  WiFi.mode(WIFI_STA);
  WiFi.begin(wifiConfig.wifiSsid.c_str(), wifiConfig.wifiPassword.c_str());

  const uint32_t startMs = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startMs < kWifiConnectTimeoutMs) {
    const uint32_t elapsedMs = millis() - startMs;
    const int progress = 5 + static_cast<int>((elapsedMs * 12) / kWifiConnectTimeoutMs);
    report(callback, context, "Connecting Wi-Fi", wifiConfig.wifiSsid, progress);
    delay(kWifiConnectPollMs);
  }

  return WiFi.status() == WL_CONNECTED;
}

void RssFeedManager::disconnectWiFi() {
  WiFi.disconnect(true, false);
  WiFi.mode(WIFI_OFF);
}

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
    http.setTimeout(15000);
    const char *headers[] = {"Location"};
    http.collectHeaders(headers, 1);

    const bool ok = currentUrl.startsWith("https://") ? http.begin(secureClient, currentUrl)
                                                      : http.begin(plainClient, currentUrl);
    if (!ok) {
      error = "HTTP begin failed";
      return false;
    }

    report(callback, context, feedProgressLabel(feedIndex, feedCount),
           "Requesting " + hostLabelForUrl(currentUrl), 18 + feedIndex * 7);
    const int statusCode = http.GET();
    if (isRedirectStatus(statusCode)) {
      String location = http.header("Location");
      http.end();
      if (location.isEmpty()) {
        error = "Redirect missing location";
        return false;
      }
      currentUrl = resolveRedirectUrl(currentUrl, location);
      Serial.printf("[rss] redirect %u url=%s\n", static_cast<unsigned int>(statusCode),
                    currentUrl.c_str());
      report(callback, context, feedProgressLabel(feedIndex, feedCount),
             "Redirecting to " + hostLabelForUrl(currentUrl), 18 + feedIndex * 7);
      delay(250);
      continue;
    }
    if (statusCode != HTTP_CODE_OK) {
      error = "HTTP " + String(statusCode);
      http.end();
      return false;
    }

    WiFiClient *stream = http.getStreamPtr();
    if (stream == nullptr) {
      error = "No stream";
      http.end();
      return false;
    }

    body = "";
    body.reserve(8192);
    uint8_t buffer[512];
    size_t totalRead = 0;
    const int reportedSize = http.getSize();
    const uint32_t startedMs = millis();
    uint32_t lastByteMs = startedMs;
    uint32_t lastReportMs = 0;
    while (http.connected() || stream->available()) {
      const uint32_t nowMs = millis();
      if (nowMs - startedMs > kFeedTotalTimeoutMs) {
        error = "Timed out";
        http.end();
        return false;
      }
      if (nowMs - lastByteMs > kFeedIdleTimeoutMs) {
        error = "No data";
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
      totalRead += static_cast<size_t>(bytesRead);
      for (int i = 0; i < bytesRead; ++i) {
        body += static_cast<char>(buffer[i]);
      }
    }
    http.end();

    if (body.isEmpty()) {
      error = "Empty response";
      return false;
    }
    if (totalRead >= kMaxFeedBytes) {
      Serial.printf("[rss] feed capped url=%s bytes=%u\n", currentUrl.c_str(),
                    static_cast<unsigned int>(totalRead));
      report(callback, context, feedProgressLabel(feedIndex, feedCount),
             "Reached " + String(static_cast<unsigned int>(kMaxFeedBytes / 1024)) + " KB cap",
             20 + feedIndex * 7);
      delay(500);
    } else {
      report(callback, context, feedProgressLabel(feedIndex, feedCount),
             "Downloaded " + String(static_cast<unsigned int>(totalRead / 1024)) + " KB",
             20 + feedIndex * 7);
    }
    return true;
  }

  error = "Too many redirects";
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
    FeedItem item;
    if (!parseNextItem(feedBody, searchStart, item)) {
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

bool RssFeedManager::parseNextItem(const String &feedBody, size_t &searchStart, FeedItem &item) {
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

bool RssFeedManager::saveItem(const FeedItem &item, Preferences &preferences, Result &result) {
  SD_MMC.mkdir(kBooksPath);
  SD_MMC.mkdir(kArticleFilesPath);
  const String finalPath = String(kArticleFilesPath) + "/" + filenameForItem(item);
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
  file.println(metadataSafe(item.author.isEmpty() ? sourceLabelForItem(item) : item.author));
  if (!item.link.isEmpty()) {
    file.print("@source ");
    file.println(metadataSafe(item.link));
  }
  file.println();

  String body = item.body;
  if (body.length() > kMaxArticleChars) {
    body = body.substring(0, kMaxArticleChars);
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

bool RssFeedManager::itemAlreadySeen(const FeedItem &item, Preferences &preferences) {
  return preferences.getBool(seenKeyForItem(item).c_str(), false);
}

void RssFeedManager::markItemSeen(const FeedItem &item, Preferences &preferences) {
  preferences.putBool(seenKeyForItem(item).c_str(), true);
}

String RssFeedManager::seenKeyForItem(const FeedItem &item) const {
  char key[16];
  std::snprintf(key, sizeof(key), "rss%08lx", static_cast<unsigned long>(fnv1a(itemIdentity(item))));
  return String(key);
}

String RssFeedManager::itemIdentity(const FeedItem &item) const {
  return item.link.isEmpty() ? item.title : item.link;
}

String RssFeedManager::valueBetween(const String &text, const String &openTag,
                                    const String &closeTag, size_t start, size_t end) const {
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

String RssFeedManager::attributeValue(const String &text, const String &tagPrefix,
                                      const String &attribute, size_t start, size_t end) const {
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

String RssFeedManager::cleanText(String value) const {
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

String RssFeedManager::stripHtml(const String &html) const {
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

String RssFeedManager::xmlDecode(String value) const {
  value.replace("&amp;", "&");
  value.replace("&lt;", "<");
  value.replace("&gt;", ">");
  value.replace("&quot;", "\"");
  value.replace("&apos;", "'");
  value.replace("&rsquo;", "'");
  value.replace("&lsquo;", "'");
  value.replace("&rdquo;", "\"");
  value.replace("&ldquo;", "\"");
  value.replace("&ndash;", "-");
  value.replace("&mdash;", "-");
  value.replace("&hellip;", "...");
  value.replace("&#39;", "'");
  value.replace("&#8217;", "'");
  value.replace("&#8216;", "'");
  value.replace("&#8220;", "\"");
  value.replace("&#8221;", "\"");
  value.replace("&#8211;", "-");
  value.replace("&#8212;", "-");
  value.replace("&#8230;", "...");
  value.replace("&nbsp;", " ");
  return value;
}

String RssFeedManager::sourceLabelForItem(const FeedItem &item) const {
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

String RssFeedManager::filenameForItem(const FeedItem &item) const {
  String name = xmlDecode(item.title);
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
