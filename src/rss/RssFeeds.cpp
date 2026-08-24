#include "rss/RssFeeds.h"
#include <esp_log.h>

#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <algorithm>
#include <array>
#include <expected>
#include <iterator>
#include <ranges>
#include <string>
#include <vector>
#include "board/BoardStorage.h"

#include "hash/Fnv1a.h"
#include "logging/Logger.h"
#include "net/WifiConnection.h"
#include "rss/FeedParser.h"
#include "rss/RssConfig.h"
#include "rss/RssConfigStorage.h"
#include "storage/fs/StorageFiles.h"
#include "storage/fs/StoragePaths.h"
#include "text/AsciiText.h"

namespace {

    constexpr const char* kStatusTitle = "RSS";
    constexpr uint32_t kFeedRequestTimeoutMs = 30000;
    constexpr uint32_t kFeedTotalTimeoutMs = 90000;
    constexpr uint32_t kFeedIdleTimeoutMs = 15000;
    constexpr uint32_t kFeedProgressIntervalMs = 1000;
    constexpr size_t kMaxFeedBytes = 4UL * 1024UL * 1024UL;
    constexpr uint8_t kMaxFeedsPerCheck = 8;
    constexpr uint8_t kMaxItemsPerFeed = 5;
    constexpr uint8_t kMaxArticlesPerCheck = 12;
    constexpr uint8_t kMaxFeedRedirects = 3;

    constexpr const char* kUserAgent = "RSVP-Nano-RSS/1.0";

    std::string feedProgressLabel(uint8_t feedIndex, uint8_t feedCount) {
        return "Feed " + std::to_string(feedIndex) + "/" + std::to_string(feedCount);
    }

    bool isRedirectStatus(int statusCode) {
        static constexpr std::array kRedirectStatuses = {
            HTTP_CODE_MOVED_PERMANENTLY,  HTTP_CODE_FOUND, HTTP_CODE_SEE_OTHER, HTTP_CODE_TEMPORARY_REDIRECT,
            HTTP_CODE_PERMANENT_REDIRECT,
        };
        return std::ranges::find(kRedirectStatuses, statusCode) != kRedirectStatuses.end();
    }

    const char* friendlyHttpError(int statusCode) {
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

    std::string_view urlScheme(std::string_view url) {
        const size_t marker = url.find("://");
        if (marker == std::string_view::npos) {
            return "http";
        }
        return url.substr(0, marker);
    }

    std::string_view urlOrigin(std::string_view url) {
        const size_t marker = url.find("://");
        const size_t hostStart = marker == std::string_view::npos ? 0 : marker + 3;
        const size_t hostEnd = url.find('/', hostStart);
        return url.substr(0, hostEnd);
    }

    std::string resolveRedirectUrl(std::string_view baseUrl, std::string_view location) {
        location = AsciiText::trim(location);
        if (location.starts_with("http://") || location.starts_with("https://")) {
            return std::string{location};
        }
        if (location.starts_with("//")) {
            return std::string{urlScheme(baseUrl)} + ":" + std::string{location};
        }
        if (location.starts_with('/')) {
            return std::string{urlOrigin(baseUrl)} + std::string{location};
        }

        const size_t slash = baseUrl.rfind('/');
        const size_t marker = baseUrl.find("://");
        if (slash == std::string_view::npos || slash <= (marker == std::string_view::npos ? 0 : marker + 2)) {
            return std::string{urlOrigin(baseUrl)} + "/" + std::string{location};
        }
        return std::string{baseUrl.substr(0, slash + 1)} + std::string{location};
    }

    std::string_view itemIdentity(const feedparser::FeedItem& item) {
        return item.link.empty() ? item.title : item.link;
    }

    std::string seenKeyForItem(const feedparser::FeedItem& item) {
        char key[16];
        std::snprintf(key, sizeof(key), "rss%08lx", static_cast<unsigned long>(Fnv1a::hash(itemIdentity(item))));
        return key;
    }

    bool itemAlreadySeen(const feedparser::FeedItem& item, Preferences& preferences) {
        return preferences.getBool(seenKeyForItem(item).c_str(), false);
    }

    void markItemSeen(const feedparser::FeedItem& item, Preferences& preferences) {
        preferences.putBool(seenKeyForItem(item).c_str(), true);
    }

    std::string filenameForItem(const feedparser::FeedItem& item) {
        std::string cleaned =
            StoragePaths::sanitizeFilename(std::string_view{item.title}.substr(0, std::min<size_t>(item.title.size(),
                                                                                                   72)));
        while (cleaned.contains("--")) {
            const size_t position = cleaned.find("--");
            cleaned.erase(position, 1);
        }
        if (cleaned.empty()) {
            cleaned = "rss-article";
        }
        char suffix[16];
        std::snprintf(suffix, sizeof(suffix), "-%08lx", static_cast<unsigned long>(Fnv1a::hash(itemIdentity(item))));
        return cleaned + suffix + ".rsvp";
    }

    std::string metadataSafe(std::string value) {
        std::ranges::replace(value, '\r', ' ');
        std::ranges::replace(value, '\n', ' ');
        return std::string{AsciiText::trim(value)};
    }

    void report(RssFeeds::StatusCallback callback, void* context, const char* line1, const char* line2,
                int progressPercent) {
        if (callback == nullptr) {
            return;
        }
        callback(context, kStatusTitle, line1, line2, progressPercent);
    }

    std::expected<std::string, std::string> fetchUrl(std::string_view url, uint8_t feedIndex, uint8_t feedCount,
                                                     RssFeeds::StatusCallback callback, void* context) {
        std::string currentUrl{url};
        for (uint8_t redirectCount = 0; redirectCount <= kMaxFeedRedirects; ++redirectCount) {
            WiFiClientSecure secureClient;
            WiFiClient plainClient;
            secureClient.setInsecure();
            secureClient.setHandshakeTimeout(15);

            HTTPClient http;
            http.setUserAgent(kUserAgent);
            http.setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS);
            http.setTimeout(kFeedRequestTimeoutMs);
            const char* headers[] = {"Location"};
            http.collectHeaders(headers, 1);

            const bool ok = currentUrl.starts_with("https://") ? http.begin(secureClient, currentUrl.c_str())
                                                               : http.begin(plainClient, currentUrl.c_str());
            if (!ok)
                return std::unexpected(std::string{"Feed link did not open"});

            const std::string progressLabel = feedProgressLabel(feedIndex, feedCount);
            const std::string requestLabel = "Requesting " + feedparser::hostLabelForUrl(currentUrl);
            report(callback, context, progressLabel.c_str(), requestLabel.c_str(), 18 + feedIndex * 7);
            const int statusCode = http.GET();
            if (isRedirectStatus(statusCode)) {
                const String locationHeader = http.header("Location");
                http.end();
                if (locationHeader.isEmpty())
                    return std::unexpected(std::string{"Feed moved but gave no link"});
                currentUrl = resolveRedirectUrl(currentUrl, {locationHeader.c_str(), locationHeader.length()});
                ESP_LOGD("rss", "redirect %u url=%s", static_cast<unsigned int>(statusCode), currentUrl.c_str());
                const std::string redirectLabel = "Redirecting to " + feedparser::hostLabelForUrl(currentUrl);
                report(callback, context, progressLabel.c_str(), redirectLabel.c_str(), 18 + feedIndex * 7);
                delay(250);
                continue;
            }
            if (statusCode != HTTP_CODE_OK) {
                const char* error = friendlyHttpError(statusCode);
                http.end();
                return std::unexpected(std::string{error});
            }

            WiFiClient* stream = http.getStreamPtr();
            if (stream == nullptr) {
                http.end();
                return std::unexpected(std::string{"No data from site"});
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
            std::string body;
            body.reserve(reserveBytes);
            const uint32_t startedMs = millis();
            uint32_t lastByteMs = startedMs;
            uint32_t lastReportMs = 0;
            while (http.connected() || stream->available()) {
                const uint32_t nowMs = millis();
                if (nowMs - startedMs > kFeedTotalTimeoutMs) {
                    if (completeItemsRead > 0) {
                        acceptedPartialFeed = true;
                        ESP_LOGW("rss", "total timeout after usable items url=%s bytes=%u items=%u", currentUrl.c_str(),
                                 static_cast<unsigned int>(totalRead), static_cast<unsigned int>(completeItemsRead));
                        break;
                    }
                    http.end();
                    return std::unexpected(std::string{"Site took too long"});
                }
                if (nowMs - lastByteMs > kFeedIdleTimeoutMs) {
                    if (completeItemsRead > 0) {
                        acceptedPartialFeed = true;
                        ESP_LOGD("rss", "idle after usable items url=%s bytes=%u items=%u", currentUrl.c_str(),
                                 static_cast<unsigned int>(totalRead), static_cast<unsigned int>(completeItemsRead));
                        break;
                    }
                    if (totalRead > 0 && feedparser::hasCompleteFeed(body)) {
                        ESP_LOGD("rss", "idle after complete feed url=%s bytes=%u", currentUrl.c_str(),
                                 static_cast<unsigned int>(totalRead));
                        break;
                    }
                    http.end();
                    return std::unexpected(std::string{"Site stopped sending data"});
                }
                if (nowMs - lastReportMs >= kFeedProgressIntervalMs) {
                    lastReportMs = nowMs;
                    const std::string downloaded = "Downloaded " + std::to_string(totalRead / 1024) + " KB";
                    report(callback, context, progressLabel.c_str(), downloaded.c_str(), 20 + feedIndex * 7);
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
                const size_t chunkSize = std::min(remaining, std::min(sizeof(buffer), static_cast<size_t>(available)));
                const int bytesRead = stream->readBytes(buffer, chunkSize);
                if (bytesRead <= 0) {
                    break;
                }
                lastByteMs = millis();
                const size_t previousRead = totalRead;
                totalRead += static_cast<size_t>(bytesRead);
                body.append(reinterpret_cast<const char*>(buffer), static_cast<size_t>(bytesRead));
                while (completeItemsRead < kMaxItemsPerFeed
                       && feedparser::advancePastItem(body, completeItemSearchStart)) {
                    ++completeItemsRead;
                }
                if (completeItemsRead >= kMaxItemsPerFeed) {
                    stoppedAfterItems = true;
                    ESP_LOGI("rss", "downloaded item limit url=%s bytes=%u items=%u", currentUrl.c_str(),
                             static_cast<unsigned int>(totalRead), static_cast<unsigned int>(completeItemsRead));
                    break;
                }
                const size_t closeSearchStart = previousRead > 16 ? previousRead - 16 : 0;
                if (feedparser::hasCompleteFeed(body, closeSearchStart)) {
                    break;
                }
            }
            http.end();

            if (body.empty())
                return std::unexpected(std::string{"Feed was empty"});
            if (totalRead >= kMaxFeedBytes) {
                ESP_LOGW("rss", "feed capped url=%s bytes=%u", currentUrl.c_str(),
                         static_cast<unsigned int>(totalRead));
                const std::string capped = "Reached " + std::to_string(kMaxFeedBytes / 1024) + " KB cap";
                report(callback, context, progressLabel.c_str(), capped.c_str(), 20 + feedIndex * 7);
                delay(500);
            } else if (stoppedAfterItems) {
                const std::string downloaded = "Downloaded " + std::to_string(completeItemsRead) + " items";
                report(callback, context, progressLabel.c_str(), downloaded.c_str(), 20 + feedIndex * 7);
            } else if (acceptedPartialFeed) {
                report(callback, context, progressLabel.c_str(), "Downloaded partial feed", 20 + feedIndex * 7);
            } else {
                const std::string downloaded = "Downloaded " + std::to_string(totalRead / 1024) + " KB";
                report(callback, context, progressLabel.c_str(), downloaded.c_str(), 20 + feedIndex * 7);
            }
            return body;
        }

        return std::unexpected(std::string{"Feed redirected too often"});
    }

    std::expected<void, std::error_code> saveItem(const feedparser::FeedItem& item, Preferences& preferences,
                                                  RssFeeds::Result& result) {
        if (auto directory = StorageFiles::ensureDirectory(StoragePaths::kLibraryPath); !directory)
            return directory;
        if (auto directory = StorageFiles::ensureDirectory(StoragePaths::kArticleFilesPath); !directory)
            return directory;
        const std::string finalPath = std::string(StoragePaths::kArticleFilesPath) + "/" + filenameForItem(item);
        const std::string tmpPath = finalPath + ".tmp";
        Board::Storage::filesystem().remove(tmpPath.c_str());

        File file = Board::Storage::filesystem().open(tmpPath.c_str(), FILE_WRITE);
        if (!file)
            return std::unexpected(std::make_error_code(std::errc::io_error));

        file.println("@rsvp 1");
        file.print("@title ");
        file.println(metadataSafe(item.title).c_str());
        file.print("@author ");
        file.println(metadataSafe(item.author.empty() ? feedparser::sourceLabelForItem(item) : item.author).c_str());
        if (!item.link.empty()) {
            file.print("@source ");
            file.println(metadataSafe(item.link).c_str());
        }
        file.println();

        std::string body = item.body;
        if (body.length() > feedparser::kMaxArticleChars) {
            body.resize(feedparser::kMaxArticleChars);
            body += "\n\n[Article truncated on device.]";
        }
        file.println(body.c_str());
        const bool writeFailed = file.getWriteError() != 0;
        file.close();
        if (writeFailed) {
            Board::Storage::filesystem().remove(tmpPath.c_str());
            return std::unexpected(std::make_error_code(std::errc::io_error));
        }

        Board::Storage::filesystem().remove(finalPath.c_str());
        if (!Board::Storage::filesystem().rename(tmpPath.c_str(), finalPath.c_str())) {
            Board::Storage::filesystem().remove(tmpPath.c_str());
            return std::unexpected(std::make_error_code(std::errc::io_error));
        }

        markItemSeen(item, preferences);
        ++result.articlesSaved;
        ESP_LOGI("rss", "saved %s", finalPath.c_str());
        return {};
    }

    bool processFeed(std::string_view feedUrl, std::string_view feedBody, Preferences& preferences,
                     RssFeeds::Result& result, uint8_t feedIndex, uint8_t feedCount, RssFeeds::StatusCallback callback,
                     void* context) {
        size_t searchStart = 0;
        uint8_t itemCount = 0;
        uint8_t savedBefore = result.articlesSaved;
        uint8_t skippedBefore = result.articlesSkipped;
        const std::string progressLabel = feedProgressLabel(feedIndex, feedCount);
        report(callback, context, progressLabel.c_str(), "Parsing items", 24 + feedIndex * 7);
        while (itemCount < kMaxItemsPerFeed && result.articlesSaved < kMaxArticlesPerCheck) {
            feedparser::FeedItem item;
            if (!feedparser::parseNextItem(feedBody, searchStart, item)) {
                break;
            }
            ++itemCount;
            if (itemAlreadySeen(item, preferences)) {
                ++result.articlesSkipped;
                const std::string synced =
                    "Already synced " + std::to_string(itemCount) + "/" + std::to_string(kMaxItemsPerFeed);
                report(callback, context, progressLabel.c_str(), synced.c_str(), 24 + feedIndex * 7);
                continue;
            }
            const std::string saving = "Saving article " + std::to_string(itemCount);
            report(callback, context, saving.c_str(), item.title.c_str(), 24 + feedIndex * 7);
            if (auto saved = saveItem(item, preferences, result); !saved)
                Logger::failure("rss", "save article", StoragePaths::kArticleFilesPath, saved.error());
        }
        const uint8_t savedHere = result.articlesSaved - savedBefore;
        const uint8_t skippedHere = result.articlesSkipped - skippedBefore;
        if (itemCount == 0) {
            report(callback, context, progressLabel.c_str(), "No usable items", 24 + feedIndex * 7);
        } else {
            const std::string saved = std::to_string(savedHere) + " saved, " + std::to_string(skippedHere) + " skipped";
            report(callback, context, progressLabel.c_str(), saved.c_str(), 24 + feedIndex * 7);
        }
        ESP_LOGW("rss", "feed url=%.*s items=%u saved=%u skipped=%u", static_cast<int>(feedUrl.size()), feedUrl.data(),
                 static_cast<unsigned int>(itemCount), static_cast<unsigned int>(savedHere),
                 static_cast<unsigned int>(skippedHere));
        delay(600);
        return itemCount > 0;
    }
} // namespace

RssFeeds::Result RssFeeds::check(Preferences& preferences, const settings::DeviceSettings& settings,
                                 const settings::DeviceSecrets& secrets, StatusCallback callback, void* context) {
    const std::string& ssid = settings.network.ssid;
    const std::string& wifiPassword = secrets.wifiPassword;

    Result result;
    if (AsciiText::trim(ssid).empty()) {
        result.summary = "Wi-Fi not set";
        result.detail = "Settings -> Wi-Fi";
        return result;
    }

    auto connected = net::connectStation(ssid.c_str(), wifiPassword.c_str(), [&](int percent) {
        report(callback, context, "Connecting Wi-Fi", ssid.c_str(), percent);
    });
    if (!connected) {
        net::disconnect();
        result.summary = "Wi-Fi failed";
        result.detail = connected.error().message();
        return result;
    }

    auto config = rss::load(Board::Storage::filesystem());
    if (!config) {
        net::disconnect();
        result.summary = config.error() == std::errc::no_such_file_or_directory ? "No feeds" : "Invalid feeds";
        result.detail = StoragePaths::kRssConfigPath;
        return result;
    }

    const size_t feedCount = std::min(config->feeds.size(), static_cast<size_t>(kMaxFeedsPerCheck));
    if (feedCount == 0) {
        net::disconnect();
        result.summary = "No feed URLs";
        result.detail = StoragePaths::kRssConfigPath;
        return result;
    }

    uint8_t feedFailures = 0;
    std::string firstFeedError;
    bool mixedFeedErrors = false;

    for (uint8_t feedIndex = 0; feedIndex < feedCount && result.articlesSaved < kMaxArticlesPerCheck; ++feedIndex) {
        const std::string& line = config->feeds[feedIndex];
        const uint8_t displayIndex = feedIndex + 1;
        const uint8_t displayFeedCount = static_cast<uint8_t>(feedCount);
        const std::string progressLabel = feedProgressLabel(displayIndex, displayFeedCount);
        const std::string downloading = "Downloading " + feedparser::hostLabelForUrl(line);
        report(callback, context, progressLabel.c_str(), downloading.c_str(), 15 + displayIndex * 8);

        auto feedBody = fetchUrl(line, displayIndex, displayFeedCount, callback, context);
        if (!feedBody) {
            const std::string& error = feedBody.error();
            ESP_LOGE("rss", "feed failed url=%s error=%s", line.c_str(), error.c_str());
            ++feedFailures;
            if (firstFeedError.empty()) {
                firstFeedError = error;
            } else if (error != firstFeedError) {
                mixedFeedErrors = true;
            }
            const std::string skipped = "Skipped: " + error;
            report(callback, context, progressLabel.c_str(), skipped.c_str(), 15 + displayIndex * 8);
            delay(600);
            continue;
        }

        ++result.feedsChecked;
        processFeed(line, *feedBody, preferences, result, displayIndex, displayFeedCount, callback, context);
    }

    net::disconnect();

    if (result.feedsChecked == 0) {
        result.summary = "Feeds unavailable";
        result.detail = mixedFeedErrors || firstFeedError.empty() ? "Check feed URLs" : firstFeedError;
    } else if (result.articlesSaved == 0) {
        result.summary = "No new articles";
        result.detail = std::to_string(result.feedsChecked) + " checked";
        if (feedFailures > 0) {
            result.detail += ", " + std::to_string(feedFailures) + " failed";
        }
    } else {
        result.summary =
            std::to_string(result.articlesSaved) + " article" + (result.articlesSaved == 1 ? "" : "s") + " saved";
        result.detail = std::to_string(result.feedsChecked) + " checked";
        if (feedFailures > 0) {
            result.detail += ", " + std::to_string(feedFailures) + " failed";
        }
    }
    return result;
}
