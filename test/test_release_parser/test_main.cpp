#include <unity.h>

#include "update/ReleaseParser.h"

void setUp() {}

void tearDown() {}

void test_extracts_tag_from_asset_redirect() {
    constexpr std::string_view asset = "rsvp-nano-esp32-s3-touch-lcd-3.49-ota.bin";
    const auto tag = releaseparser::tagFromAssetLocation(
        "https://github.com/ionutdecebal/rsvpnano/releases/download/v0.0.9/"
        "rsvp-nano-esp32-s3-touch-lcd-3.49-ota.bin",
        asset);
    TEST_ASSERT_TRUE(tag.has_value());
    TEST_ASSERT_EQUAL_STRING("v0.0.9", tag->c_str());
}

void test_rejects_invalid_asset_redirects() {
    TEST_ASSERT_FALSE(releaseparser::tagFromAssetLocation("https://github.com/releases/latest", "firmware.bin")
                          .has_value());
    TEST_ASSERT_FALSE(releaseparser::tagFromAssetLocation("https://github.com/releases/download/v1/other.bin",
                                                          "firmware.bin")
                          .has_value());
}

void test_builds_version_from_release_tag_and_commit() {
    const auto version =
        releaseparser::versionForCommit("preview-v0.0.9", "0123456789abcdef0123456789abcdef01234567\n");
    TEST_ASSERT_TRUE(version.has_value());
    TEST_ASSERT_EQUAL_STRING("preview-v0.0.9+0123456789ab", version->c_str());
    TEST_ASSERT_FALSE(releaseparser::versionForCommit("v1", "not-a-commit").has_value());
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_extracts_tag_from_asset_redirect);
    RUN_TEST(test_rejects_invalid_asset_redirects);
    RUN_TEST(test_builds_version_from_release_tag_and_commit);
    return UNITY_END();
}
