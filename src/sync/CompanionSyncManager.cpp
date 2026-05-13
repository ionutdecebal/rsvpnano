#include "sync/CompanionSyncManager.h"

#include <ESPmDNS.h>
#include <SD_MMC.h>
#include <WiFi.h>
#include <algorithm>
#include <cstdio>
#include <vector>

namespace {

constexpr const char *kMdnsName = "rsvp-nano";
constexpr const char *kBooksPath = "/books";
constexpr const char *kBookFilesPath = "/books/books";
constexpr const char *kArticleFilesPath = "/books/articles";
constexpr const char *kConfigPath = "/config";
constexpr const char *kRssConfigPath = "/config/rss.conf";
constexpr const char *kPrefsNamespace = "rsvp";
constexpr size_t kMaxMetadataLineChars = 160;
constexpr size_t kMaxSettingsPatchBytes = 2048;
constexpr size_t kMaxRssFeedsPatchBytes = 4096;
constexpr size_t kMaxRssFeeds = 24;
constexpr const char *kPrefWpm = "wpm";
constexpr const char *kPrefBrightness = "bright";
constexpr const char *kPrefDarkMode = "dark";
constexpr const char *kPrefNightMode = "night";
constexpr const char *kPrefUiLanguage = "ui_lang";
constexpr const char *kPrefReaderMode = "read_mode";
constexpr const char *kPrefHandedness = "handed";
constexpr const char *kPrefPhantomWords = "phantom_on";
constexpr const char *kPrefFooterMetricMode = "prog_md";
constexpr const char *kPrefBatteryLabelMode = "bat_md";
constexpr const char *kPrefReaderFontSize = "font_size";
constexpr const char *kPrefReaderTypeface = "typeface";
constexpr const char *kPrefTypographyFocusHighlight = "type_hlt";
constexpr const char *kPrefPacingLongMs = "pace_lms";
constexpr const char *kPrefPacingComplexMs = "pace_cms";
constexpr const char *kPrefPacingPunctuationMs = "pace_pms";
constexpr const char *kPrefPauseMode = "pause_md";
constexpr const char *kPrefAccurateTime = "time_est_a";
constexpr const char *kPrefTypographyTracking = "type_trk";
constexpr const char *kPrefTypographyAnchor = "type_anc";
constexpr const char *kPrefTypographyGuideWidth = "type_wid";
constexpr const char *kPrefTypographyGuideGap = "type_gap";
constexpr uint16_t kDefaultWpm = 300;
constexpr uint16_t kMinWpm = 100;
constexpr uint16_t kMaxWpm = 1000;
constexpr uint8_t kDefaultBrightness = 3;
constexpr uint8_t kMaxBrightness = 4;
constexpr uint8_t kMaxUiLanguage = 1;
constexpr uint8_t kMaxReaderMode = 1;
constexpr uint8_t kMaxHandedness = 1;
constexpr uint8_t kMaxFooterMetric = 2;
constexpr uint8_t kMaxBatteryLabel = 1;
constexpr uint8_t kMaxReaderFontSize = 2;
constexpr uint8_t kMaxReaderTypeface = 2;
constexpr uint8_t kMaxPauseMode = 1;
constexpr uint16_t kDefaultPacingDelayMs = 200;
constexpr uint16_t kMaxPacingDelayMs = 600;
constexpr int8_t kMinTypographyTracking = -2;
constexpr int8_t kMaxTypographyTracking = 3;
constexpr uint8_t kMinTypographyAnchor = 30;
constexpr uint8_t kMaxTypographyAnchor = 40;
constexpr uint8_t kDefaultTypographyAnchor = 30;
constexpr uint8_t kMinTypographyGuideWidth = 12;
constexpr uint8_t kMaxTypographyGuideWidth = 30;
constexpr uint8_t kDefaultTypographyGuideWidth = 30;
constexpr uint8_t kMinTypographyGuideGap = 2;
constexpr uint8_t kMaxTypographyGuideGap = 8;
constexpr uint8_t kDefaultTypographyGuideGap = 5;

bool isSafeFilenameChar(char c) {
  return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
         c == '-' || c == '_' || c == '.' || c == ' ';
}

String ipToString(IPAddress ip) {
  return String(ip[0]) + "." + String(ip[1]) + "." + String(ip[2]) + "." + String(ip[3]);
}

String stripBom(String value) {
  if (value.length() >= 3 && static_cast<uint8_t>(value[0]) == 0xEF &&
      static_cast<uint8_t>(value[1]) == 0xBB && static_cast<uint8_t>(value[2]) == 0xBF) {
    value.remove(0, 3);
  }
  return value;
}

bool directiveMatches(const String &loweredLine, const char *directive) {
  if (!loweredLine.startsWith(directive)) {
    return false;
  }
  const size_t directiveLength = strlen(directive);
  return loweredLine.length() == directiveLength ||
         isspace(static_cast<unsigned char>(loweredLine[directiveLength]));
}

String directiveValue(const String &line, const char *directive) {
  String value = line.substring(strlen(directive));
  value.trim();
  return value;
}

bool isSupportedBookName(const String &loweredName) {
  return loweredName.endsWith(".rsvp") || loweredName.endsWith(".txt") ||
         loweredName.endsWith(".epub");
}

String displayNameForPath(const String &path) {
  const int separator = path.lastIndexOf('/');
  if (separator < 0) {
    return path;
  }
  return path.substring(separator + 1);
}

String relativeLibraryName(const String &path) {
  const String prefix = String(kBooksPath) + "/";
  if (path.startsWith(prefix)) {
    return path.substring(prefix.length());
  }
  return displayNameForPath(path);
}

String libraryCategoryForPath(const String &path) {
  const String relative = relativeLibraryName(path);
  if (relative.startsWith("articles/")) {
    return "article";
  }
  if (relative.startsWith("books/")) {
    return "book";
  }
  return "legacy";
}

uint16_t clampU16(uint16_t value, uint16_t minValue, uint16_t maxValue) {
  if (value < minValue) {
    return minValue;
  }
  if (value > maxValue) {
    return maxValue;
  }
  return value;
}

int clampInt(int value, int minValue, int maxValue) {
  if (value < minValue) {
    return minValue;
  }
  if (value > maxValue) {
    return maxValue;
  }
  return value;
}

String enumLabel(uint8_t value, const char *const *labels, size_t count, uint8_t fallback = 0) {
  if (value >= count) {
    value = fallback;
  }
  return labels[value];
}

int enumValue(const String &value, const char *const *labels, size_t count) {
  for (size_t i = 0; i < count; ++i) {
    if (value == labels[i]) {
      return static_cast<int>(i);
    }
  }
  return -1;
}

bool findJsonKey(const String &body, const char *key, int &colonIndex) {
  const String needle = String("\"") + key + "\"";
  const int keyIndex = body.indexOf(needle);
  if (keyIndex < 0) {
    return false;
  }
  colonIndex = body.indexOf(':', keyIndex + needle.length());
  return colonIndex >= 0;
}

int skipJsonWhitespace(const String &body, int index) {
  while (index < static_cast<int>(body.length()) &&
         isspace(static_cast<unsigned char>(body[index]))) {
    ++index;
  }
  return index;
}

bool readJsonInt(const String &body, const char *key, int &value) {
  int colonIndex = -1;
  if (!findJsonKey(body, key, colonIndex)) {
    return false;
  }
  int index = skipJsonWhitespace(body, colonIndex + 1);
  bool negative = false;
  if (index < static_cast<int>(body.length()) && body[index] == '-') {
    negative = true;
    ++index;
  }
  if (index >= static_cast<int>(body.length()) || !isdigit(static_cast<unsigned char>(body[index]))) {
    return false;
  }
  int result = 0;
  while (index < static_cast<int>(body.length()) &&
         isdigit(static_cast<unsigned char>(body[index]))) {
    result = result * 10 + (body[index] - '0');
    ++index;
  }
  value = negative ? -result : result;
  return true;
}

bool readJsonBool(const String &body, const char *key, bool &value) {
  int colonIndex = -1;
  if (!findJsonKey(body, key, colonIndex)) {
    return false;
  }
  const int index = skipJsonWhitespace(body, colonIndex + 1);
  if (body.substring(index, index + 4) == "true") {
    value = true;
    return true;
  }
  if (body.substring(index, index + 5) == "false") {
    value = false;
    return true;
  }
  return false;
}

bool readJsonString(const String &body, const char *key, String &value) {
  int colonIndex = -1;
  if (!findJsonKey(body, key, colonIndex)) {
    return false;
  }
  int index = skipJsonWhitespace(body, colonIndex + 1);
  if (index >= static_cast<int>(body.length()) || body[index] != '"') {
    return false;
  }
  ++index;
  String result;
  while (index < static_cast<int>(body.length())) {
    const char c = body[index++];
    if (c == '"') {
      value = result;
      return true;
    }
    if (c == '\\' && index < static_cast<int>(body.length())) {
      const char escaped = body[index++];
      switch (escaped) {
        case '"':
        case '\\':
        case '/':
          result += escaped;
          break;
        case 'n':
          result += '\n';
          break;
        case 'r':
          result += '\r';
          break;
        case 't':
          result += '\t';
          break;
        default:
          result += escaped;
          break;
      }
    } else {
      result += c;
    }
  }
  return false;
}

bool isHttpUrl(String value) {
  value.trim();
  value.toLowerCase();
  return value.startsWith("http://") || value.startsWith("https://");
}

bool nextJsonArrayString(const String &body, int &index, String &value) {
  index = skipJsonWhitespace(body, index);
  if (index >= static_cast<int>(body.length())) {
    return false;
  }
  if (body[index] == ',') {
    index = skipJsonWhitespace(body, index + 1);
  }
  if (index >= static_cast<int>(body.length()) || body[index] == ']') {
    return false;
  }
  if (body[index] != '"') {
    return false;
  }
  ++index;
  String result;
  while (index < static_cast<int>(body.length())) {
    const char c = body[index++];
    if (c == '"') {
      value = result;
      return true;
    }
    if (c == '\\' && index < static_cast<int>(body.length())) {
      const char escaped = body[index++];
      switch (escaped) {
        case '"':
        case '\\':
        case '/':
          result += escaped;
          break;
        case 'n':
          result += '\n';
          break;
        case 'r':
          result += '\r';
          break;
        case 't':
          result += '\t';
          break;
        default:
          result += escaped;
          break;
      }
    } else {
      result += c;
    }
  }
  return false;
}

String rsvpMetadataValueFromLine(const String &line, const char *directive, bool &pastDirectives) {
  String trimmed = stripBom(line);
  trimmed.trim();
  if (trimmed.isEmpty()) {
    return "";
  }

  String lowered = trimmed;
  lowered.toLowerCase();
  if (directiveMatches(lowered, directive)) {
    return directiveValue(trimmed, directive);
  }

  if (!trimmed.startsWith("@")) {
    pastDirectives = true;
  }
  return "";
}

}  // namespace

CompanionSyncManager *CompanionSyncManager::instance_ = nullptr;

bool CompanionSyncManager::begin(const Config &config) {
  (void)config;
  if (active_) {
    return true;
  }

  instance_ = this;
  pairingCode_ = String(static_cast<uint32_t>(esp_random()) % 900000UL + 100000UL);
  statusLine1_ = "Starting sync";
  statusLine2_ = "Preparing Wi-Fi";
  preferences_.begin(kPrefsNamespace, false);

  const bool networkReady = startAccessPoint();
  if (!networkReady) {
    statusLine1_ = "Wi-Fi failed";
    statusLine2_ = "";
    end();
    return false;
  }

  if (!startServer()) {
    statusLine1_ = "HTTP failed";
    statusLine2_ = "";
    end();
    return false;
  }

  active_ = true;
  statusLine1_ = "Sync ready";
  statusLine2_ = networkSsid_;
  Serial.printf("[sync] ready ssid=%s url=%s pairing=%s\n", networkSsid_.c_str(), baseUrl().c_str(),
                pairingCode_.c_str());
  return true;
}

void CompanionSyncManager::update() {
  if (!active_ || !serverStarted_) {
    return;
  }
  server_.handleClient();
}

void CompanionSyncManager::end() {
  stopServer();

  if (networkMode_ == NetworkMode::Station) {
    WiFi.disconnect(true, false);
  } else if (networkMode_ == NetworkMode::AccessPoint) {
    WiFi.softAPdisconnect(true);
  }
  WiFi.mode(WIFI_OFF);
  preferences_.end();

  networkMode_ = NetworkMode::None;
  active_ = false;
  statusLine1_ = "Idle";
  statusLine2_ = "";
  instance_ = nullptr;
}

bool CompanionSyncManager::active() const { return active_; }

String CompanionSyncManager::statusLine1() const { return statusLine1_; }

String CompanionSyncManager::statusLine2() const { return statusLine2_; }

String CompanionSyncManager::baseUrl() const {
  if (networkMode_ == NetworkMode::Station) {
    return "http://" + ipToString(WiFi.localIP());
  }
  if (networkMode_ == NetworkMode::AccessPoint) {
    return "http://" + ipToString(WiFi.softAPIP());
  }
  return "";
}

void CompanionSyncManager::handleInfoStatic() {
  if (instance_ != nullptr) {
    instance_->handleInfo();
  }
}

void CompanionSyncManager::handleRootStatic() {
  if (instance_ != nullptr) {
    instance_->handleRoot();
  }
}

void CompanionSyncManager::handleBooksListStatic() {
  if (instance_ != nullptr) {
    instance_->handleBooksList();
  }
}

void CompanionSyncManager::handleSettingsStatic() {
  if (instance_ != nullptr) {
    instance_->handleSettings();
  }
}

void CompanionSyncManager::handleRssFeedsStatic() {
  if (instance_ != nullptr) {
    instance_->handleRssFeeds();
  }
}

void CompanionSyncManager::handleBookDeleteStatic() {
  if (instance_ != nullptr) {
    instance_->handleBookDelete();
  }
}

void CompanionSyncManager::handleBooksStatic() {
  if (instance_ != nullptr) {
    instance_->handleBooks();
  }
}

void CompanionSyncManager::handleBookUploadStatic() {
  if (instance_ != nullptr) {
    instance_->handleBookUpload();
  }
}

void CompanionSyncManager::handleNotFoundStatic() {
  if (instance_ != nullptr) {
    instance_->handleNotFound();
  }
}

bool CompanionSyncManager::startAccessPoint() {
  const String ssid = "RSVP-Nano-" + deviceSuffix();
  statusLine1_ = "Sync Wi-Fi";
  statusLine2_ = ssid;
  networkSsid_ = ssid;
  WiFi.mode(WIFI_AP);
  if (!WiFi.softAP(ssid.c_str())) {
    Serial.println("[sync] softAP failed");
    return false;
  }

  networkMode_ = NetworkMode::AccessPoint;
  Serial.printf("[sync] softAP ssid=%s ip=%s\n", ssid.c_str(), ipToString(WiFi.softAPIP()).c_str());
  return true;
}

bool CompanionSyncManager::startServer() {
  server_.on("/", HTTP_GET, handleRootStatic);
  server_.on("/api/info", HTTP_GET, handleInfoStatic);
  server_.on("/api/books", HTTP_GET, handleBooksListStatic);
  server_.on("/api/books", HTTP_DELETE, handleBookDeleteStatic);
  server_.on("/api/books", HTTP_POST, handleBooksStatic, handleBookUploadStatic);
  server_.on("/api/settings", HTTP_GET, handleSettingsStatic);
  server_.on("/api/settings", HTTP_PATCH, handleSettingsStatic);
  server_.on("/api/settings", HTTP_PUT, handleSettingsStatic);
  server_.on("/api/rss-feeds", HTTP_GET, handleRssFeedsStatic);
  server_.on("/api/rss-feeds", HTTP_PUT, handleRssFeedsStatic);
  server_.onNotFound(handleNotFoundStatic);
  server_.begin();
  serverStarted_ = true;

  if (networkMode_ == NetworkMode::Station && MDNS.begin(kMdnsName)) {
    MDNS.addService("http", "tcp", 80);
  }
  return true;
}

void CompanionSyncManager::stopServer() {
  if (serverStarted_) {
    server_.stop();
    MDNS.end();
  }
  finishUpload(false);
  serverStarted_ = false;
}

void CompanionSyncManager::handleInfo() {
  const String mode = networkMode_ == NetworkMode::Station ? "station" : "access_point";
  const String body = String("{") + "\"name\":\"RSVP Nano\"," +
                      "\"mode\":\"" + mode + "\"," +
                      "\"baseUrl\":\"" + jsonEscape(baseUrl()) + "\"," +
                      "\"networkSsid\":\"" + jsonEscape(networkSsid_) + "\"," +
                      "\"pairingCode\":\"" + pairingCode_ + "\"," +
                      "\"uploadPath\":\"/api/books\"" + "}";
  server_.send(200, "application/json", body);
}

void CompanionSyncManager::handleRoot() {
  String body;
  body.reserve(980);
  body += "<!doctype html><html><head><meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">";
  body += "<title>RSVP Nano Sync</title>";
  body += "<style>body{font-family:-apple-system,BlinkMacSystemFont,sans-serif;margin:32px;line-height:1.45}";
  body += "code{background:#f2f2f2;padding:2px 5px;border-radius:4px}</style></head><body>";
  body += "<h1>RSVP Nano Sync</h1>";
  body += "<p>Status: <strong>" + statusLine1_ + "</strong></p>";
  if (!statusLine2_.isEmpty()) {
    body += "<p><code>" + statusLine2_ + "</code></p>";
  }
  body += "<p>API endpoints:</p><ul>";
  body += "<li><a href=\"/api/info\">/api/info</a></li>";
  body += "<li><a href=\"/api/books\">/api/books</a></li>";
  body += "<li><a href=\"/api/settings\">/api/settings</a></li>";
  body += "<li><a href=\"/api/rss-feeds\">/api/rss-feeds</a></li>";
  body += "</ul>";
  body += "<p>Pairing code: <strong>" + pairingCode_ + "</strong></p>";
  body += "</body></html>";
  server_.send(200, "text/html", body);
}

void CompanionSyncManager::handleBooksList() {
  String body = "{\"books\":[";
  bool first = true;

  const auto appendDirectory = [&](const char *directoryPath) {
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
        const String path = String(directoryPath) + "/" + name;
        String lowered = name;
        lowered.toLowerCase();
        if (isSupportedBookName(lowered)) {
          const RsvpMetadata metadata = readRsvpMetadata(path);
          uint8_t progressPercent = 0;
          const bool hasProgress = progressPercentForPath(path, progressPercent);
          if (!first) {
            body += ",";
          }
          first = false;
          body += "{\"name\":\"" + jsonEscape(relativeLibraryName(path)) + "\",\"category\":\"" +
                  libraryCategoryForPath(path) + "\",\"title\":\"" +
                  jsonEscape(metadata.title) + "\",\"author\":\"" + jsonEscape(metadata.author) +
                  "\",\"bytes\":" +
                  String(static_cast<uint32_t>(entry.size()));
          if (hasProgress) {
            body += ",\"progressPercent\":" + String(progressPercent);
          }
          body += "}";
        }
      }
      entry.close();
      entry = dir.openNextFile();
    }

    dir.close();
  };

  appendDirectory(kBooksPath);
  appendDirectory(kBookFilesPath);
  appendDirectory(kArticleFilesPath);

  body += "]}";
  server_.send(200, "application/json", body);
}

void CompanionSyncManager::handleSettings() {
  if (server_.method() == HTTP_GET) {
    server_.send(200, "application/json", settingsJson());
    return;
  }

  const String body = server_.arg("plain");
  if (body.length() > kMaxSettingsPatchBytes) {
    server_.send(413, "application/json", "{\"ok\":false,\"error\":\"Settings payload too large\"}");
    return;
  }

  String error;
  if (!applySettingsJson(body, error)) {
    server_.send(400, "application/json",
                 String("{\"ok\":false,\"error\":\"") + jsonEscape(error) + "\"}");
    return;
  }

  server_.send(200, "application/json", settingsJson());
}

void CompanionSyncManager::handleRssFeeds() {
  if (server_.method() == HTTP_GET) {
    server_.send(200, "application/json", rssFeedsJson());
    return;
  }

  String error;
  if (!writeRssFeedsJson(server_.arg("plain"), error)) {
    server_.send(400, "application/json",
                 String("{\"ok\":false,\"error\":\"") + jsonEscape(error) + "\"}");
    return;
  }

  statusLine1_ = "RSS feeds saved";
  statusLine2_ = kRssConfigPath;
  server_.send(200, "application/json", rssFeedsJson());
}

void CompanionSyncManager::handleBooks() {
  finishUpload(uploadError_.isEmpty());
  if (!uploadError_.isEmpty()) {
    server_.send(400, "application/json",
                 String("{\"ok\":false,\"error\":\"") + jsonEscape(uploadError_) + "\"}");
    uploadError_ = "";
    return;
  }

  server_.send(201, "application/json",
               String("{\"ok\":true,\"path\":\"") + jsonEscape(uploadFinalPath_) + "\"}");
  uploadFinalPath_ = "";
}

void CompanionSyncManager::handleBookDelete() {
  String requested = server_.arg("name");
  requested.trim();
  if (requested.isEmpty()) {
    server_.send(400, "application/json", "{\"ok\":false,\"error\":\"Missing filename\"}");
    return;
  }

  String filename = requested;
  String path;
  const int separator = requested.indexOf('/');
  if (separator >= 0) {
    const String directory = requested.substring(0, separator);
    filename = sanitizeFilename(requested.substring(separator + 1));
    if (filename.isEmpty() || requested.indexOf("..") >= 0 ||
        (directory != "books" && directory != "articles")) {
      server_.send(400, "application/json", "{\"ok\":false,\"error\":\"Invalid library path\"}");
      return;
    }
    path = String(kBooksPath) + "/" + directory + "/" + filename;
  } else {
    filename = sanitizeFilename(requested);
    path = String(kBooksPath) + "/" + filename;
  }

  String lowered = filename;
  lowered.toLowerCase();
  if (!isSupportedBookName(lowered)) {
    server_.send(400, "application/json", "{\"ok\":false,\"error\":\"Unsupported file type\"}");
    return;
  }

  File file = SD_MMC.open(path);
  if ((!file || file.isDirectory()) && separator < 0) {
    if (file) {
      file.close();
    }
    path = String(kBookFilesPath) + "/" + filename;
    file = SD_MMC.open(path);
  }
  if ((!file || file.isDirectory()) && separator < 0) {
    if (file) {
      file.close();
    }
    path = String(kArticleFilesPath) + "/" + filename;
    file = SD_MMC.open(path);
  }
  if (!file || file.isDirectory()) {
    if (file) {
      file.close();
    }
    server_.send(404, "application/json", "{\"ok\":false,\"error\":\"Book not found\"}");
    return;
  }
  file.close();

  if (!SD_MMC.remove(path)) {
    server_.send(500, "application/json", "{\"ok\":false,\"error\":\"Delete failed\"}");
    return;
  }

  statusLine1_ = "Book deleted";
  statusLine2_ = filename;
  Serial.printf("[sync] deleted %s\n", path.c_str());
  server_.send(200, "application/json",
               String("{\"ok\":true,\"path\":\"") + jsonEscape(path) + "\"}");
}

void CompanionSyncManager::handleBookUpload() {
  HTTPUpload &upload = server_.upload();

  if (upload.status == UPLOAD_FILE_START) {
    String filename = sanitizeFilename(server_.arg("name"));
    if (filename.isEmpty()) {
      filename = sanitizeFilename(upload.filename);
    }
    if (filename.isEmpty()) {
      uploadError_ = "Missing filename";
      return;
    }

    String lowered = filename;
    lowered.toLowerCase();
    if (!isSupportedBookName(lowered)) {
      filename += ".rsvp";
    }

    String category = server_.arg("category");
    category.toLowerCase();
    const char *targetDirectory = category == "article" ? kArticleFilesPath : kBookFilesPath;

    SD_MMC.mkdir(kBooksPath);
    SD_MMC.mkdir(targetDirectory);
    uploadFinalPath_ = String(targetDirectory) + "/" + filename;
    uploadTmpPath_ = uploadFinalPath_ + ".tmp";
    SD_MMC.remove(uploadTmpPath_);
    uploadFile_ = SD_MMC.open(uploadTmpPath_, FILE_WRITE);
    if (!uploadFile_) {
      uploadError_ = "Could not create file";
      return;
    }
    uploadError_ = "";
    statusLine1_ = "Receiving book";
    statusLine2_ = filename;
    Serial.printf("[sync] upload start %s\n", uploadFinalPath_.c_str());
    return;
  }

  if (upload.status == UPLOAD_FILE_WRITE) {
    if (!uploadError_.isEmpty() || !uploadFile_) {
      return;
    }
    const size_t written = uploadFile_.write(upload.buf, upload.currentSize);
    if (written != upload.currentSize) {
      uploadError_ = "Write failed";
    }
    return;
  }

  if (upload.status == UPLOAD_FILE_END) {
    Serial.printf("[sync] upload end bytes=%u error=%s\n", upload.totalSize,
                  uploadError_.c_str());
    return;
  }

  if (upload.status == UPLOAD_FILE_ABORTED) {
    uploadError_ = "Upload aborted";
    finishUpload(false);
  }
}

void CompanionSyncManager::handleNotFound() {
  server_.send(404, "application/json", "{\"ok\":false,\"error\":\"Not found\"}");
}

String CompanionSyncManager::settingsJson() {
  static const char *const readerModeLabels[] = {"rsvp", "scroll"};
  static const char *const handednessLabels[] = {"right", "left"};
  static const char *const footerMetricLabels[] = {"percentage", "chapter_time", "book_time"};
  static const char *const batteryLabelLabels[] = {"percent", "time_remaining"};
  static const char *const typefaceLabels[] = {"standard", "open_dyslexic", "atkinson"};
  static const char *const pauseModeLabels[] = {"sentence_end", "instant"};

  const uint16_t wpm =
      clampU16(preferences_.getUShort(kPrefWpm, kDefaultWpm), kMinWpm, kMaxWpm);
  const uint8_t readerMode =
      static_cast<uint8_t>(clampInt(preferences_.getUChar(kPrefReaderMode, 0), 0, kMaxReaderMode));
  const uint8_t pauseMode =
      static_cast<uint8_t>(clampInt(preferences_.getUChar(kPrefPauseMode, 0), 0, kMaxPauseMode));
  const uint16_t longDelay =
      clampU16(preferences_.getUShort(kPrefPacingLongMs, kDefaultPacingDelayMs), 0,
               kMaxPacingDelayMs);
  const uint16_t complexDelay =
      clampU16(preferences_.getUShort(kPrefPacingComplexMs, kDefaultPacingDelayMs), 0,
               kMaxPacingDelayMs);
  const uint16_t punctuationDelay =
      clampU16(preferences_.getUShort(kPrefPacingPunctuationMs, kDefaultPacingDelayMs), 0,
               kMaxPacingDelayMs);
  const uint8_t brightness = static_cast<uint8_t>(
      clampInt(preferences_.getUChar(kPrefBrightness, kDefaultBrightness), 0, kMaxBrightness));
  const uint8_t handedness =
      static_cast<uint8_t>(clampInt(preferences_.getUChar(kPrefHandedness, 0), 0, kMaxHandedness));
  const uint8_t footerMetric = static_cast<uint8_t>(
      clampInt(preferences_.getUChar(kPrefFooterMetricMode, 0), 0, kMaxFooterMetric));
  const uint8_t batteryLabel = static_cast<uint8_t>(
      clampInt(preferences_.getUChar(kPrefBatteryLabelMode, 0), 0, kMaxBatteryLabel));
  const uint8_t language =
      static_cast<uint8_t>(clampInt(preferences_.getUChar(kPrefUiLanguage, 0), 0, kMaxUiLanguage));
  const uint8_t fontSize = static_cast<uint8_t>(
      clampInt(preferences_.getUChar(kPrefReaderFontSize, 0), 0, kMaxReaderFontSize));
  const uint8_t typeface =
      static_cast<uint8_t>(clampInt(preferences_.getUChar(kPrefReaderTypeface, 0), 0,
                                    kMaxReaderTypeface));
  const int tracking =
      clampInt(preferences_.getChar(kPrefTypographyTracking, 0), kMinTypographyTracking,
               kMaxTypographyTracking);
  const uint8_t anchor = static_cast<uint8_t>(
      clampInt(preferences_.getUChar(kPrefTypographyAnchor, kDefaultTypographyAnchor),
               kMinTypographyAnchor, kMaxTypographyAnchor));
  const uint8_t guideWidth = static_cast<uint8_t>(
      clampInt(preferences_.getUChar(kPrefTypographyGuideWidth, kDefaultTypographyGuideWidth),
               kMinTypographyGuideWidth, kMaxTypographyGuideWidth));
  const uint8_t guideGap = static_cast<uint8_t>(
      clampInt(preferences_.getUChar(kPrefTypographyGuideGap, kDefaultTypographyGuideGap),
               kMinTypographyGuideGap, kMaxTypographyGuideGap));

  String body;
  body.reserve(1100);
  body += "{\"ok\":true,\"version\":1";
  body += ",\"reading\":{";
  body += "\"wpm\":" + String(wpm);
  body += ",\"readerMode\":\"";
  body += enumLabel(readerMode, readerModeLabels, 2);
  body += "\"";
  body += ",\"pauseMode\":\"";
  body += enumLabel(pauseMode, pauseModeLabels, 2);
  body += "\"";
  body += ",\"accurateTimeEstimate\":true";
  body += ",\"pacing\":{\"longWordMs\":" + String(longDelay) +
          ",\"complexWordMs\":" + String(complexDelay) +
          ",\"punctuationMs\":" + String(punctuationDelay) + "}";
  body += "}";
  body += ",\"display\":{";
  body += "\"brightnessIndex\":" + String(brightness);
  body += ",\"darkMode\":" + String(preferences_.getBool(kPrefDarkMode, false) ? "true" : "false");
  body += ",\"nightMode\":" +
          String(preferences_.getBool(kPrefNightMode, false) ? "true" : "false");
  body += ",\"handedness\":\"";
  body += enumLabel(handedness, handednessLabels, 2);
  body += "\"";
  body += ",\"footerMetric\":\"";
  body += enumLabel(footerMetric, footerMetricLabels, 3);
  body += "\"";
  body += ",\"batteryLabel\":\"";
  body += enumLabel(batteryLabel, batteryLabelLabels, 2);
  body += "\"";
  body += ",\"language\":" + String(language);
  body += ",\"phantomWords\":" +
          String(preferences_.getBool(kPrefPhantomWords, true) ? "true" : "false");
  body += ",\"fontSizeIndex\":" + String(fontSize);
  body += "}";
  body += ",\"typography\":{";
  body += "\"typeface\":\"";
  body += enumLabel(typeface, typefaceLabels, 3);
  body += "\"";
  body += ",\"focusHighlight\":" +
          String(preferences_.getBool(kPrefTypographyFocusHighlight, true) ? "true" : "false");
  body += ",\"tracking\":" + String(tracking);
  body += ",\"anchorPercent\":" + String(anchor);
  body += ",\"guideWidth\":" + String(guideWidth);
  body += ",\"guideGap\":" + String(guideGap);
  body += "}";
  body += ",\"limits\":{";
  body += "\"wpm\":{\"min\":" + String(kMinWpm) + ",\"max\":" + String(kMaxWpm) + "}";
  body += ",\"brightnessIndex\":{\"min\":0,\"max\":" + String(kMaxBrightness) + "}";
  body += ",\"pacingMs\":{\"min\":0,\"max\":" + String(kMaxPacingDelayMs) + "}";
  body += ",\"tracking\":{\"min\":" + String(kMinTypographyTracking) +
          ",\"max\":" + String(kMaxTypographyTracking) + "}";
  body += ",\"anchorPercent\":{\"min\":" + String(kMinTypographyAnchor) +
          ",\"max\":" + String(kMaxTypographyAnchor) + "}";
  body += ",\"guideWidth\":{\"min\":" + String(kMinTypographyGuideWidth) +
          ",\"max\":" + String(kMaxTypographyGuideWidth) + "}";
  body += ",\"guideGap\":{\"min\":" + String(kMinTypographyGuideGap) +
          ",\"max\":" + String(kMaxTypographyGuideGap) + "}";
  body += "}}";
  return body;
}

bool CompanionSyncManager::applySettingsJson(const String &body, String &error) {
  if (body.isEmpty()) {
    error = "Missing settings JSON";
    return false;
  }

  static const char *const readerModeLabels[] = {"rsvp", "scroll"};
  static const char *const handednessLabels[] = {"right", "left"};
  static const char *const footerMetricLabels[] = {"percentage", "chapter_time", "book_time"};
  static const char *const batteryLabelLabels[] = {"percent", "time_remaining"};
  static const char *const typefaceLabels[] = {"standard", "open_dyslexic", "atkinson"};
  static const char *const pauseModeLabels[] = {"sentence_end", "instant"};

  int intValue = 0;
  bool boolValue = false;
  String stringValue;

  if (readJsonInt(body, "wpm", intValue)) {
    if (intValue < kMinWpm || intValue > kMaxWpm) {
      error = "wpm must be between 100 and 1000";
      return false;
    }
    preferences_.putUShort(kPrefWpm, static_cast<uint16_t>(intValue));
  }
  if (readJsonString(body, "readerMode", stringValue)) {
    const int value = enumValue(stringValue, readerModeLabels, 2);
    if (value < 0) {
      error = "readerMode must be rsvp or scroll";
      return false;
    }
    preferences_.putUChar(kPrefReaderMode, static_cast<uint8_t>(value));
  }
  if (readJsonString(body, "pauseMode", stringValue)) {
    const int value = enumValue(stringValue, pauseModeLabels, 2);
    if (value < 0) {
      error = "pauseMode must be sentence_end or instant";
      return false;
    }
    preferences_.putUChar(kPrefPauseMode, static_cast<uint8_t>(value));
  }
  preferences_.putBool(kPrefAccurateTime, true);
  if (readJsonInt(body, "longWordMs", intValue)) {
    if (intValue < 0 || intValue > kMaxPacingDelayMs) {
      error = "longWordMs must be between 0 and 600";
      return false;
    }
    preferences_.putUShort(kPrefPacingLongMs, static_cast<uint16_t>(intValue));
  }
  if (readJsonInt(body, "complexWordMs", intValue)) {
    if (intValue < 0 || intValue > kMaxPacingDelayMs) {
      error = "complexWordMs must be between 0 and 600";
      return false;
    }
    preferences_.putUShort(kPrefPacingComplexMs, static_cast<uint16_t>(intValue));
  }
  if (readJsonInt(body, "punctuationMs", intValue)) {
    if (intValue < 0 || intValue > kMaxPacingDelayMs) {
      error = "punctuationMs must be between 0 and 600";
      return false;
    }
    preferences_.putUShort(kPrefPacingPunctuationMs, static_cast<uint16_t>(intValue));
  }
  if (readJsonInt(body, "brightnessIndex", intValue)) {
    if (intValue < 0 || intValue > kMaxBrightness) {
      error = "brightnessIndex must be between 0 and 4";
      return false;
    }
    preferences_.putUChar(kPrefBrightness, static_cast<uint8_t>(intValue));
  }
  if (readJsonBool(body, "darkMode", boolValue)) {
    preferences_.putBool(kPrefDarkMode, boolValue);
  }
  if (readJsonBool(body, "nightMode", boolValue)) {
    preferences_.putBool(kPrefNightMode, boolValue);
  }
  if (readJsonString(body, "handedness", stringValue)) {
    const int value = enumValue(stringValue, handednessLabels, 2);
    if (value < 0) {
      error = "handedness must be right or left";
      return false;
    }
    preferences_.putUChar(kPrefHandedness, static_cast<uint8_t>(value));
  }
  if (readJsonString(body, "footerMetric", stringValue)) {
    const int value = enumValue(stringValue, footerMetricLabels, 3);
    if (value < 0) {
      error = "footerMetric must be percentage, chapter_time, or book_time";
      return false;
    }
    preferences_.putUChar(kPrefFooterMetricMode, static_cast<uint8_t>(value));
  }
  if (readJsonString(body, "batteryLabel", stringValue)) {
    const int value = enumValue(stringValue, batteryLabelLabels, 2);
    if (value < 0) {
      error = "batteryLabel must be percent or time_remaining";
      return false;
    }
    preferences_.putUChar(kPrefBatteryLabelMode, static_cast<uint8_t>(value));
  }
  if (readJsonInt(body, "language", intValue)) {
    if (intValue < 0 || intValue > kMaxUiLanguage) {
      error = "language is out of range";
      return false;
    }
    preferences_.putUChar(kPrefUiLanguage, static_cast<uint8_t>(intValue));
  }
  if (readJsonBool(body, "phantomWords", boolValue)) {
    preferences_.putBool(kPrefPhantomWords, boolValue);
  }
  if (readJsonInt(body, "fontSizeIndex", intValue)) {
    if (intValue < 0 || intValue > kMaxReaderFontSize) {
      error = "fontSizeIndex must be between 0 and 2";
      return false;
    }
    preferences_.putUChar(kPrefReaderFontSize, static_cast<uint8_t>(intValue));
  }
  if (readJsonString(body, "typeface", stringValue)) {
    const int value = enumValue(stringValue, typefaceLabels, 3);
    if (value < 0) {
      error = "typeface must be standard, open_dyslexic, or atkinson";
      return false;
    }
    preferences_.putUChar(kPrefReaderTypeface, static_cast<uint8_t>(value));
  }
  if (readJsonBool(body, "focusHighlight", boolValue)) {
    preferences_.putBool(kPrefTypographyFocusHighlight, boolValue);
  }
  if (readJsonInt(body, "tracking", intValue)) {
    if (intValue < kMinTypographyTracking || intValue > kMaxTypographyTracking) {
      error = "tracking is out of range";
      return false;
    }
    preferences_.putChar(kPrefTypographyTracking, static_cast<int8_t>(intValue));
  }
  if (readJsonInt(body, "anchorPercent", intValue)) {
    if (intValue < kMinTypographyAnchor || intValue > kMaxTypographyAnchor) {
      error = "anchorPercent is out of range";
      return false;
    }
    preferences_.putUChar(kPrefTypographyAnchor, static_cast<uint8_t>(intValue));
  }
  if (readJsonInt(body, "guideWidth", intValue)) {
    if (intValue < kMinTypographyGuideWidth || intValue > kMaxTypographyGuideWidth) {
      error = "guideWidth is out of range";
      return false;
    }
    preferences_.putUChar(kPrefTypographyGuideWidth, static_cast<uint8_t>(intValue));
  }
  if (readJsonInt(body, "guideGap", intValue)) {
    if (intValue < kMinTypographyGuideGap || intValue > kMaxTypographyGuideGap) {
      error = "guideGap is out of range";
      return false;
    }
    preferences_.putUChar(kPrefTypographyGuideGap, static_cast<uint8_t>(intValue));
  }

  return true;
}

String CompanionSyncManager::rssFeedsJson() {
  String body = "{\"ok\":true,\"feeds\":[";
  File file = SD_MMC.open(kRssConfigPath);
  bool first = true;
  if (file && !file.isDirectory()) {
    while (file.available()) {
      String line = file.readStringUntil('\n');
      line.trim();
      if (line.isEmpty() || line.startsWith("#")) {
        continue;
      }
      if (line.startsWith("feed=")) {
        line = line.substring(5);
        line.trim();
      }
      if (!isHttpUrl(line)) {
        continue;
      }
      if (!first) {
        body += ",";
      }
      first = false;
      body += "\"" + jsonEscape(line) + "\"";
    }
  }
  if (file) {
    file.close();
  }
  body += "]}";
  return body;
}

bool CompanionSyncManager::writeRssFeedsJson(const String &body, String &error) {
  if (body.length() > kMaxRssFeedsPatchBytes) {
    error = "RSS feed payload too large";
    return false;
  }

  int colonIndex = -1;
  if (!findJsonKey(body, "feeds", colonIndex)) {
    error = "Missing feeds array";
    return false;
  }
  int index = skipJsonWhitespace(body, colonIndex + 1);
  if (index >= static_cast<int>(body.length()) || body[index] != '[') {
    error = "feeds must be an array";
    return false;
  }
  ++index;

  std::vector<String> feeds;
  feeds.reserve(8);
  while (true) {
    index = skipJsonWhitespace(body, index);
    if (index < static_cast<int>(body.length()) && body[index] == ']') {
      break;
    }

    String feed;
    if (!nextJsonArrayString(body, index, feed)) {
      error = "Invalid feeds array";
      return false;
    }
    feed.trim();
    if (feed.isEmpty()) {
      continue;
    }
    if (!isHttpUrl(feed)) {
      error = "Feeds must start with http:// or https://";
      return false;
    }
    if (feeds.size() >= kMaxRssFeeds) {
      error = "Too many RSS feeds";
      return false;
    }
    bool duplicate = false;
    for (const String &existing : feeds) {
      if (existing == feed) {
        duplicate = true;
        break;
      }
    }
    if (!duplicate) {
      feeds.push_back(feed);
    }
  }

  SD_MMC.mkdir(kConfigPath);
  const String tmpPath = String(kRssConfigPath) + ".tmp";
  SD_MMC.remove(tmpPath);
  File file = SD_MMC.open(tmpPath, FILE_WRITE);
  if (!file) {
    error = "Could not write RSS config";
    return false;
  }
  file.println("# RSVP Nano RSS feeds");
  for (const String &feed : feeds) {
    file.print("feed=");
    file.println(feed);
  }
  file.close();

  SD_MMC.remove(kRssConfigPath);
  if (!SD_MMC.rename(tmpPath, kRssConfigPath)) {
    SD_MMC.remove(tmpPath);
    error = "Could not save RSS config";
    return false;
  }
  return true;
}

String CompanionSyncManager::deviceSuffix() const {
  uint64_t mac = ESP.getEfuseMac();
  char suffix[7];
  snprintf(suffix, sizeof(suffix), "%06X", static_cast<unsigned int>(mac & 0xFFFFFF));
  return String(suffix);
}

String CompanionSyncManager::jsonEscape(const String &value) const {
  String escaped;
  escaped.reserve(value.length() + 8);
  for (size_t i = 0; i < value.length(); ++i) {
    const uint8_t c = static_cast<uint8_t>(value[i]);
    switch (c) {
      case '"':
        escaped += "\\\"";
        break;
      case '\\':
        escaped += "\\\\";
        break;
      case '\b':
        escaped += "\\b";
        break;
      case '\f':
        escaped += "\\f";
        break;
      case '\n':
        escaped += "\\n";
        break;
      case '\r':
        escaped += "\\r";
        break;
      case '\t':
        escaped += "\\t";
        break;
      default:
        if (c < 0x20) {
          char code[7];
          std::snprintf(code, sizeof(code), "\\u%04x", c);
          escaped += code;
        } else {
          escaped += static_cast<char>(c);
        }
        break;
    }
  }
  return escaped;
}

String CompanionSyncManager::sanitizeFilename(const String &name) const {
  String sanitized;
  sanitized.reserve(name.length());
  for (size_t i = 0; i < name.length(); ++i) {
    const char c = name[i];
    sanitized += isSafeFilenameChar(c) ? c : '-';
  }
  sanitized.trim();
  while (sanitized.startsWith(".")) {
    sanitized.remove(0, 1);
  }
  return sanitized;
}

CompanionSyncManager::RsvpMetadata CompanionSyncManager::readRsvpMetadata(
    const String &path) const {
  RsvpMetadata metadata;
  String loweredPath = path;
  loweredPath.toLowerCase();
  if (!loweredPath.endsWith(".rsvp")) {
    return metadata;
  }

  File file = SD_MMC.open(path);
  if (!file || file.isDirectory()) {
    if (file) {
      file.close();
    }
    return metadata;
  }

  String line;
  bool pastDirectives = false;
  while (file.available()) {
    const char c = static_cast<char>(file.read());
    if (c == '\r') {
      continue;
    }

    if (c != '\n') {
      line += c;
      if (line.length() > kMaxMetadataLineChars) {
        pastDirectives = true;
        line = "";
        break;
      }
      continue;
    }

    if (metadata.title.isEmpty()) {
      metadata.title = rsvpMetadataValueFromLine(line, "@title", pastDirectives);
    }
    if (metadata.author.isEmpty() && !pastDirectives) {
      metadata.author = rsvpMetadataValueFromLine(line, "@author", pastDirectives);
    }
    if (!metadata.title.isEmpty() && !metadata.author.isEmpty()) {
      break;
    }

    if (pastDirectives) {
      break;
    }
    line = "";
  }

  if (!line.isEmpty() && !pastDirectives) {
    if (metadata.title.isEmpty()) {
      metadata.title = rsvpMetadataValueFromLine(line, "@title", pastDirectives);
    }
    if (metadata.author.isEmpty() && !pastDirectives) {
      metadata.author = rsvpMetadataValueFromLine(line, "@author", pastDirectives);
    }
  }

  file.close();
  return metadata;
}

bool CompanionSyncManager::progressPercentForPath(const String &path, uint8_t &percent) {
  const String positionKey = bookPositionKey(path);
  const String countKey = bookWordCountKey(path);
  if (!preferences_.isKey(positionKey.c_str()) || !preferences_.isKey(countKey.c_str())) {
    return false;
  }

  const size_t wordCount = preferences_.getUInt(countKey.c_str(), 0);
  if (wordCount <= 1) {
    return false;
  }

  size_t wordIndex = preferences_.getUInt(positionKey.c_str(), 0);
  wordIndex = std::min(wordIndex, wordCount - 1);
  const size_t progress = (wordIndex * static_cast<size_t>(100)) / (wordCount - 1);
  percent = static_cast<uint8_t>(std::min(static_cast<size_t>(100), progress));
  return true;
}

String CompanionSyncManager::bookPositionKey(const String &bookPath) const {
  char key[10];
  std::snprintf(key, sizeof(key), "p%08lx", static_cast<unsigned long>(hashBookPath(bookPath)));
  return String(key);
}

String CompanionSyncManager::bookWordCountKey(const String &bookPath) const {
  char key[10];
  std::snprintf(key, sizeof(key), "c%08lx", static_cast<unsigned long>(hashBookPath(bookPath)));
  return String(key);
}

uint32_t CompanionSyncManager::hashBookPath(const String &path) const {
  uint32_t hash = 2166136261UL;
  for (size_t i = 0; i < path.length(); ++i) {
    hash ^= static_cast<uint8_t>(path[i]);
    hash *= 16777619UL;
  }
  return hash;
}

void CompanionSyncManager::finishUpload(bool success) {
  if (uploadFile_) {
    uploadFile_.close();
  }

  if (uploadTmpPath_.isEmpty()) {
    return;
  }

  if (success && uploadError_.isEmpty()) {
    SD_MMC.remove(uploadFinalPath_);
    if (!SD_MMC.rename(uploadTmpPath_, uploadFinalPath_)) {
      uploadError_ = "Rename failed";
      SD_MMC.remove(uploadTmpPath_);
    } else {
      statusLine1_ = "Book received";
      statusLine2_ = uploadFinalPath_;
      Serial.printf("[sync] upload ready %s\n", uploadFinalPath_.c_str());
    }
  } else {
    SD_MMC.remove(uploadTmpPath_);
  }

  uploadTmpPath_ = "";
}
