#include "sync/CompanionSyncManager.h"

#include <ESPmDNS.h>
#include <SD_MMC.h>
#include <WiFi.h>
#include <algorithm>
#include <cstdio>

namespace {

constexpr const char *kMdnsName = "rsvp-nano";
constexpr const char *kBooksPath = "/books";
constexpr const char *kPrefsNamespace = "rsvp";
constexpr size_t kMaxMetadataLineChars = 160;

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
  preferences_.begin(kPrefsNamespace, true);

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
  body.reserve(900);
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
  body += "</ul>";
  body += "<p>Pairing code: <strong>" + pairingCode_ + "</strong></p>";
  body += "</body></html>";
  server_.send(200, "text/html", body);
}

void CompanionSyncManager::handleBooksList() {
  File dir = SD_MMC.open(kBooksPath);
  if (!dir || !dir.isDirectory()) {
    if (dir) {
      dir.close();
    }
    server_.send(200, "application/json", "{\"books\":[]}");
    return;
  }

  String body = "{\"books\":[";
  bool first = true;
  File entry = dir.openNextFile();
  while (entry) {
    if (!entry.isDirectory()) {
      const String entryName = String(entry.name());
      String name = entryName;
      const int separator = entryName.lastIndexOf('/');
      if (separator >= 0) {
        name = entryName.substring(separator + 1);
      }
      const String path = String(kBooksPath) + "/" + name;
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
        body += "{\"name\":\"" + jsonEscape(name) + "\",\"title\":\"" +
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

  body += "]}";
  server_.send(200, "application/json", body);
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
  const String filename = sanitizeFilename(server_.arg("name"));
  if (filename.isEmpty()) {
    server_.send(400, "application/json", "{\"ok\":false,\"error\":\"Missing filename\"}");
    return;
  }

  String lowered = filename;
  lowered.toLowerCase();
  if (!isSupportedBookName(lowered)) {
    server_.send(400, "application/json", "{\"ok\":false,\"error\":\"Unsupported file type\"}");
    return;
  }

  const String path = String(kBooksPath) + "/" + filename;
  File file = SD_MMC.open(path);
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

    SD_MMC.mkdir(kBooksPath);
    uploadFinalPath_ = String(kBooksPath) + "/" + filename;
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
    const char c = value[i];
    if (c == '"' || c == '\\') {
      escaped += '\\';
    }
    escaped += c;
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
