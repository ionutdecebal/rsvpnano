// Host unit test for the pure Calibre ajax JSON parsers in
// src/calibre/CalibreClient.cpp (namespace calibreparser). Mirrors
// test/test_release_parser/test_main.cpp in spirit, but is built standalone by
// run_host_test.sh with plain g++ -std=c++17 against the existing
// test/support/Arduino.h String shim. No ArduinoJson is required because this
// repo hand-rolls String-based JSON parsing (see the header note in
// CalibreClient.h); a minimal assert harness stands in for Unity so the script
// needs no PlatformIO toolchain.
//
// If tools/calibre-sync/fixtures/*.json exist, they are loaded and asserted
// against; otherwise inline literals matching the known ajax shapes are used.
// Either way the same invariants are checked.

#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "calibre/CalibreClient.h"

static int g_failures = 0;
static int g_checks = 0;

#define CHECK(cond)                                                        \
  do {                                                                     \
    ++g_checks;                                                            \
    if (!(cond)) {                                                         \
      ++g_failures;                                                        \
      std::printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);        \
    }                                                                      \
  } while (0)

#define CHECK_STR_EQ(expected, actual)                                            \
  do {                                                                            \
    ++g_checks;                                                                   \
    const std::string e = (expected);                                            \
    const std::string a = (actual);                                               \
    if (e != a) {                                                                  \
      ++g_failures;                                                                \
      std::printf("  FAIL %s:%d: expected \"%s\" got \"%s\"\n", __FILE__,         \
                  __LINE__, e.c_str(), a.c_str());                                \
    }                                                                             \
  } while (0)

namespace {

// Returns fixture text, or "" when the fixture is absent (inline literals used).
std::string loadFixture(const std::string &name) {
  const std::string path = std::string("tools/calibre-sync/fixtures/") + name;
  std::ifstream in(path);
  if (!in.good()) {
    return std::string();
  }
  std::ostringstream ss;
  ss << in.rdbuf();
  return ss.str();
}

const char *kLibraryInfoInline =
    "{\"default_library\":\"rsvplib\","
    "\"library_map\":{\"rsvplib\":\"rsvplib\"}}";

const char *kSearchInline =
    "{\"book_ids\":[1,2,42,1337],\"num\":4,\"total_num\":4,\"offset\":0}";

// Lowercase format keys, and format_metadata holds BOTH epub and rsvp blocks
// (epub first) -- the parser must pick rsvp's own size/mtime, not epub's.
const char *kBookWithRsvpInline =
    "{\"title\":\"Some Book\",\"last_modified\":\"2024-01-02T03:04:05+00:00\","
    "\"main_format\":{\"epub\":\"/get/epub/42/lib\"},"
    "\"format_metadata\":{"
    "\"epub\":{\"size\":20945,\"mtime\":\"2024-01-01T00:00:00.111+00:00\"},"
    "\"rsvp\":{\"size\":123456,\"mtime\":\"2024-05-06T07:08:09.528479+00:00\"}},"
    "\"other_formats\":{\"rsvp\":\"/get/rsvp/42/lib\"}}";

// No rsvp format: other_formats.rsvp key is ABSENT (not null).
const char *kBookNoRsvpInline =
    "{\"title\":\"No RSVP Book\",\"last_modified\":\"2024-01-02T03:04:05+00:00\","
    "\"format_metadata\":{\"epub\":{\"size\":999,\"mtime\":\"2024-01-01T00:00:00+00:00\"}},"
    "\"other_formats\":{\"epub\":\"/get/epub/7/lib\"}}";

void test_library_info() {
  std::printf("test_library_info\n");
  std::string fixture = loadFixture("library-info.json");
  const String json = fixture.empty() ? String(kLibraryInfoInline) : String(fixture.c_str());

  calibreparser::LibraryInfo info;
  CHECK(calibreparser::parseLibraryInfo(json, info));
  // Live fixture default_library is "rsvplib"; inline literal uses it too.
  const char *expected = "rsvplib";  // both fixture and inline literal
  CHECK_STR_EQ(expected, info.defaultLibrary.c_str());
}

void test_library_info_missing() {
  std::printf("test_library_info_missing\n");
  const String json = "{\"library_map\":{}}";
  calibreparser::LibraryInfo info;
  CHECK(!calibreparser::parseLibraryInfo(json, info));
  CHECK(info.defaultLibrary.isEmpty());
}

void test_search_book_ids() {
  std::printf("test_search_book_ids\n");
  std::string fixture = loadFixture("search.json");
  const String json = fixture.empty() ? String(kSearchInline) : String(fixture.c_str());

  std::vector<int> ids;
  CHECK(calibreparser::parseSearchBookIds(json, ids));
  if (fixture.empty()) {
    CHECK(ids.size() == 4);
    if (ids.size() == 4) {
      CHECK(ids[0] == 1);
      CHECK(ids[1] == 2);
      CHECK(ids[2] == 42);
      CHECK(ids[3] == 1337);
    }
  } else {
    // Live fixture search.json: book_ids == [1].
    CHECK(ids.size() == 1);
    if (!ids.empty()) {
      CHECK(ids[0] == 1);
    }
  }
}

void test_search_empty_array() {
  std::printf("test_search_empty_array\n");
  const String json = "{\"book_ids\":[],\"num\":0,\"total_num\":0,\"offset\":0}";
  std::vector<int> ids;
  CHECK(calibreparser::parseSearchBookIds(json, ids));  // key present => true
  CHECK(ids.empty());
}

void test_search_missing_key() {
  std::printf("test_search_missing_key\n");
  const String json = "{\"num\":0}";
  std::vector<int> ids;
  CHECK(!calibreparser::parseSearchBookIds(json, ids));
  CHECK(ids.empty());
}

void test_book_rsvp_ref() {
  std::printf("test_book_rsvp_ref\n");
  std::string fixture = loadFixture("book-1.json");
  const String json = fixture.empty() ? String(kBookWithRsvpInline) : String(fixture.c_str());

  calibreparser::RsvpRef ref;
  CHECK(calibreparser::parseBookRsvpRef(json, ref));
  if (fixture.empty()) {
    CHECK_STR_EQ("/get/rsvp/42/lib", ref.url.c_str());
    CHECK(ref.size == 123456);
    // rsvp's own mtime (not epub's), with fractional seconds preserved.
    CHECK_STR_EQ("2024-05-06T07:08:09.528479+00:00", ref.lastModified.c_str());
  } else {
    // Live fixture book-1.json: confirmed values from the prototype.
    CHECK_STR_EQ("/get/rsvp/1/rsvplib", ref.url.c_str());
    CHECK(ref.size == 475);
    CHECK_STR_EQ("2026-06-17T15:37:34.528479+00:00", ref.lastModified.c_str());
  }
}

void test_book_rsvp_unescapes_slashes() {
  std::printf("test_book_rsvp_unescapes_slashes\n");
  const String json =
      "{\"other_formats\":{\"rsvp\":\"\\/get\\/rsvp\\/9\\/Lib\"},"
      "\"format_metadata\":{\"rsvp\":{\"size\":10,\"mtime\":\"t\"}}}";
  calibreparser::RsvpRef ref;
  CHECK(calibreparser::parseBookRsvpRef(json, ref));
  CHECK_STR_EQ("/get/rsvp/9/Lib", ref.url.c_str());
  CHECK(ref.size == 10);
}

void test_book_rsvp_falls_back_to_top_last_modified() {
  std::printf("test_book_rsvp_falls_back_to_top_last_modified\n");
  // RSVP format metadata present but without mtime -> use top-level last_modified.
  const String json =
      "{\"last_modified\":\"2020-12-31T00:00:00+00:00\","
      "\"other_formats\":{\"rsvp\":\"/get/rsvp/3/Lib\"},"
      "\"format_metadata\":{\"rsvp\":{\"size\":55}}}";
  calibreparser::RsvpRef ref;
  CHECK(calibreparser::parseBookRsvpRef(json, ref));
  CHECK(ref.size == 55);
  CHECK_STR_EQ("2020-12-31T00:00:00+00:00", ref.lastModified.c_str());
}

void test_book_no_rsvp_returns_not_found() {
  std::printf("test_book_no_rsvp_returns_not_found\n");
  std::string fixture = loadFixture("book_no_rsvp.json");
  const String json = fixture.empty() ? String(kBookNoRsvpInline) : String(fixture.c_str());

  calibreparser::RsvpRef ref;
  CHECK(!calibreparser::parseBookRsvpRef(json, ref));  // must not crash
  CHECK(ref.url.isEmpty());
  CHECK(ref.size == 0);
}

void test_book_no_formats_at_all() {
  std::printf("test_book_no_formats_at_all\n");
  const String json = "{\"title\":\"Bare\"}";
  calibreparser::RsvpRef ref;
  CHECK(!calibreparser::parseBookRsvpRef(json, ref));
  CHECK(ref.url.isEmpty());
}

void test_basic_auth_header() {
  std::printf("test_basic_auth_header\n");
  net::HttpAuth auth;
  CHECK(net::basicAuthHeaderValue(auth).isEmpty());  // no user => no header
  auth.username = "Aladdin";
  auth.password = "open sesame";
  // base64("Aladdin:open sesame") == "QWxhZGRpbjpvcGVuIHNlc2FtZQ=="
  CHECK_STR_EQ("Basic QWxhZGRpbjpvcGVuIHNlc2FtZQ==",
               net::basicAuthHeaderValue(auth).c_str());
}

}  // namespace

int main() {
  test_library_info();
  test_library_info_missing();
  test_search_book_ids();
  test_search_empty_array();
  test_search_missing_key();
  test_book_rsvp_ref();
  test_book_rsvp_unescapes_slashes();
  test_book_rsvp_falls_back_to_top_last_modified();
  test_book_no_rsvp_returns_not_found();
  test_book_no_formats_at_all();
  test_basic_auth_header();

  std::printf("\n%d checks, %d failures\n", g_checks, g_failures);
  if (g_failures == 0) {
    std::printf("ALL TESTS PASSED\n");
    return 0;
  }
  std::printf("TESTS FAILED\n");
  return 1;
}
