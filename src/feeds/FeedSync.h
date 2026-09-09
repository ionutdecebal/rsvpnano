#pragma once

#include "feeds/FeedParser.h"

namespace rss {

    constexpr size_t kMaxArticlesPerCheck = 12;

    struct SyncResult {
        size_t scanned = 0;
        size_t saved = 0;
        size_t skipped = 0;
    };

    // Only successful new saves consume the budget. A later check scans past
    // already-synced entries to reach the rest of an aggregated feed.
    inline SyncResult syncFeed(std::string_view body, size_t saveBudget, auto&& alreadySeen, auto&& save) {
        SyncResult result;
        size_t cursor = 0;
        feedparser::FeedItem item;
        while (result.saved < saveBudget && feedparser::parseNextItem(body, cursor, item)) {
            ++result.scanned;
            if (alreadySeen(item))
                ++result.skipped;
            else if (save(item))
                ++result.saved;
        }
        return result;
    }

} // namespace rss
