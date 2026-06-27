#include <unity.h>

#include "update/ReleaseParser.h"

namespace {

const char *kAsset = "rsvp-nano-esp32-s3-touch-lcd-3.49-ota.bin";

releaseparser::ReleaseInfo parseJson(const String &json) {
  releaseparser::ReleaseInfo out;
  releaseparser::parse(json, kAsset, out);
  return out;
}

}  // namespace

void setUp() {}

void tearDown() {}

void test_extracts_tag_and_matching_asset_url() {
  const String json =
      "{\"tag_name\":\"v0.0.6\",\"assets\":["
      "{\"name\":\"other.bin\",\"browser_download_url\":\"https://example.com/other.bin\"},"
      "{\"name\":\"rsvp-nano-esp32-s3-touch-lcd-3.49-ota.bin\",\"browser_download_url\":\"https://example.com/ota.bin\"}"
      "]}";
  releaseparser::ReleaseInfo out;
  TEST_ASSERT_TRUE(releaseparser::parse(json, kAsset, out));
  TEST_ASSERT_EQUAL_STRING("v0.0.6", out.tagName.c_str());
  TEST_ASSERT_EQUAL_STRING("https://example.com/ota.bin", out.assetUrl.c_str());
}

void test_returns_false_when_tag_missing() {
  const String json = "{\"assets\":[]}";
  releaseparser::ReleaseInfo out;
  TEST_ASSERT_FALSE(releaseparser::parse(json, kAsset, out));
  TEST_ASSERT_TRUE(out.tagName.isEmpty());
}

void test_asset_url_empty_when_no_asset_matches() {
  const String json =
      "{\"tag_name\":\"v1.2.3\",\"assets\":["
      "{\"name\":\"wrong.bin\",\"browser_download_url\":\"https://example.com/wrong.bin\"}]}";
  const releaseparser::ReleaseInfo out = parseJson(json);
  TEST_ASSERT_EQUAL_STRING("v1.2.3", out.tagName.c_str());
  TEST_ASSERT_TRUE(out.assetUrl.isEmpty());
}

void test_handles_whitespace_after_colon() {
  const String json = "{ \"tag_name\" :   \"v9\" }";
  const releaseparser::ReleaseInfo out = parseJson(json);
  TEST_ASSERT_EQUAL_STRING("v9", out.tagName.c_str());
}

void test_unescapes_forward_slashes_in_url() {
  const String json =
      "{\"tag_name\":\"v2\",\"assets\":["
      "{\"name\":\"rsvp-nano-esp32-s3-touch-lcd-3.49-ota.bin\","
      "\"browser_download_url\":\"https:\\/\\/example.com\\/path\\/ota.bin\"}]}";
  const releaseparser::ReleaseInfo out = parseJson(json);
  TEST_ASSERT_EQUAL_STRING("https://example.com/path/ota.bin", out.assetUrl.c_str());
}

void test_picks_url_after_matching_name_not_a_neighbor() {
  // The matching asset is the second entry; its url must come from after its name.
  const String json =
      "{\"tag_name\":\"v3\",\"assets\":["
      "{\"name\":\"a.bin\",\"browser_download_url\":\"https://example.com/a.bin\"},"
      "{\"name\":\"rsvp-nano-esp32-s3-touch-lcd-3.49-ota.bin\",\"browser_download_url\":\"https://example.com/correct.bin\"}"
      "]}";
  const releaseparser::ReleaseInfo out = parseJson(json);
  TEST_ASSERT_EQUAL_STRING("https://example.com/correct.bin", out.assetUrl.c_str());
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_extracts_tag_and_matching_asset_url);
  RUN_TEST(test_returns_false_when_tag_missing);
  RUN_TEST(test_asset_url_empty_when_no_asset_matches);
  RUN_TEST(test_handles_whitespace_after_colon);
  RUN_TEST(test_unescapes_forward_slashes_in_url);
  RUN_TEST(test_picks_url_after_matching_name_not_a_neighbor);
  return UNITY_END();
}
