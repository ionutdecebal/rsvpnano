#pragma once

#include <Arduino.h>
#include <Preferences.h>

#include "rss/FeedParser.h"
#include "update/OtaUpdater.h"

class RssFeedManager {
 public:
  using StatusCallback = OtaUpdater::StatusCallback;

  struct Result {
    uint8_t feedsChecked = 0;
    uint8_t articlesSaved = 0;
    uint8_t articlesSkipped = 0;
    String summary;
    String detail;
  };

  Result checkFeeds(const OtaUpdater::Config &wifiConfig, Preferences &preferences,
                    StatusCallback callback = nullptr, void *context = nullptr);

 private:
  bool connectWiFi(const OtaUpdater::Config &wifiConfig, StatusCallback callback, void *context);
  void disconnectWiFi();
  bool fetchUrl(const String &url, String &body, String &error, uint8_t feedIndex,
                uint8_t feedCount, StatusCallback callback, void *context);
  bool processFeed(const String &feedUrl, const String &feedBody, Preferences &preferences,
                   Result &result, uint8_t feedIndex, uint8_t feedCount, StatusCallback callback,
                   void *context);
  bool saveItem(const feedparser::FeedItem &item, Preferences &preferences, Result &result);
  bool itemAlreadySeen(const feedparser::FeedItem &item, Preferences &preferences);
  void markItemSeen(const feedparser::FeedItem &item, Preferences &preferences);
  String seenKeyForItem(const feedparser::FeedItem &item) const;
  String itemIdentity(const feedparser::FeedItem &item) const;
  String filenameForItem(const feedparser::FeedItem &item) const;
  String metadataSafe(String value) const;
  uint32_t fnv1a(const String &value) const;
  void report(StatusCallback callback, void *context, const String &line1, const String &line2,
              int progressPercent);
};
