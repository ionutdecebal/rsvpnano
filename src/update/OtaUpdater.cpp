#include "update/OtaUpdater.h"

#include <algorithm>

#include <HTTPClient.h>
#include <HTTPUpdate.h>
#include <SD_MMC.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

#include "net/WifiConnection.h"
#include "update/ReleaseParser.h"

#ifndef RSVP_FIRMWARE_VERSION
#define RSVP_FIRMWARE_VERSION "dev"
#endif

namespace {

constexpr const char *kConfigPaths[] = {
    "/config/ota.conf",
    "/ota.conf",
};
constexpr size_t kMaxReleaseJsonBytes = 32768;
constexpr const char *kStatusTitle = "OTA";
const char *kRedirectHeaderKeys[] = {
    "Location",
};

String trimCopy(String value) {
  value.trim();
  return value;
}

bool parseBoolValue(const String &value) {
  String lowered = trimCopy(value);
  lowered.toLowerCase();
  return lowered == "1" || lowered == "true" || lowered == "yes" || lowered == "on";
}

String readBodyLimited(HTTPClient &http, size_t maxBytes) {
  WiFiClient *stream = http.getStreamPtr();
  if (stream == nullptr) {
    return "";
  }

  const int reportedSize = http.getSize();
  String body;
  const size_t reserveBytes =
      reportedSize > 0 ? std::min(static_cast<size_t>(reportedSize), maxBytes) : 1024;
  body.reserve(reserveBytes);

  uint8_t buffer[512];
  size_t totalRead = 0;
  while (http.connected() || stream->available()) {
    if (reportedSize > 0 && totalRead >= static_cast<size_t>(reportedSize)) {
      break;
    }

    const int available = stream->available();
    if (available <= 0) {
      delay(1);
      continue;
    }

    const size_t remaining = maxBytes - totalRead;
    if (remaining == 0) {
      break;
    }

    const size_t chunkSize =
        std::min(remaining, std::min(sizeof(buffer), static_cast<size_t>(available)));
    const int bytesRead = stream->readBytes(buffer, chunkSize);
    if (bytesRead <= 0) {
      break;
    }

    totalRead += static_cast<size_t>(bytesRead);
    for (int i = 0; i < bytesRead; ++i) {
      body += static_cast<char>(buffer[i]);
    }
  }

  return body;
}

String userAgentForVersion(const String &version) {
  return String("RSVP-Nano/") + (version.isEmpty() ? "dev" : version);
}

String versionDetail(const String &currentVersion, const String &latestVersion) {
  if (latestVersion.isEmpty()) {
    return currentVersion;
  }
  if (currentVersion.isEmpty()) {
    return latestVersion;
  }
  return currentVersion + " -> " + latestVersion;
}

bool assetNameLooksCompatibleWithBoard(const String &assetName, String &errorDetail) {
  const String trimmed = trimCopy(assetName);
  if (trimmed.isEmpty()) {
    errorDetail = "Asset name missing";
    return false;
  }

  if (trimmed != Board::Config::OTA_ASSET_NAME) {
    errorDetail = "Asset does not match " + String(Board::Config::BOARD_LABEL);
    return false;
  }

  return true;
}

}  // namespace

bool OtaUpdater::loadConfig(Config &config) const {
  config = Config();
  for (const char *path : kConfigPaths) {
    if (loadConfigFromPath(path, config)) {
      return true;
    }
  }

  return false;
}

bool OtaUpdater::isConfigured(const Config &config) const {
  return !trimCopy(config.wifiSsid).isEmpty();
}

String OtaUpdater::currentVersion() const { return RSVP_FIRMWARE_VERSION; }

bool OtaUpdater::loadConfigFromPath(const char *path, Config &config) const {
  File file = SD_MMC.open(path);
  if (!file || file.isDirectory()) {
    if (file) {
      file.close();
    }
    return false;
  }

  while (file.available()) {
    String line = file.readStringUntil('\n');
    line.trim();
    if (line.isEmpty() || line.startsWith("#")) {
      continue;
    }

    const int equalsIndex = line.indexOf('=');
    if (equalsIndex <= 0) {
      continue;
    }

    String key = line.substring(0, equalsIndex);
    String value = line.substring(equalsIndex + 1);
    key.trim();
    value.trim();
    key.toLowerCase();

    if (key == "wifi_ssid") {
      config.wifiSsid = value;
    } else if (key == "wifi_password") {
      config.wifiPassword = value;
    } else if (key == "github_owner") {
      config.githubOwner = value;
    } else if (key == "github_repo") {
      config.githubRepo = value;
    } else if (key == "asset_name") {
      config.assetName = value;
    } else if (key == "auto_check") {
      config.autoCheck = parseBoolValue(value);
    }
  }

  file.close();
  return true;
}

bool OtaUpdater::connectWiFi(const Config &config, StatusCallback callback,
                             void *context) const {
  return net::connectStation(config.wifiSsid, config.wifiPassword, [&](int percent) {
    reportStatus(callback, context, kStatusTitle, "Connecting Wi-Fi", config.wifiSsid, percent);
  });
}

void OtaUpdater::disconnectWiFi() const { net::disconnect(); }

bool OtaUpdater::fetchLatestRelease(const Config &config, LatestRelease &release,
                                    String &errorDetail, StatusCallback callback,
                                    void *context) const {
  const String version = currentVersion();
  const String url = "https://api.github.com/repos/" + config.githubOwner + "/" +
                     config.githubRepo + "/releases/latest";

  reportStatus(callback, context, kStatusTitle, "Checking GitHub", config.githubRepo, 22);

  WiFiClientSecure client;
  // GitHub release metadata and assets can redirect across multiple hosts, so keep the transport
  // flexible for now. A signed manifest is the best follow-up hardening step.
  client.setInsecure();
  client.setHandshakeTimeout(15);

  HTTPClient http;
  http.setUserAgent(userAgentForVersion(version));
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.setTimeout(15000);
  if (!http.begin(client, url)) {
    errorDetail = "HTTP begin failed";
    return false;
  }

  http.addHeader("Accept", "application/vnd.github+json");
  const int statusCode = http.GET();
  if (statusCode != HTTP_CODE_OK) {
    if (statusCode == HTTP_CODE_NOT_FOUND) {
      errorDetail = "No published release";
    } else {
      errorDetail = "GitHub HTTP " + String(statusCode);
    }
    http.end();
    return false;
  }

  const String body = readBodyLimited(http, kMaxReleaseJsonBytes);
  http.end();

  releaseparser::ReleaseInfo parsed;
  if (!releaseparser::parse(body, config.assetName, parsed)) {
    errorDetail = "Release tag missing";
    return false;
  }
  release.tagName = parsed.tagName;
  release.assetUrl = parsed.assetUrl;

  if (release.assetUrl.isEmpty()) {
    errorDetail = config.assetName + " missing";
    return false;
  }

  return true;
}

bool OtaUpdater::resolveDownloadUrl(const String &assetUrl, const String &version,
                                    String &resolvedUrl, String &errorDetail,
                                    StatusCallback callback, void *context) const {
  reportStatus(callback, context, kStatusTitle, "Resolving asset", version, 29);

  WiFiClientSecure client;
  client.setInsecure();
  client.setHandshakeTimeout(15);

  HTTPClient http;
  http.collectHeaders(kRedirectHeaderKeys, 1);
  http.setUserAgent(userAgentForVersion(version));
  http.setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS);
  http.setTimeout(15000);
  if (!http.begin(client, assetUrl)) {
    errorDetail = "Asset URL failed";
    return false;
  }

  http.addHeader("Accept", "application/octet-stream");
  const int statusCode = http.GET();
  if (statusCode == HTTP_CODE_OK) {
    resolvedUrl = assetUrl;
    http.end();
    return true;
  }

  if (statusCode == HTTP_CODE_MOVED_PERMANENTLY || statusCode == HTTP_CODE_FOUND ||
      statusCode == HTTP_CODE_SEE_OTHER || statusCode == HTTP_CODE_TEMPORARY_REDIRECT ||
      statusCode == HTTP_CODE_PERMANENT_REDIRECT) {
    resolvedUrl = http.header("Location");
    http.end();
    if (!resolvedUrl.isEmpty()) {
      return true;
    }
    errorDetail = "Asset redirect missing";
    return false;
  }

  errorDetail = "Asset HTTP " + String(statusCode);
  http.end();
  return false;
}

void OtaUpdater::reportStatus(StatusCallback callback, void *context, const char *title,
                              const String &line1, const String &line2,
                              int progressPercent) const {
  if (callback == nullptr) {
    return;
  }

  callback(context, title, line1.c_str(), line2.c_str(), progressPercent);
}

OtaUpdater::Result OtaUpdater::checkOnly(const Config &config, StatusCallback callback,
                                         void *context) const {
  Result result;
  result.currentVersion = currentVersion();

  String assetCompatibilityError;
  if (!assetNameLooksCompatibleWithBoard(config.assetName, assetCompatibilityError)) {
    result.code = ResultCode::AssetMismatch;
    result.summary = "Wrong OTA asset";
    result.detail = assetCompatibilityError;
    return result;
  }

  if (!isConfigured(config)) {
    result.code = ResultCode::NotConfigured;
    result.summary = "Wi-Fi not set";
    result.detail = "Settings -> Wi-Fi";
    return result;
  }

  if (!connectWiFi(config, callback, context)) {
    disconnectWiFi();
    result.code = ResultCode::ConnectFailed;
    result.summary = "Wi-Fi failed";
    result.detail = "Check credentials";
    return result;
  }

  LatestRelease release;
  String metadataError;
  if (!fetchLatestRelease(config, release, metadataError, callback, context)) {
    disconnectWiFi();
    result.code = ResultCode::MetadataFailed;
    result.summary = "GitHub failed";
    result.detail = metadataError;
    return result;
  }

  disconnectWiFi();
  result.latestVersion = release.tagName;
  if (release.tagName == result.currentVersion) {
    result.code = ResultCode::NoUpdate;
    result.summary = "Already current";
    result.detail = release.tagName;
    return result;
  }

  if (release.assetUrl.isEmpty()) {
    result.code = ResultCode::AssetMissing;
    result.summary = "Asset missing";
    result.detail = config.assetName;
    return result;
  }

  result.code = ResultCode::UpdateAvailable;
  result.summary = "Update available";
  result.detail = release.tagName;
  return result;
}

OtaUpdater::Result OtaUpdater::checkAndInstall(const Config &config, StatusCallback callback,
                                               void *context) const {
  Result result;
  result.currentVersion = currentVersion();

  String assetCompatibilityError;
  if (!assetNameLooksCompatibleWithBoard(config.assetName, assetCompatibilityError)) {
    result.code = ResultCode::AssetMismatch;
    result.summary = "Wrong OTA asset";
    result.detail = assetCompatibilityError;
    return result;
  }

  if (!isConfigured(config)) {
    result.code = ResultCode::NotConfigured;
    result.summary = "Wi-Fi not set";
    result.detail = "Settings -> Wi-Fi";
    return result;
  }

  if (!connectWiFi(config, callback, context)) {
    disconnectWiFi();
    result.code = ResultCode::ConnectFailed;
    result.summary = "Wi-Fi failed";
    result.detail = "Check credentials";
    return result;
  }

  LatestRelease release;
  String metadataError;
  if (!fetchLatestRelease(config, release, metadataError, callback, context)) {
    disconnectWiFi();
    result.code = ResultCode::MetadataFailed;
    result.summary = "GitHub failed";
    result.detail = metadataError;
    return result;
  }

  result.latestVersion = release.tagName;
  if (release.tagName == result.currentVersion) {
    disconnectWiFi();
    result.code = ResultCode::NoUpdate;
    result.summary = "Already current";
    result.detail = release.tagName;
    return result;
  }

  if (release.assetUrl.isEmpty()) {
    disconnectWiFi();
    result.code = ResultCode::AssetMissing;
    result.summary = "Asset missing";
    result.detail = config.assetName;
    return result;
  }

  reportStatus(callback, context, kStatusTitle, "Preparing update",
               versionDetail(result.currentVersion, result.latestVersion), 28);

  String resolvedAssetUrl;
  String resolveError;
  if (!resolveDownloadUrl(release.assetUrl, result.latestVersion, resolvedAssetUrl, resolveError,
                          callback, context)) {
    disconnectWiFi();
    result.code = ResultCode::InstallFailed;
    result.summary = "Asset failed";
    result.detail = resolveError;
    return result;
  }

  WiFiClientSecure client;
  // Match the metadata request behavior until the update path gains certificate pinning or
  // signature verification above the transport layer.
  client.setInsecure();
  client.setHandshakeTimeout(15);

  HTTPUpdate updater;
  updater.rebootOnUpdate(false);
  updater.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);

  int lastReportedProgress = -1;
  updater.onProgress([this, callback, context, &result, &lastReportedProgress](int current,
                                                                                int total) {
    if (total <= 0) {
      reportStatus(callback, context, kStatusTitle, "Downloading update", result.latestVersion,
                   -1);
      return;
    }

    const int progress = 30 + static_cast<int>((static_cast<int64_t>(current) * 65) / total);
    if (progress == lastReportedProgress) {
      return;
    }

    lastReportedProgress = progress;
    reportStatus(callback, context, kStatusTitle, "Downloading update", result.latestVersion,
                 progress);
  });

  const String version = result.currentVersion;
  const t_httpUpdate_return updateResult =
      updater.update(client, resolvedAssetUrl, version, [version](HTTPClient *http) {
        http->setUserAgent(userAgentForVersion(version));
        http->addHeader("Accept", "application/octet-stream");
      });

  disconnectWiFi();

  switch (updateResult) {
    case HTTP_UPDATE_OK:
      result.code = ResultCode::Success;
      result.summary = "Update ready";
      result.detail = result.latestVersion;
      result.rebootRequired = true;
      return result;
    case HTTP_UPDATE_NO_UPDATES:
      result.code = ResultCode::NoUpdate;
      result.summary = "Already current";
      result.detail = result.latestVersion;
      return result;
    case HTTP_UPDATE_FAILED:
    default:
      result.code = ResultCode::InstallFailed;
      result.summary = "Update failed";
      result.detail = updater.getLastErrorString();
      return result;
  }
}
