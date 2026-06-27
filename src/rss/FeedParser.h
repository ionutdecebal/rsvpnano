#pragma once

#include <Arduino.h>

// Pure parsing of RSS/Atom feed bodies into article items, plus the text
// normalization (HTML stripping, XML entity decoding, character folding) that
// turns feed markup into reader-ready text. No networking, no SD access -- safe
// to unit test on the host with the Arduino String shim.
namespace feedparser {

constexpr size_t kMaxArticleChars = 512UL * 1024UL;

struct FeedItem {
  String title;
  String link;
  String author;
  String body;
};

// Parses the next <item> (RSS) or <entry> (Atom) starting at searchStart and
// advances searchStart past it. Returns false when no further item is found or
// the item has no usable body text.
bool parseNextItem(const String &feedBody, size_t &searchStart, FeedItem &item);

// The bare host of a URL: scheme and a leading "www." removed, path dropped.
// Returns "" when no host can be found.
String hostLabelForUrl(const String &url);

// A human label for an item's origin: its link's host, or "RSS" when absent.
String sourceLabelForItem(const FeedItem &item);

}  // namespace feedparser
