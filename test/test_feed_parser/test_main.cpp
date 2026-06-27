#include <unity.h>

#include "rss/FeedParser.h"

namespace {

feedparser::FeedItem firstItem(const String &feed) {
  size_t cursor = 0;
  feedparser::FeedItem item;
  feedparser::parseNextItem(feed, cursor, item);
  return item;
}

}  // namespace

void test_parses_rss_item_fields() {
  const String feed =
      "<rss><channel>"
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
  const String feed =
      "<feed>"
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

void test_decodes_entities_and_strips_html() {
  // stripHtml runs before entity decoding, so real markup tags become spaces
  // and named entities decode to their character.
  const String feed =
      "<rss><channel><item>"
      "<title>Tom &amp; Jerry</title>"
      "<link>https://example.com/x</link>"
      "<description><p>Hello <b>there</b></p></description>"
      "</item></channel></rss>";
  const feedparser::FeedItem item = firstItem(feed);
  TEST_ASSERT_EQUAL_STRING("Tom & Jerry", item.title.c_str());
  TEST_ASSERT_EQUAL_STRING("Hello there", item.body.c_str());
}

void test_unwraps_cdata_in_body() {
  const String feed =
      "<rss><channel><item>"
      "<title>T</title>"
      "<link>https://example.com/y</link>"
      "<description><![CDATA[Plain cdata text]]></description>"
      "</item></channel></rss>";
  const feedparser::FeedItem item = firstItem(feed);
  TEST_ASSERT_EQUAL_STRING("Plain cdata text", item.body.c_str());
}

void test_falls_back_to_host_for_author() {
  const String feed =
      "<rss><channel><item>"
      "<title>No Author</title>"
      "<link>https://www.example.com/post</link>"
      "<description>Body.</description>"
      "</item></channel></rss>";
  const feedparser::FeedItem item = firstItem(feed);
  TEST_ASSERT_EQUAL_STRING("example.com", item.author.c_str());
}

void test_prefers_dc_creator_author() {
  const String feed =
      "<rss><channel><item>"
      "<title>T</title>"
      "<link>https://example.com/z</link>"
      "<dc:creator>Jane Doe</dc:creator>"
      "<description>Body.</description>"
      "</item></channel></rss>";
  const feedparser::FeedItem item = firstItem(feed);
  TEST_ASSERT_EQUAL_STRING("Jane Doe", item.author.c_str());
}

void test_iterates_multiple_items_then_stops() {
  const String feed =
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
  const String feed =
      "<rss><channel>"
      "<item><title>One</title><link>https://e.com/1</link><description>b1</description></item>";

  feedparser::FeedItem item;
  size_t cursor = 0;
  TEST_ASSERT_TRUE(feedparser::parseNextItem(feed, cursor, item));
  TEST_ASSERT_EQUAL_STRING("One", item.title.c_str());
  TEST_ASSERT_EQUAL_STRING("b1", item.body.c_str());
}

void test_preserves_long_full_text_content() {
  String longBody;
  longBody.reserve(140000);
  for (int i = 0; i < 3000; ++i) {
    longBody += "Long article paragraph with readable text. ";
  }
  longBody += "tail-marker";

  const String feed =
      "<rss><channel><item>"
      "<title>Long Read</title>"
      "<link>https://example.com/long</link>"
      "<content:encoded><![CDATA[" +
      longBody +
      "]]></content:encoded>"
      "</item></channel></rss>";

  const feedparser::FeedItem item = firstItem(feed);
  TEST_ASSERT_TRUE(item.body.length() > 64000);
  TEST_ASSERT_TRUE(item.body.endsWith("tail-marker"));
}

void test_host_label_strips_scheme_and_www() {
  TEST_ASSERT_EQUAL_STRING("example.com",
                           feedparser::hostLabelForUrl("https://www.example.com/path?x=1").c_str());
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_parses_rss_item_fields);
  RUN_TEST(test_parses_atom_entry_with_href_link);
  RUN_TEST(test_decodes_entities_and_strips_html);
  RUN_TEST(test_unwraps_cdata_in_body);
  RUN_TEST(test_falls_back_to_host_for_author);
  RUN_TEST(test_prefers_dc_creator_author);
  RUN_TEST(test_iterates_multiple_items_then_stops);
  RUN_TEST(test_parses_complete_item_from_partial_feed);
  RUN_TEST(test_preserves_long_full_text_content);
  RUN_TEST(test_host_label_strips_scheme_and_www);
  return UNITY_END();
}
