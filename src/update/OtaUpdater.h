#pragma once

#include <Arduino.h>

class OtaUpdater {
 public:
  using StatusCallback = void (*)(void *context, const char *title, const char *line1,
                                  const char *line2, int progressPercent);

  struct Config {
    String wifiSsid;
    String wifiPassword;
    String githubOwner = "ionutdecebal";
    String githubRepo = "rsvpnano";
    String assetName = "rsvp-nano-ota.bin";
    bool autoCheck = false;
  };

  enum class ResultCode : uint8_t {
    Success,
    NoUpdate,
    UpdateAvailable,
    NotConfigured,
    ConnectFailed,
    MetadataFailed,
    AssetMissing,
    InstallFailed,
  };

  struct Result {
    ResultCode code = ResultCode::MetadataFailed;
    String currentVersion;
    String latestVersion;
    String summary;
    String detail;
    bool rebootRequired = false;
  };

  bool loadConfig(Config &config) const;
  bool isConfigured(const Config &config) const;
  String currentVersion() const;
  Result checkOnly(const Config &config, StatusCallback callback = nullptr,
                   void *context = nullptr) const;
  Result checkAndInstall(const Config &config, StatusCallback callback = nullptr,
                         void *context = nullptr) const;

 private:
  struct LatestRelease {
    String tagName;
    String assetUrl;
  };

  bool loadConfigFromPath(const char *path, Config &config) const;
  bool connectWiFi(const Config &config, StatusCallback callback, void *context) const;
  void disconnectWiFi() const;
  bool fetchLatestRelease(const Config &config, LatestRelease &release, String &errorDetail,
                          StatusCallback callback, void *context) const;
  bool resolveDownloadUrl(const String &assetUrl, const String &version, String &resolvedUrl,
                          String &errorDetail, StatusCallback callback, void *context) const;
  void reportStatus(StatusCallback callback, void *context, const char *title,
                    const String &line1, const String &line2, int progressPercent) const;
};
