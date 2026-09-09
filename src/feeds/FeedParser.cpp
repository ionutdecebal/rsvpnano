#include "feeds/FeedParser.h"

#include <algorithm>
#include <cctype>
#include <ranges>

#include "text/AsciiText.h"
#include "text/TextNormalizer.h"

namespace feedparser {
    namespace {

        size_t indexOfIgnoreCase(std::string_view text, std::string_view needle, size_t start, size_t limit) {
            if (needle.empty() || start >= text.size()) {
                return std::string_view::npos;
            }
            limit = std::min(limit, text.size());
            if (limit < start || limit - start < needle.size()) {
                return std::string_view::npos;
            }

            const std::string_view haystack = text.substr(start, limit - start);
            const auto match = std::ranges::search(haystack, needle, [](char left, char right) {
                return AsciiText::toLower(left) == AsciiText::toLower(right);
            });
            return match.begin() == haystack.end()
                     ? std::string_view::npos
                     : start + static_cast<size_t>(std::ranges::distance(haystack.begin(), match.begin()));
        }

        size_t tagEndIndex(std::string_view text, size_t start, size_t limit) {
            const size_t end = text.find('>', start);
            return end < std::min(limit, text.size()) ? end : std::string_view::npos;
        }

        struct ItemBounds {
            size_t start;
            size_t end;
            size_t next;
        };

        bool opensTag(std::string_view text, std::string_view name) {
            if (text.size() <= name.size())
                return false;
            const char boundary = text[name.size()];
            return (boundary == '>' || boundary == '/' || std::isspace(static_cast<unsigned char>(boundary)))
                && std::ranges::equal(text.substr(0, name.size()), name, [](char left, char right) {
                       return AsciiText::toLower(left) == right;
                   });
        }

        bool findNextItem(std::string_view feedBody, size_t searchStart, ItemBounds& bounds) {
            size_t itemStart = feedBody.find('<', searchStart);
            std::string_view closeTag;
            // Recognize either format in one forward pass; scanning for all RSS
            // tags before trying Atom would rescan the remaining feed per entry.
            for (; itemStart != std::string_view::npos; itemStart = feedBody.find('<', itemStart + 1)) {
                const auto tag = feedBody.substr(itemStart + 1);
                if (opensTag(tag, "item")) {
                    closeTag = "</item>";
                    break;
                }
                if (opensTag(tag, "entry")) {
                    closeTag = "</entry>";
                    break;
                }
            }
            if (itemStart == std::string_view::npos)
                return false;

            const size_t itemEnd = indexOfIgnoreCase(feedBody, closeTag, itemStart, feedBody.size());
            if (itemEnd == std::string_view::npos) {
                return false;
            }

            bounds = {
                .start = itemStart,
                .end = itemEnd,
                .next = itemEnd + closeTag.size(),
            };
            return true;
        }

        std::string valueBetween(std::string_view text, std::string_view openTag, std::string_view closeTag,
                                 size_t start, size_t end) {
            const size_t open = indexOfIgnoreCase(text, openTag, start, end);
            if (open == std::string_view::npos || open >= end) {
                return {};
            }
            const size_t valueStart = tagEndIndex(text, open, end);
            if (valueStart == std::string_view::npos || valueStart >= end) {
                return {};
            }
            const size_t close = indexOfIgnoreCase(text, closeTag, valueStart + 1, end);
            if (close == std::string_view::npos || close > end) {
                return {};
            }
            return std::string{text.substr(valueStart + 1, close - valueStart - 1)};
        }

        std::string attributeValue(std::string_view text, std::string_view tagPrefix, std::string_view attribute,
                                   size_t start, size_t end) {
            size_t tagStart = indexOfIgnoreCase(text, tagPrefix, start, end);
            while (tagStart != std::string_view::npos && tagStart < end) {
                const size_t tagEnd = tagEndIndex(text, tagStart, end);
                if (tagEnd == std::string_view::npos || tagEnd > end) {
                    return {};
                }

                const std::string needle = std::string{attribute} + "=";
                const size_t attrIndex = indexOfIgnoreCase(text, needle, tagStart, tagEnd);
                if (attrIndex != std::string_view::npos) {
                    size_t valueStart = attrIndex + needle.length();
                    while (valueStart < tagEnd && isspace(static_cast<unsigned char>(text[valueStart]))) {
                        ++valueStart;
                    }
                    if (valueStart < tagEnd) {
                        const char quote = text[valueStart];
                        if (quote == '"' || quote == '\'') {
                            ++valueStart;
                            const size_t valueEnd = text.find(quote, valueStart);
                            if (valueEnd < tagEnd) {
                                return std::string{text.substr(valueStart, valueEnd - valueStart)};
                            }
                        } else {
                            size_t valueEnd = valueStart;
                            while (valueEnd < tagEnd && !isspace(static_cast<unsigned char>(text[valueEnd]))
                                   && text[valueEnd] != '>') {
                                ++valueEnd;
                            }
                            if (valueEnd > valueStart) {
                                return std::string{text.substr(valueStart, valueEnd - valueStart)};
                            }
                        }
                    }
                }

                tagStart = indexOfIgnoreCase(text, tagPrefix, tagEnd + 1, end);
            }
            return {};
        }

        std::string stripHtml(std::string_view html) {
            std::string output;
            output.reserve(std::min(html.size(), kMaxArticleChars));
            bool inTag = false;
            for (const char c: html) {
                if (c == '<') {
                    inTag = true;
                    if (!output.ends_with(' ') && !output.ends_with('\n')) {
                        output += ' ';
                    }
                    continue;
                }
                if (c == '>') {
                    inTag = false;
                    continue;
                }
                if (!inTag) {
                    output += c;
                }
                if (output.length() >= kMaxArticleChars) {
                    break;
                }
            }
            return output;
        }

        void eraseAll(std::string& text, std::string_view needle) {
            for (size_t position = 0; (position = text.find(needle, position)) != std::string::npos;) {
                text.erase(position, needle.size());
            }
        }

        std::string cleanText(std::string value) {
            eraseAll(value, "<![CDATA[");
            eraseAll(value, "]]>");
            value = stripHtml(value);
            value = RsvpText::decodeMarkupEntities(value);
            if (value.contains('&')) {
                value = RsvpText::decodeMarkupEntities(value);
            }
            std::ranges::replace(value, '\r', '\n');
            while (value.contains("\n\n\n")) {
                const size_t position = value.find("\n\n\n");
                value.erase(position, 1);
            }
            return std::string{AsciiText::trim(value)};
        }

    } // namespace

    bool hasCompleteFeed(std::string_view feedBody, size_t searchStart) {
        return indexOfIgnoreCase(feedBody, "</rss>", searchStart, feedBody.size()) != std::string_view::npos
            || indexOfIgnoreCase(feedBody, "</feed>", searchStart, feedBody.size()) != std::string_view::npos;
    }

    bool advancePastItem(std::string_view feedBody, size_t& searchStart) {
        ItemBounds bounds{};
        if (!findNextItem(feedBody, searchStart, bounds)) {
            return false;
        }
        searchStart = bounds.next;
        return true;
    }

    std::string hostLabelForUrl(std::string_view url) {
        const size_t scheme = url.find("://");
        const size_t start = scheme == std::string_view::npos ? 0 : scheme + 3;
        const size_t slash = url.find('/', start);
        std::string_view host = url.substr(start, slash == std::string_view::npos ? slash : slash - start);
        if (host.starts_with("www.")) {
            host.remove_prefix(4);
        }
        return std::string{host};
    }

    std::string sourceLabelForItem(const FeedItem& item) {
        if (item.link.empty()) {
            return "RSS";
        }

        std::string source = hostLabelForUrl(item.link);
        return source.empty() ? "RSS" : std::move(source);
    }

    bool parseNextItem(std::string_view feedBody, size_t& searchStart, FeedItem& item) {
        ItemBounds bounds{};
        while (findNextItem(feedBody, searchStart, bounds)) {
            searchStart = bounds.next;

            item.title = cleanText(valueBetween(feedBody, "<title", "</title>", bounds.start, bounds.end));
            item.link = cleanText(valueBetween(feedBody, "<link>", "</link>", bounds.start, bounds.end));
            if (item.link.empty()) {
                item.link = cleanText(attributeValue(feedBody, "<link", "href", bounds.start, bounds.end));
            }
            if (item.link.empty()) {
                item.link = cleanText(valueBetween(feedBody, "<guid", "</guid>", bounds.start, bounds.end));
            }
            item.author = cleanText(valueBetween(feedBody, "<author", "</author>", bounds.start, bounds.end));
            if (item.author.empty()) {
                item.author =
                    cleanText(valueBetween(feedBody, "<dc:creator", "</dc:creator>", bounds.start, bounds.end));
            }
            if (item.author.empty()) {
                item.author = sourceLabelForItem(item);
            }

            item.body =
                cleanText(valueBetween(feedBody, "<content:encoded", "</content:encoded>", bounds.start, bounds.end));
            if (item.body.empty()) {
                item.body = cleanText(valueBetween(feedBody, "<content", "</content>", bounds.start, bounds.end));
            }
            if (item.body.empty()) {
                item.body =
                    cleanText(valueBetween(feedBody, "<description", "</description>", bounds.start, bounds.end));
            }
            if (item.body.empty()) {
                item.body = cleanText(valueBetween(feedBody, "<summary", "</summary>", bounds.start, bounds.end));
            }
            if (item.body.empty()) {
                item.body = item.link;
            }

            if (item.title.empty()) {
                item.title = item.link.empty() ? "RSS Article" : item.link;
            }
            if (!item.body.empty())
                return true;
        }
        return false;
    }

} // namespace feedparser
