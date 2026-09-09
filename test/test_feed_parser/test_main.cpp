#include <glaze/json.hpp>
#include <unity.h>
#include <unordered_set>

#include "hash/Fnv1a.h"
#include "feeds/FeedParser.h"
#include "feeds/FeedSync.h"
#include "feeds/RssConfig.h"
#include "text/AsciiText.h"
#include "text/TextNormalizer.h"

void setUp() {}

void tearDown() {}

namespace {

    feedparser::FeedItem firstItem(std::string_view feed) {
        size_t cursor = 0;
        feedparser::FeedItem item;
        feedparser::parseNextItem(feed, cursor, item);
        return item;
    }

} // namespace

void test_parses_rss_item_fields() {
    constexpr std::string_view feed = "<rss><channel>"
                                      "<item>"
                                      "<title>Hello World</title>"
                                      "<link>https://example.com/a</link>"
                                      "<description>Body text here.</description>"
                                      "</item>"
                                      "</channel></rss>";
    const feedparser::FeedItem item = firstItem(feed);
    TEST_ASSERT_EQUAL_STRING("Hello World", item.title.c_str());
    TEST_ASSERT_EQUAL_STRING("https://example.com/a", item.link.c_str());
    TEST_ASSERT_EQUAL_STRING("Body text here.", item.body.c_str());
}

void test_parses_atom_entry_with_href_link() {
    constexpr std::string_view feed = "<feed>"
                                      "<entry>"
                                      "<title>Atom Title</title>"
                                      "<link href=\"https://example.com/atom\"/>"
                                      "<summary>Summary body.</summary>"
                                      "</entry>"
                                      "</feed>";
    const feedparser::FeedItem item = firstItem(feed);
    TEST_ASSERT_EQUAL_STRING("Atom Title", item.title.c_str());
    TEST_ASSERT_EQUAL_STRING("https://example.com/atom", item.link.c_str());
    TEST_ASSERT_EQUAL_STRING("Summary body.", item.body.c_str());
}

void test_parses_tags_case_insensitively() {
    constexpr std::string_view feed =
        "<RSS><CHANNEL><ITEM><TITLE>Upper</TITLE><DESCRIPTION>Body</DESCRIPTION></ITEM></CHANNEL></RSS>";
    const feedparser::FeedItem item = firstItem(feed);
    TEST_ASSERT_EQUAL_STRING("Upper", item.title.c_str());
    TEST_ASSERT_EQUAL_STRING("Body", item.body.c_str());
    TEST_ASSERT_TRUE(feedparser::hasCompleteFeed(feed));
}

void test_decodes_entities_and_strips_html() {
    // stripHtml runs before entity decoding, so real markup tags become spaces
    // and named entities decode to their character.
    constexpr std::string_view feed = "<rss><channel><item>"
                                      "<title>Tom &amp; Jerry</title>"
                                      "<link>https://example.com/x</link>"
                                      "<description><p>Hello <b>there</b></p></description>"
                                      "</item></channel></rss>";
    const feedparser::FeedItem item = firstItem(feed);
    TEST_ASSERT_EQUAL_STRING("Tom & Jerry", item.title.c_str());
    TEST_ASSERT_EQUAL_STRING("Hello there", item.body.c_str());
}

void test_decodes_numeric_nested_and_extended_entities() {
    constexpr std::string_view feed = "<rss><channel><item>"
                                      "<title>A&#x2014;B &amp;amp; &OElig;uvre</title>"
                                      "<description>Body.</description>"
                                      "</item></channel></rss>";
    const feedparser::FeedItem item = firstItem(feed);
    constexpr std::string_view expected = "A\xE2\x80\x94"
                                          "B & \xC5\x92uvre";
    TEST_ASSERT_EQUAL_STRING(expected.data(), item.title.c_str());
}

void test_unwraps_cdata_in_body() {
    constexpr std::string_view feed = "<rss><channel><item>"
                                      "<title>T</title>"
                                      "<link>https://example.com/y</link>"
                                      "<description><![CDATA[Plain cdata text]]></description>"
                                      "</item></channel></rss>";
    const feedparser::FeedItem item = firstItem(feed);
    TEST_ASSERT_EQUAL_STRING("Plain cdata text", item.body.c_str());
}

void test_falls_back_to_host_for_author() {
    constexpr std::string_view feed = "<rss><channel><item>"
                                      "<title>No Author</title>"
                                      "<link>https://www.example.com/post</link>"
                                      "<description>Body.</description>"
                                      "</item></channel></rss>";
    const feedparser::FeedItem item = firstItem(feed);
    TEST_ASSERT_EQUAL_STRING("example.com", item.author.c_str());
}

void test_prefers_dc_creator_author() {
    constexpr std::string_view feed = "<rss><channel><item>"
                                      "<title>T</title>"
                                      "<link>https://example.com/z</link>"
                                      "<dc:creator>Jane Doe</dc:creator>"
                                      "<description>Body.</description>"
                                      "</item></channel></rss>";
    const feedparser::FeedItem item = firstItem(feed);
    TEST_ASSERT_EQUAL_STRING("Jane Doe", item.author.c_str());
}

void test_iterates_multiple_items_then_stops() {
    constexpr std::string_view feed =
        "<rss><channel>"
        "<item><title>One</title><link>https://e.com/1</link><description>b1</description></item>"
        "<item><title>Two</title><link>https://e.com/2</link><description>b2</description></item>"
        "</channel></rss>";
    size_t cursor = 0;
    feedparser::FeedItem a;
    feedparser::FeedItem b;
    feedparser::FeedItem c;
    TEST_ASSERT_TRUE(feedparser::parseNextItem(feed, cursor, a));
    TEST_ASSERT_TRUE(feedparser::parseNextItem(feed, cursor, b));
    TEST_ASSERT_FALSE(feedparser::parseNextItem(feed, cursor, c));
    TEST_ASSERT_EQUAL_STRING("One", a.title.c_str());
    TEST_ASSERT_EQUAL_STRING("Two", b.title.c_str());
}

void test_parses_complete_item_from_partial_feed() {
    constexpr std::string_view feed =
        "<rss><channel>"
        "<item><title>One</title><link>https://e.com/1</link><description>b1</description></item>";

    feedparser::FeedItem item;
    size_t cursor = 0;
    TEST_ASSERT_TRUE(feedparser::parseNextItem(feed, cursor, item));
    TEST_ASSERT_EQUAL_STRING("One", item.title.c_str());
    TEST_ASSERT_EQUAL_STRING("b1", item.body.c_str());
}

void test_detects_complete_feed_and_advances_over_items() {
    const std::string partial = "<feed><entry><title>One</title><summary>Body</summary></entry>"
                                "<entry><title>Two</title>";
    size_t cursor = 0;
    TEST_ASSERT_TRUE(feedparser::advancePastItem(partial, cursor));
    TEST_ASSERT_FALSE(feedparser::advancePastItem(partial, cursor));
    TEST_ASSERT_FALSE(feedparser::hasCompleteFeed(partial));
    TEST_ASSERT_TRUE(feedparser::hasCompleteFeed(partial + "</entry></feed>"));
}

void test_sync_reaches_later_items_across_checks_and_skips_empty_entries() {
    for (const bool atom: {false, true}) {
        std::string feed = atom ? "<feed>" : "<rss><channel>";
        const std::string open = atom ? "<EnTrY lang=\"en\">" : "<ItEm lang=\"en\">";
        const std::string close = atom ? "</entry>" : "</item>";
        std::unordered_set<std::string> seen;
        seen.reserve(317);
        for (size_t i = 0; i < 317; ++i) {
            const std::string link = "https://example.com/" + std::to_string(i);
            feed += open + "<title>Article</title>"
                  + (atom ? "<link href=\"" + link + "\"/><summary>Text</summary>"
                          : "<link>" + link + "</link><description>Text</description>")
                  + close;
            if (i < 300)
                seen.insert(link);
            if (i == 300)
                feed += open + "<title>Empty entry must not end the feed</title>" + close;
        }
        feed += atom ? "</feed>" : "</channel></rss>";
        const auto alreadySeen = [&](const feedparser::FeedItem& item) {
            return seen.contains(item.link);
        };
        const auto save = [&](const feedparser::FeedItem& item) {
            return seen.insert(item.link).second;
        };

        const auto first = rss::syncFeed(feed, rss::kMaxArticlesPerCheck, alreadySeen, save);
        TEST_ASSERT_EQUAL(12, first.saved);
        TEST_ASSERT_EQUAL(300, first.skipped);
        TEST_ASSERT_TRUE(seen.contains("https://example.com/311"));
        TEST_ASSERT_FALSE(seen.contains("https://example.com/312"));
        const auto second = rss::syncFeed(feed, rss::kMaxArticlesPerCheck, alreadySeen, save);
        TEST_ASSERT_EQUAL(5, second.saved);
        TEST_ASSERT_EQUAL(312, second.skipped);
        TEST_ASSERT_TRUE(seen.contains("https://example.com/316"));
        const auto third = rss::syncFeed(feed, rss::kMaxArticlesPerCheck, alreadySeen, save);
        TEST_ASSERT_EQUAL(0, third.saved);
        TEST_ASSERT_EQUAL(317, third.skipped);
    }
}

void test_item_opening_tags_require_a_name_boundary() {
    for (const std::string tag: {"item", "entry"}) {
        const std::string feed = "<" + tag + "Info><title>Wrong</title></" + tag + "Info>" + "<" + tag
                               + "><title>Right</title><summary>Body</summary></" + tag + ">";
        const auto item = firstItem(feed);
        TEST_ASSERT_EQUAL_STRING("Right", item.title.c_str());
    }
}

void test_sync_budget_counts_successes_and_partial_atom_feed_remains_usable() {
    constexpr std::string_view feed = "<feed><entry><title>Empty</title></entry>"
                                      "<entry><title>Write fails</title><summary>Body</summary></entry>"
                                      "<entry><title>Saved</title><summary>Body</summary></entry>"
                                      "<entry><title>Next check</title><summary>Body</summary></entry>"
                                      "<entry><title>Incomplete";
    size_t writes = 0;
    const auto unseen = [](const feedparser::FeedItem&) {
        return false;
    };
    const auto save = [&](const feedparser::FeedItem& item) {
        ++writes;
        return item.title != "Write fails";
    };
    // This feed gets the remaining one save in a check shared with other feeds.
    const auto result = rss::syncFeed(feed, 1, unseen, save);
    TEST_ASSERT_EQUAL(1, result.saved);
    TEST_ASSERT_EQUAL(2, result.scanned);
    TEST_ASSERT_EQUAL(2, writes);
    TEST_ASSERT_EQUAL(0, rss::syncFeed(feed, 0, unseen, save).scanned);
    TEST_ASSERT_EQUAL(2, writes);
    const auto allComplete = rss::syncFeed(feed, rss::kMaxArticlesPerCheck, unseen, save);
    TEST_ASSERT_EQUAL(2, allComplete.saved);
    TEST_ASSERT_EQUAL(3, allComplete.scanned);
}

void test_preserves_long_full_text_content() {
    std::string longBody;
    longBody.reserve(140000);
    for (int i = 0; i < 3000; ++i) {
        longBody += "Long article paragraph with readable text. ";
    }
    longBody += "tail-marker";

    const std::string feed = "<rss><channel><item>"
                             "<title>Long Read</title>"
                             "<link>https://example.com/long</link>"
                             "<content:encoded><![CDATA["
                           + longBody
                           + "]]></content:encoded>"
                             "</item></channel></rss>";

    const feedparser::FeedItem item = firstItem(feed);
    TEST_ASSERT_TRUE(item.body.length() > 64000);
    TEST_ASSERT_TRUE(item.body.ends_with("tail-marker"));
}

void test_host_label_strips_scheme_and_www() {
    TEST_ASSERT_EQUAL_STRING("example.com", feedparser::hostLabelForUrl("https://www.example.com/path?x=1").c_str());
}

void test_rss_config_round_trip_and_normalization() {
    rss::Config config{.feeds = {" https://example.com/feed ", "https://example.com/feed", "http://example.org/rss"}};
    const auto encoded = rss::encodeToml(config);
    TEST_ASSERT_TRUE(encoded.has_value());
    TEST_ASSERT_EQUAL(std::string::npos, encoded->find("schemaVersion"));

    const auto decoded = rss::decodeToml(*encoded);
    TEST_ASSERT_TRUE(decoded.has_value());
    TEST_ASSERT_EQUAL(2, decoded->feeds.size());
    TEST_ASSERT_EQUAL_STRING("https://example.com/feed", decoded->feeds[0].c_str());
    TEST_ASSERT_TRUE(rss::decodeToml("obsolete = true\n").has_value());
    TEST_ASSERT_FALSE(rss::decodeToml("feeds = [\"ftp://example.com/feed\"]\n").has_value());

    std::string json;
    TEST_ASSERT_FALSE(glz::write_json(*decoded, json));
    rss::Config fromJson;
    TEST_ASSERT_FALSE(glz::read_json(fromJson, json));
    TEST_ASSERT_EQUAL(2, fromJson.feeds.size());
}

void test_standard_error_codes_are_preserved() {
    TEST_ASSERT_TRUE(rss::decodeToml("feeds = [\"ftp://example.com/feed\"]\n").error() == std::errc::invalid_argument);

    rss::Config tooMany;
    for (size_t index = 0; index <= rss::kMaxFeeds; ++index)
        tooMany.feeds.push_back("https://example.com/" + std::to_string(index));
    TEST_ASSERT_TRUE(rss::normalize(tooMany).error() == std::errc::no_buffer_space);
    TEST_ASSERT_TRUE(rss::decodeToml(std::string(rss::kMaxConfigBytes + 1, 'x')).error() == std::errc::value_too_large);

    TEST_ASSERT_EQUAL(255, *AsciiText::parseUnsigned<uint16_t>("ff", 16));
    TEST_ASSERT_TRUE(AsciiText::parseUnsigned<uint16_t>("12x").error() == std::errc::invalid_argument);
    TEST_ASSERT_TRUE(AsciiText::parseUnsigned<uint8_t>("256").error() == std::errc::result_out_of_range);
}

void test_text_normalizer_preserves_utf8_and_rejects_malformed_bytes() {
    RsvpText::NormalizationStats stats;
    const std::string normalized =
        RsvpText::normalizeDisplayText("\xC4\x8C"
                                       "esk\xC3\xBD \xD0\x9F\xD1\x80\xD0\xB8\xD0\xB2\xD0\xB5\xD1\x82\xC2\xA0world",
                                       &stats);
    TEST_ASSERT_EQUAL_STRING("\xC4\x8C"
                             "esk\xC3\xBD \xD0\x9F\xD1\x80\xD0\xB8\xD0\xB2\xD0\xB5\xD1\x82 world",
                             normalized.c_str());
    TEST_ASSERT_EQUAL(0, stats.malformedUtf8);
    TEST_ASSERT_EQUAL(9, stats.nonAsciiCodepoints);

    const char malformedBytes[] = {'A', static_cast<char>(0xFF), 'B', '\0'};
    stats = {};
    TEST_ASSERT_EQUAL_STRING("A?B", RsvpText::normalizeDisplayText(malformedBytes, &stats).c_str());
    TEST_ASSERT_EQUAL(1, stats.malformedUtf8);
}

void test_metadata_preserves_scripts_for_locale_pack_fonts() {
    TEST_ASSERT_EQUAL_STRING("Alice 愛麗絲 - Алиса",
                             RsvpText::normalizeDisplayText("Alice 愛麗絲 - Алиса").c_str());
}

void test_fnv1a_supports_whole_and_incremental_hashing() {
    constexpr std::string_view text = "hello";
    TEST_ASSERT_EQUAL_HEX32(0x4F9F2CABU, Fnv1a::hash(text));
    TEST_ASSERT_EQUAL_HEX32(Fnv1a::hash(text),
                            Fnv1a::append(Fnv1a::append(Fnv1a::kOffsetBasis, text.substr(0, 2)), text.substr(2)));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_parses_rss_item_fields);
    RUN_TEST(test_parses_atom_entry_with_href_link);
    RUN_TEST(test_parses_tags_case_insensitively);
    RUN_TEST(test_decodes_entities_and_strips_html);
    RUN_TEST(test_decodes_numeric_nested_and_extended_entities);
    RUN_TEST(test_unwraps_cdata_in_body);
    RUN_TEST(test_falls_back_to_host_for_author);
    RUN_TEST(test_prefers_dc_creator_author);
    RUN_TEST(test_iterates_multiple_items_then_stops);
    RUN_TEST(test_parses_complete_item_from_partial_feed);
    RUN_TEST(test_detects_complete_feed_and_advances_over_items);
    RUN_TEST(test_sync_reaches_later_items_across_checks_and_skips_empty_entries);
    RUN_TEST(test_item_opening_tags_require_a_name_boundary);
    RUN_TEST(test_sync_budget_counts_successes_and_partial_atom_feed_remains_usable);
    RUN_TEST(test_preserves_long_full_text_content);
    RUN_TEST(test_host_label_strips_scheme_and_www);
    RUN_TEST(test_rss_config_round_trip_and_normalization);
    RUN_TEST(test_standard_error_codes_are_preserved);
    RUN_TEST(test_text_normalizer_preserves_utf8_and_rejects_malformed_bytes);
    RUN_TEST(test_metadata_preserves_scripts_for_locale_pack_fonts);
    RUN_TEST(test_fnv1a_supports_whole_and_incremental_hashing);
    return UNITY_END();
}
