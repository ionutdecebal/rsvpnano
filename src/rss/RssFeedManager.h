#pragma once

#include <Arduino.h>
#include <Preferences.h>

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
  struct FeedItem {
    String title;
    String link;
    String author;
    String body;
  };

  bool connectWiFi(const OtaUpdater::Config &wifiConfig, StatusCallback callback, void *context);
  void disconnectWiFi();
  bool fetchUrl(const String &url, String &body, String &error, uint8_t feedIndex,
                uint8_t feedCount, StatusCallback callback, void *context);
  bool processFeed(const String &feedUrl, const String &feedBody, Preferences &preferences,
                   Result &result, uint8_t feedIndex, uint8_t feedCount, StatusCallback callback,
                   void *context);
  bool parseNextItem(const String &feedBody, size_t &searchStart, FeedItem &item);
  bool saveItem(const FeedItem &item, Preferences &preferences, Result &result);
  bool itemAlreadySeen(const FeedItem &item, Preferences &preferences);
  void markItemSeen(const FeedItem &item, Preferences &preferences);
  String seenKeyForItem(const FeedItem &item) const;
  String itemIdentity(const FeedItem &item) const;
  String valueBetween(const String &text, const String &openTag, const String &closeTag,
                      size_t start, size_t end) const;
  String attributeValue(const String &text, const String &tagPrefix, const String &attribute,
                        size_t start, size_t end) const;
  String cleanText(String value) const;
  String stripHtml(const String &html) const;
  String xmlDecode(String value) const;
  String sourceLabelForItem(const FeedItem &item) const;
  String filenameForItem(const FeedItem &item) const;
  String metadataSafe(String value) const;
  uint32_t fnv1a(const String &value) const;
  void report(StatusCallback callback, void *context, const String &line1, const String &line2,
              int progressPercent);
};
