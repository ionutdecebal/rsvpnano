#pragma once

#include <cstddef>
#include <string>
#include <string_view>

// Pure parsing of RSS/Atom feed bodies into article items, plus the text
// normalization (HTML stripping, XML entity decoding, character folding) that
// turns feed markup into reader-ready text. No networking, no SD access -- safe
// to unit test on the host.
namespace feedparser {

    constexpr size_t kMaxArticleChars = 512UL * 1024UL;

    struct FeedItem {
        std::string title;
        std::string link;
        std::string author;
        std::string body;
    };

    bool hasCompleteFeed(std::string_view feedBody, size_t searchStart = 0);
    bool advancePastItem(std::string_view feedBody, size_t& searchStart);

    // Parses the next usable <item> (RSS) or <entry> (Atom), skipping empty
    // entries and advancing searchStart. Returns false at the end of complete items.
    bool parseNextItem(std::string_view feedBody, size_t& searchStart, FeedItem& item);

    // The bare host of a URL: scheme and a leading "www." removed, path dropped.
    // Returns "" when no host can be found.
    std::string hostLabelForUrl(std::string_view url);

    // A human label for an item's origin: its link's host, or "RSS" when absent.
    std::string sourceLabelForItem(const FeedItem& item);

} // namespace feedparser
