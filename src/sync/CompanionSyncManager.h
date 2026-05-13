#pragma once

#include <Arduino.h>
#include <FS.h>
#include <Preferences.h>
#include <WebServer.h>

class CompanionSyncManager {
 public:
  struct Config {
    String wifiSsid;
    String wifiPassword;
  };

  bool begin(const Config &config);
  void update();
  void end();
  bool active() const;
  String statusLine1() const;
  String statusLine2() const;
  String baseUrl() const;

 private:
  enum class NetworkMode : uint8_t {
    None,
    Station,
    AccessPoint,
  };

  struct RsvpMetadata {
    String title;
    String author;
  };

  static void handleInfoStatic();
  static void handleRootStatic();
  static void handleBooksListStatic();
  static void handleSettingsStatic();
  static void handleRssFeedsStatic();
  static void handleBookDeleteStatic();
  static void handleBooksStatic();
  static void handleBookUploadStatic();
  static void handleNotFoundStatic();

  bool startAccessPoint();
  bool startServer();
  void stopServer();
  void handleInfo();
  void handleRoot();
  void handleBooksList();
  void handleSettings();
  void handleRssFeeds();
  void handleBookDelete();
  void handleBooks();
  void handleBookUpload();
  void handleNotFound();
  String settingsJson();
  bool applySettingsJson(const String &body, String &error);
  String rssFeedsJson();
  bool writeRssFeedsJson(const String &body, String &error);
  String deviceSuffix() const;
  String jsonEscape(const String &value) const;
  String sanitizeFilename(const String &name) const;
  RsvpMetadata readRsvpMetadata(const String &path) const;
  bool progressPercentForPath(const String &path, uint8_t &percent);
  String bookPositionKey(const String &bookPath) const;
  String bookWordCountKey(const String &bookPath) const;
  uint32_t hashBookPath(const String &path) const;
  void finishUpload(bool success);

  static CompanionSyncManager *instance_;

  WebServer server_{80};
  File uploadFile_;
  String uploadFinalPath_;
  String uploadTmpPath_;
  String uploadError_;
  String pairingCode_;
  String networkSsid_;
  Preferences preferences_;
  String statusLine1_ = "Idle";
  String statusLine2_;
  NetworkMode networkMode_ = NetworkMode::None;
  bool active_ = false;
  bool serverStarted_ = false;
};
