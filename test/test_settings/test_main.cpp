#include <unity.h>

#include <glaze/toml.hpp>

#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "companion/CompanionApiModels.h"
#include "drivers/power/BatteryCurve.h"
#include "reader/ReadingSession.h"
#include "settings/SettingsCodec.h"
#include "settings/SettingsGlaze.h"
#include "library/BookLibrary.h"
#include "text/UnicodeText.h"

void setUp() {}
void tearDown() {}

void test_defaults_round_trip_through_toml_and_companion_json() {
    const settings::DeviceSettings defaults;
    auto toml = settings::codec::encodeToml(defaults, settings::SettingsSource::Programmatic);
    TEST_ASSERT_TRUE_MESSAGE(toml.has_value(), toml ? "" : toml.error().message.c_str());
    TEST_ASSERT_EQUAL(std::string::npos, toml->find("schemaVersion"));
    TEST_ASSERT_NOT_EQUAL(std::string::npos, toml->find("batteryIconVisible = true"));
    TEST_ASSERT_NOT_EQUAL(std::string::npos, toml->find("checkOnStartup = false"));
    auto fromToml = settings::codec::decodeToml(*toml, settings::SettingsSource::Sd);
    TEST_ASSERT_TRUE_MESSAGE(fromToml.has_value(), fromToml ? "" : fromToml.error().message.c_str());
    TEST_ASSERT_TRUE(defaults == *fromToml);

    std::string json;
    TEST_ASSERT_TRUE(companion::api::encode(defaults, json).has_value());
    auto fromJson = companion::api::decode<settings::DeviceSettings>(json);
    TEST_ASSERT_TRUE_MESSAGE(fromJson.has_value(), fromJson ? "" : fromJson.error().c_str());
    TEST_ASSERT_TRUE(defaults == *fromJson);
}

void test_battery_curve_reaches_full() {
    TEST_ASSERT_EQUAL_UINT8(100, BoardDrivers::BatteryCurve::percentForVoltage(4.15f));
}

void test_enum_names_are_human_readable() {
    settings::DeviceSettings value;
    value.reading.mode = settings::ReadingMode::page;
    value.reading.pauseMode = settings::PauseMode::instant;
    value.interface.screensaver = standby::Kind::reaction;
    value.interface.libraryLayout = settings::LibraryLayout::list;
    value.reading.pauseAtChapterStart = true;
    auto toml = settings::codec::encodeToml(value, settings::SettingsSource::Programmatic);
    TEST_ASSERT_TRUE(toml.has_value());
    TEST_ASSERT_NOT_EQUAL(std::string::npos, toml->find("libraryLayout = \"list\""));
    TEST_ASSERT_NOT_EQUAL(std::string::npos, toml->find("pauseAtChapterStart = true"));
    TEST_ASSERT_NOT_EQUAL(std::string::npos, toml->find("mode = \"page\""));
    TEST_ASSERT_NOT_EQUAL(std::string::npos, toml->find("pauseMode = \"instant\""));
    TEST_ASSERT_NOT_EQUAL(std::string::npos, toml->find("screensaver = \"reaction\""));
}

void test_missing_fields_retain_defaults() {
    auto value = settings::codec::decodeToml("obsolete = true\n[reading]\nwpm = 300\n", settings::SettingsSource::Sd);
    TEST_ASSERT_TRUE_MESSAGE(value.has_value(), value ? "" : value.error().message.c_str());
    TEST_ASSERT_EQUAL_UINT16(300, value->reading.wpm);
    TEST_ASSERT_TRUE(value->reading.batteryIconVisible);
    TEST_ASSERT_FALSE(value->reading.pauseAtChapterStart);
    TEST_ASSERT_TRUE(value->interface.libraryLayout == settings::LibraryLayout::shelf);
    TEST_ASSERT_EQUAL_STRING("literata", value->reading.typography.fontId.c_str());
    auto canonical = settings::codec::encodeToml(*value, settings::SettingsSource::Programmatic);
    TEST_ASSERT_TRUE(canonical.has_value());
    TEST_ASSERT_EQUAL(std::string::npos, canonical->find("obsolete"));
    TEST_ASSERT_NOT_EQUAL(std::string::npos, canonical->find("batteryIconVisible = true"));
}

void test_bounded_values_clamp_during_deserialization() {
    auto value = settings::codec::decodeToml("[reading]\nwpm = 9\n", settings::SettingsSource::Sd);
    TEST_ASSERT_TRUE_MESSAGE(value.has_value(), value ? "" : value.error().message.c_str());
    TEST_ASSERT_EQUAL_UINT16(10, value->reading.wpm);
}

void test_bounded_values_clamp_on_every_assignment() {
    settings::BoundedValue<uint8_t, 5, 100, 5> brightness{-1};
    TEST_ASSERT_EQUAL_UINT8(5, brightness);
    brightness = 101;
    TEST_ASSERT_EQUAL_UINT8(100, brightness);
    brightness.cycle();
    TEST_ASSERT_EQUAL_UINT8(5, brightness);
}

void test_invalid_input_cannot_mutate_a_live_value() {
    settings::DeviceSettings live;
    live.reading.wpm = 450;
    auto candidate = settings::codec::decodeToml("[reading]\nwpm = 600\n[interface]\nbrightnessPercent = 101\n",
                                                 settings::SettingsSource::Sd);
    TEST_ASSERT_TRUE(candidate.has_value());
    TEST_ASSERT_EQUAL_UINT16(600, candidate->reading.wpm);
    TEST_ASSERT_EQUAL_UINT8(100, candidate->interface.brightnessPercent);
    TEST_ASSERT_EQUAL_UINT16(450, live.reading.wpm);
}

void test_unknown_keys_are_ignored_and_invalid_enums_still_fail() {
    auto unknown = companion::api::decode<settings::DeviceSettings>(R"({"surprise":true})");
    TEST_ASSERT_TRUE(unknown.has_value());
    TEST_ASSERT_TRUE(unknown->reading.batteryIconVisible);

    auto invalidEnum = companion::api::decode<settings::DeviceSettings>(R"({"reading":{"pauseMode":"later"}})");
    TEST_ASSERT_FALSE(invalidEnum.has_value());
}

void test_companion_patch_preserves_omitted_fields() {
    auto reading = settings::DeviceSettings{}.reading;
    reading.wpm = 540;
    constexpr std::string_view patch = R"({"leftHanded":true})";
    const auto error = glz::read<glz::opts{.error_on_unknown_keys = false}>(reading, patch);
    TEST_ASSERT_FALSE(error);
    TEST_ASSERT_EQUAL_UINT16(540, reading.wpm);
    TEST_ASSERT_TRUE(reading.leftHanded);
}

void test_companion_resource_encodes_directly() {
    const settings::NetworkSettings response{"Home"};
    const settings::NetworkSettings* resource = &response;
    std::string json;
    TEST_ASSERT_TRUE(companion::api::encode(resource, json).has_value());
    TEST_ASSERT_EQUAL_STRING("{\"ssid\":\"Home\"}", json.c_str());
}

void test_companion_theme_list_uses_ids_and_names() {
    const std::vector<ui::themes::Theme> response{
        {.id = "default", .definition = {.name = "Default"}},
        {.id = "night", .definition = {.name = "Night"}},
    };
    std::string json;
    TEST_ASSERT_TRUE(companion::api::encode(response, json).has_value());
    TEST_ASSERT_EQUAL_STRING(R"([{"id":"default","name":"Default"},{"id":"night","name":"Night"}])", json.c_str());
}

void test_companion_font_list_uses_ids_and_names() {
    const std::vector<FontCatalog::Family> response{
        {.id = "literata",
         .label = "Literata",
         .builtIn = true,
         .scriptMask = UnicodeText::ScriptLatin | UnicodeText::ScriptCyrillic},
        {.id = "hebrew", .label = "Noto Serif Hebrew", .locales = "he", .scriptMask = UnicodeText::ScriptHebrew},
    };
    std::string json;
    TEST_ASSERT_TRUE(companion::api::encode(response, json).has_value());
    TEST_ASSERT_EQUAL_STRING(
        R"([{"id":"literata","name":"Literata","locales":[],"scripts":["Latn","Cyrl"],"builtIn":true},{"id":"hebrew","name":"Noto Serif Hebrew","locales":["he"],"scripts":["Hebr"],"builtIn":false}])",
        json.c_str());
}

void test_companion_library_is_a_bare_array() {
    BookMetadata metadata;
    metadata.locale = "en";
    metadata.scriptMask = UnicodeText::ScriptLatin;
    const BookLibrary::Entry book{.title = "Example", .author = "Reader"};
    const reading::BookIdentity identity{.wordCount = 42};
    auto scripts = UnicodeText::scriptTags(metadata.scriptMask);
    auto languages = companion::api::bookLanguages(metadata);
    const auto bookMetadata =
        glz::obj{"title",    book.title,       "author",  book.author, "wordCount", identity.wordCount,
                 "locale",   metadata.locale,  "scripts", scripts,     "languages", languages,
                 "chapters", metadata.chapters};
    const reading::State* reading = nullptr;
    const auto response = glz::arr{glz::obj{"id", "book-id", "name", "books/example.rsvp", "bytes", 0, "metadata",
                                            bookMetadata, "reading", reading}};
    std::string json;
    TEST_ASSERT_TRUE(companion::api::encode(response, json).has_value());
    TEST_ASSERT_TRUE(json.starts_with(R"([{"id":)"));
    TEST_ASSERT_FALSE(json.contains(R"("items":)"));
    TEST_ASSERT_TRUE(json.contains(R"("languages":[{"locale":"en","scripts":["Latn"]}])"));
    TEST_ASSERT_TRUE(json.contains(R"("metadata":{"title":"Example","author":"Reader","wordCount":42)"));
    TEST_ASSERT_FALSE(json.contains("scriptMask"));
}

void test_companion_appearance_selection_decodes_one_id() {
    auto selection = companion::api::decode<companion::api::AppearanceSelection>(R"({"id":"night"})");
    TEST_ASSERT_TRUE(selection.has_value());
    TEST_ASSERT_EQUAL_STRING("night", selection->id.c_str());
}

void test_companion_device_is_identity_only() {
    const companion::api::DeviceInfo response{
        .ssid = "RSVP-Nano-123456",
        .firmwareVersion = "preview-v0.0.9+abc",
        .otaAsset = "reader-ota.bin",
    };
    std::string json;
    TEST_ASSERT_TRUE(companion::api::encode(response, json).has_value());
    TEST_ASSERT_EQUAL_STRING(
        R"({"ssid":"RSVP-Nano-123456","firmwareVersion":"preview-v0.0.9+abc","otaAsset":"reader-ota.bin"})",
        json.c_str());
}

void test_companion_catalog_creations_return_one_resource() {
    std::string json;
    TEST_ASSERT_TRUE(companion::api::encode(ui::themes::Theme{.id = "night", .definition = {.name = "Night"}}, json)
                         .has_value());
    TEST_ASSERT_TRUE(json.contains(R"("id":"night")"));
    TEST_ASSERT_FALSE(json.contains(R"("fonts":)"));
    TEST_ASSERT_FALSE(json.contains(R"("settings":)"));

    TEST_ASSERT_TRUE(companion::api::encode(FontCatalog::Family{.id = "andika", .label = "Andika"}, json).has_value());
    TEST_ASSERT_TRUE(json.contains(R"("id":"andika")"));
    TEST_ASSERT_FALSE(json.contains(R"("themes":)"));
    TEST_ASSERT_FALSE(json.contains(R"("settings":)"));

    TEST_ASSERT_TRUE(companion::api::encode(locales::InstalledPack{.id = "ja", .locale = "ja"}, json)
                         .has_value());
    TEST_ASSERT_TRUE(json.contains(R"("id":"ja")"));
    TEST_ASSERT_FALSE(json.contains(R"("themes":)"));
    TEST_ASSERT_FALSE(json.contains(R"("fonts":)"));
}

void test_companion_locale_list_is_minimal() {
    const std::vector<locales::InstalledPack> response{
        {.id = "ja", .locale = "ja", .nativeName = "日本語"},
    };
    std::string json;
    TEST_ASSERT_TRUE(companion::api::encode(response, json).has_value());
    TEST_ASSERT_EQUAL_STRING(R"([{"id":"ja","name":"日本語","locale":"ja"}])", json.c_str());
}

void test_secrets_are_not_part_of_public_documents() {
    settings::DeviceSettings publicSettings;
    publicSettings.network.ssid = "reader";
    auto toml = settings::codec::encodeToml(publicSettings, settings::SettingsSource::Programmatic);
    std::string json;
    const auto encoded = companion::api::encode(publicSettings, json);
    TEST_ASSERT_TRUE(toml.has_value());
    TEST_ASSERT_TRUE(encoded.has_value());
    TEST_ASSERT_EQUAL(std::string::npos, toml->find("wifiPassword"));
    TEST_ASSERT_EQUAL(std::string::npos, json.find("wifiPassword"));
}

void test_book_language_font_selection_uses_global_fallback() {
    settings::ReadingOverrides book;
    TEST_ASSERT_EQUAL_STRING("global-font",
                             settings::fontForText(book, "ja", UnicodeText::ScriptHan, "global-font").data());
    book.languageFonts.push_back({.locale = "ja", .fontId = "japanese-font"});
    TEST_ASSERT_EQUAL_STRING("japanese-font",
                             settings::fontForText(book, "ja", UnicodeText::ScriptHan, "global-font").data());
    TEST_ASSERT_EQUAL_STRING("global-font",
                             settings::fontForText(book, "en", UnicodeText::ScriptLatin, "global-font").data());
    book.languageFonts.push_back({.locale = std::string{settings::kMathFontTarget}, .fontId = "math-font"});
    TEST_ASSERT_EQUAL_STRING("math-font",
                             settings::fontForText(book, "en", UnicodeText::ScriptLatin | UnicodeText::ScriptMath,
                                                   "global-font")
                                 .data());
}

void test_book_locale_follows_text_run_boundaries() {
    BookMetadata metadata;
    metadata.locale = "en";
    metadata.textRuns = {{0, "en"}, {4, "ja"}, {7, "en"}};
    TEST_ASSERT_EQUAL_STRING("en", metadata.localeAt(3).data());
    TEST_ASSERT_EQUAL_STRING("ja", metadata.localeAt(4).data());
    TEST_ASSERT_EQUAL_STRING("ja", metadata.localeAt(6).data());
    TEST_ASSERT_EQUAL_STRING("en", metadata.localeAt(7).data());
}

void test_book_reading_overrides_round_trip_through_toml() {
    reading::StoredState state;
    state.reading.wordIndex = 42;
    state.reading.overrides.languageFonts.push_back({.locale = "ar", .fontId = "arabic-font"});
    state.reading.overrides.languageFonts.push_back({.locale = std::string{settings::kMathFontTarget},
                                                     .fontId = "math-font"});
    state.reading.overrides.locale = "ar";
    state.reading.overrides.pacing = settings::ReadingPacing::cjkPhrase;

    std::string toml;
    TEST_ASSERT_FALSE(glz::write_toml(state, toml));
    TEST_ASSERT_TRUE(toml.contains("pacing = \"cjk-phrase\""));
    reading::StoredState decoded;
    TEST_ASSERT_FALSE(glz::read_toml(decoded, toml));
    TEST_ASSERT_EQUAL_UINT32(42, decoded.reading.wordIndex);
    TEST_ASSERT_EQUAL_UINT32(2, decoded.reading.overrides.languageFonts.size());
    TEST_ASSERT_EQUAL_STRING("ar", decoded.reading.overrides.languageFonts.front().locale.c_str());
    TEST_ASSERT_EQUAL_STRING("arabic-font", decoded.reading.overrides.languageFonts.front().fontId.c_str());
    TEST_ASSERT_EQUAL_STRING("math", decoded.reading.overrides.languageFonts.back().locale.c_str());
    TEST_ASSERT_EQUAL_STRING("math-font", decoded.reading.overrides.languageFonts.back().fontId.c_str());
    TEST_ASSERT_EQUAL_STRING("ar", decoded.reading.overrides.locale->c_str());
    TEST_ASSERT_EQUAL(settings::ReadingPacing::cjkPhrase, *decoded.reading.overrides.pacing);
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_defaults_round_trip_through_toml_and_companion_json);
    RUN_TEST(test_battery_curve_reaches_full);
    RUN_TEST(test_enum_names_are_human_readable);
    RUN_TEST(test_missing_fields_retain_defaults);
    RUN_TEST(test_companion_patch_preserves_omitted_fields);
    RUN_TEST(test_bounded_values_clamp_during_deserialization);
    RUN_TEST(test_bounded_values_clamp_on_every_assignment);
    RUN_TEST(test_invalid_input_cannot_mutate_a_live_value);
    RUN_TEST(test_unknown_keys_are_ignored_and_invalid_enums_still_fail);
    RUN_TEST(test_companion_resource_encodes_directly);
    RUN_TEST(test_companion_library_is_a_bare_array);
    RUN_TEST(test_companion_appearance_selection_decodes_one_id);
    RUN_TEST(test_companion_device_is_identity_only);
    RUN_TEST(test_companion_catalog_creations_return_one_resource);
    RUN_TEST(test_companion_theme_list_uses_ids_and_names);
    RUN_TEST(test_companion_font_list_uses_ids_and_names);
    RUN_TEST(test_companion_locale_list_is_minimal);
    RUN_TEST(test_secrets_are_not_part_of_public_documents);
    RUN_TEST(test_book_language_font_selection_uses_global_fallback);
    RUN_TEST(test_book_locale_follows_text_run_boundaries);
    RUN_TEST(test_book_reading_overrides_round_trip_through_toml);
    return UNITY_END();
}
