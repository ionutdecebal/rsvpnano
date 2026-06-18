// Host unit test for the pure calibresettingsjson serialize + parse helpers in
// src/calibre/CalibreSettingsJson.h.
//
// Built standalone by test/calibre/run_host_test.sh with plain
// g++ -std=c++17 against test/support/Arduino.h (String shim).
// No ArduinoJson, no Preferences, no SD -- just the free functions.
//
// Coverage:
//   - round-trip: serialize then parse gives back the same non-password fields
//   - deletionPolicy "mirror" / "keep" mapping both directions
//   - password masking on serialize (GET always returns "")
//   - password preservation on parse when body password is absent or ""
//   - missing optional fields default sanely (enabled=false, empty strings, Mirror)

#include <cstdio>
#include <string>

#include "calibre/CalibreSettings.h"
#include "calibre/CalibreSettingsJson.h"

// ---------------------------------------------------------------------------
// Minimal assert harness (mirrors test_calibre_parse.cpp)
// ---------------------------------------------------------------------------
static int g_failures = 0;
static int g_checks   = 0;

#define CHECK(cond)                                                          \
  do {                                                                       \
    ++g_checks;                                                              \
    if (!(cond)) {                                                           \
      ++g_failures;                                                          \
      std::printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);          \
    }                                                                        \
  } while (0)

#define CHECK_STR_EQ(expected, actual)                                              \
  do {                                                                              \
    ++g_checks;                                                                     \
    const std::string e_ = (expected);                                              \
    const std::string a_ = (actual);                                                \
    if (e_ != a_) {                                                                 \
      ++g_failures;                                                                 \
      std::printf("  FAIL %s:%d: expected \"%s\" got \"%s\"\n",                     \
                  __FILE__, __LINE__, e_.c_str(), a_.c_str());                      \
    }                                                                               \
  } while (0)

// ---------------------------------------------------------------------------
// Minimal JSON helpers (mirrors the anonymous-namespace helpers in
// CompanionSyncManager.cpp so we can instantiate the same templates).
// Kept local to this TU to avoid pulling in the full CompanionSyncManager.
// ---------------------------------------------------------------------------
namespace {

// Simple JSON escape sufficient for test strings (no control chars needed).
String testJsonEscape(const String &v) {
  String out;
  for (size_t i = 0; i < v.length(); ++i) {
    char c = v[i];
    if (c == '"')       out += "\\\"";
    else if (c == '\\') out += "\\\\";
    else                out += c;
  }
  return out;
}

// Locate "key": in body, return colon index.
bool findKey(const String &body, const char *key, int &colonIdx) {
  const String needle = String("\"") + key + "\"";
  const int ki = body.indexOf(needle);
  if (ki < 0) return false;
  colonIdx = body.indexOf(':', ki + static_cast<int>(needle.length()));
  return colonIdx >= 0;
}

int skipWs(const String &body, int i) {
  while (i < static_cast<int>(body.length()) && (body[i] == ' ' || body[i] == '\t' ||
         body[i] == '\n' || body[i] == '\r'))
    ++i;
  return i;
}

bool readBool(const String &body, const char *key, bool &value) {
  int ci = -1;
  if (!findKey(body, key, ci)) return false;
  int i = skipWs(body, ci + 1);
  if (body.substring(i, i + 4) == "true")  { value = true;  return true; }
  if (body.substring(i, i + 5) == "false") { value = false; return true; }
  return false;
}

bool readStr(const String &body, const char *key, String &value) {
  int ci = -1;
  if (!findKey(body, key, ci)) return false;
  int i = skipWs(body, ci + 1);
  if (i >= static_cast<int>(body.length()) || body[i] != '"') return false;
  ++i;
  String result;
  while (i < static_cast<int>(body.length())) {
    char c = body[i++];
    if (c == '"') { value = result; return true; }
    if (c == '\\' && i < static_cast<int>(body.length())) {
      char esc = body[i++];
      if (esc == '"' || esc == '\\' || esc == '/') result += esc;
      else if (esc == 'n') result += '\n';
      else if (esc == 't') result += '\t';
      else result += esc;
    } else {
      result += c;
    }
  }
  return false;
}

// Wrappers matching the template signatures in CalibreSettingsJson.h.
auto boolFn = [](const String &b, const char *k, bool &v)   { return readBool(b, k, v); };
auto strFn  = [](const String &b, const char *k, String &v) { return readStr(b, k, v);  };

// Convenience: parse body into a fresh CalibreSettings (stored password = "").
bool parse(const String &body, CalibreSettings &out, bool &pwdProvided, String &err) {
  return calibresettingsjson::parseCalibreSettingsJson(
      body, out, pwdProvided, err, boolFn, strFn);
}

String serialize(const CalibreSettings &s) {
  return calibresettingsjson::serializeCalibreSettings(s, testJsonEscape);
}

}  // namespace

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------
namespace {

// Serialize a populated struct; verify every field appears correctly.
void test_serialize_fields() {
  std::printf("test_serialize_fields\n");
  CalibreSettings s;
  s.enabled       = true;
  s.baseUrl       = "http://192.168.0.10:8099";
  s.searchQuery   = "tag:rsvp";
  s.username      = "alice";
  s.password      = "s3cr3t";  // must NOT appear in output
  s.libraryId     = "mylib";
  s.deletionPolicy = CalibreSettings::Keep;

  const String json = serialize(s);
  // ok:true
  CHECK(json.indexOf("\"ok\":true") >= 0);
  // enabled true
  CHECK(json.indexOf("\"enabled\":true") >= 0);
  // baseUrl
  CHECK(json.indexOf("\"baseUrl\":\"http://192.168.0.10:8099\"") >= 0);
  // searchQuery
  CHECK(json.indexOf("\"searchQuery\":\"tag:rsvp\"") >= 0);
  // username
  CHECK(json.indexOf("\"username\":\"alice\"") >= 0);
  // password MASKED -- real value must not appear
  CHECK(json.indexOf("s3cr3t") < 0);
  CHECK(json.indexOf("\"password\":\"\"") >= 0);
  // libraryId
  CHECK(json.indexOf("\"libraryId\":\"mylib\"") >= 0);
  // deletionPolicy keep
  CHECK(json.indexOf("\"deletionPolicy\":\"keep\"") >= 0);
}

// Mirror policy round-trips as "mirror".
void test_serialize_mirror_policy() {
  std::printf("test_serialize_mirror_policy\n");
  CalibreSettings s;
  s.deletionPolicy = CalibreSettings::Mirror;
  const String json = serialize(s);
  CHECK(json.indexOf("\"deletionPolicy\":\"mirror\"") >= 0);
}

// Keep policy round-trips as "keep".
void test_serialize_keep_policy() {
  std::printf("test_serialize_keep_policy\n");
  CalibreSettings s;
  s.deletionPolicy = CalibreSettings::Keep;
  const String json = serialize(s);
  CHECK(json.indexOf("\"deletionPolicy\":\"keep\"") >= 0);
}

// GET always masks password regardless of what is stored.
void test_password_masked_on_serialize() {
  std::printf("test_password_masked_on_serialize\n");
  CalibreSettings s;
  s.password = "topsecret";
  const String json = serialize(s);
  // The literal password must never appear.
  CHECK(json.indexOf("topsecret") < 0);
  CHECK(json.indexOf("\"password\":\"\"") >= 0);
}

// Parse a well-formed body; verify all fields parsed correctly.
void test_parse_full_body() {
  std::printf("test_parse_full_body\n");
  const String body =
      "{\"enabled\":true,\"baseUrl\":\"http://10.0.0.1:8099\","
      "\"searchQuery\":\"tag:books\",\"username\":\"bob\","
      "\"password\":\"p@ss\",\"libraryId\":\"lib2\","
      "\"deletionPolicy\":\"keep\"}";

  CalibreSettings out;
  bool pwdProvided = false;
  String err;
  CHECK(parse(body, out, pwdProvided, err));
  CHECK(out.enabled == true);
  CHECK_STR_EQ("http://10.0.0.1:8099", out.baseUrl.c_str());
  CHECK_STR_EQ("tag:books",            out.searchQuery.c_str());
  CHECK_STR_EQ("bob",                  out.username.c_str());
  CHECK_STR_EQ("p@ss",                 out.password.c_str());
  CHECK_STR_EQ("lib2",                 out.libraryId.c_str());
  CHECK(out.deletionPolicy == CalibreSettings::Keep);
  CHECK(pwdProvided == true);
}

// Parse with deletionPolicy "mirror".
void test_parse_deletion_policy_mirror() {
  std::printf("test_parse_deletion_policy_mirror\n");
  const String body = "{\"deletionPolicy\":\"mirror\"}";
  CalibreSettings out;
  out.deletionPolicy = CalibreSettings::Keep;  // start with Keep; expect Mirror
  bool pwdProvided = false;
  String err;
  CHECK(parse(body, out, pwdProvided, err));
  CHECK(out.deletionPolicy == CalibreSettings::Mirror);
}

// Parse with deletionPolicy "keep".
void test_parse_deletion_policy_keep() {
  std::printf("test_parse_deletion_policy_keep\n");
  const String body = "{\"deletionPolicy\":\"keep\"}";
  CalibreSettings out;
  bool pwdProvided = false;
  String err;
  CHECK(parse(body, out, pwdProvided, err));
  CHECK(out.deletionPolicy == CalibreSettings::Keep);
}

// Invalid deletionPolicy returns false + error.
void test_parse_bad_deletion_policy() {
  std::printf("test_parse_bad_deletion_policy\n");
  const String body = "{\"deletionPolicy\":\"delete_all\"}";
  CalibreSettings out;
  bool pwdProvided = false;
  String err;
  CHECK(!parse(body, out, pwdProvided, err));
  CHECK(!err.isEmpty());
}

// Password absent in body -> passwordProvided=false; caller preserves stored pwd.
void test_password_preserve_when_absent() {
  std::printf("test_password_preserve_when_absent\n");
  const String body = "{\"enabled\":false,\"baseUrl\":\"http://x\"}";
  CalibreSettings out;
  out.password = "existing_secret";  // simulates stored credential
  bool pwdProvided = false;
  String err;
  CHECK(parse(body, out, pwdProvided, err));
  CHECK(pwdProvided == false);
  // out.password must be unchanged (caller responsibility, but parse preserves it)
  CHECK_STR_EQ("existing_secret", out.password.c_str());
}

// Password empty string in body -> passwordProvided=false; stored pwd preserved.
void test_password_preserve_when_empty_sentinel() {
  std::printf("test_password_preserve_when_empty_sentinel\n");
  const String body = "{\"password\":\"\"}";
  CalibreSettings out;
  out.password = "old_pass";
  bool pwdProvided = false;
  String err;
  CHECK(parse(body, out, pwdProvided, err));
  CHECK(pwdProvided == false);
  CHECK_STR_EQ("old_pass", out.password.c_str());
}

// Password non-empty -> passwordProvided=true; new value is written.
void test_password_updated_when_provided() {
  std::printf("test_password_updated_when_provided\n");
  const String body = "{\"password\":\"newpass\"}";
  CalibreSettings out;
  out.password = "old_pass";
  bool pwdProvided = false;
  String err;
  CHECK(parse(body, out, pwdProvided, err));
  CHECK(pwdProvided == true);
  CHECK_STR_EQ("newpass", out.password.c_str());
}

// Empty body -> parse returns false with an error message.
void test_parse_empty_body() {
  std::printf("test_parse_empty_body\n");
  CalibreSettings out;
  bool pwdProvided = false;
  String err;
  CHECK(!parse(String(""), out, pwdProvided, err));
  CHECK(!err.isEmpty());
}

// Missing optional fields -> defaults (enabled=false, "", Mirror).
void test_parse_missing_fields_default() {
  std::printf("test_parse_missing_fields_default\n");
  const String body = "{}";
  CalibreSettings out;
  bool pwdProvided = false;
  String err;
  CHECK(parse(body, out, pwdProvided, err));
  CHECK(out.enabled == false);
  CHECK(out.baseUrl.isEmpty());
  CHECK(out.searchQuery.isEmpty());
  CHECK(out.username.isEmpty());
  CHECK(out.password.isEmpty());
  CHECK(out.libraryId.isEmpty());
  CHECK(out.deletionPolicy == CalibreSettings::Mirror);
  CHECK(pwdProvided == false);
}

// Full round-trip: serialize -> parse -> verify non-password fields match.
void test_round_trip() {
  std::printf("test_round_trip\n");
  CalibreSettings original;
  original.enabled       = true;
  original.baseUrl       = "http://192.168.1.50:8080";
  original.searchQuery   = "tag:to-read";
  original.username      = "reader";
  original.password      = "secret123";
  original.libraryId     = "Calibre";
  original.deletionPolicy = CalibreSettings::Mirror;

  // GET: serialize (password masked).
  const String json = serialize(original);

  // PUT: parse the serialized JSON back.
  CalibreSettings parsed;
  bool pwdProvided = false;
  String err;
  CHECK(parse(json, parsed, pwdProvided, err));

  CHECK_STR_EQ(original.baseUrl.c_str(),       parsed.baseUrl.c_str());
  CHECK_STR_EQ(original.searchQuery.c_str(),   parsed.searchQuery.c_str());
  CHECK_STR_EQ(original.username.c_str(),      parsed.username.c_str());
  CHECK_STR_EQ(original.libraryId.c_str(),     parsed.libraryId.c_str());
  CHECK(parsed.enabled == original.enabled);
  CHECK(parsed.deletionPolicy == original.deletionPolicy);
  // Password was masked to "" in JSON -> not provided.
  CHECK(pwdProvided == false);
  CHECK(parsed.password.isEmpty());

  // baseUrl trailing slash is normalised away.
  CalibreSettings withSlash;
  withSlash.baseUrl = "http://192.168.1.50:8080/";  // trailing slash
  const String body2 = "{\"baseUrl\":\"http://host:9/\"}";
  CalibreSettings parsed2;
  bool pwd2 = false;
  String err2;
  CHECK(parse(body2, parsed2, pwd2, err2));
  CHECK_STR_EQ("http://host:9", parsed2.baseUrl.c_str());
}

}  // namespace

int main() {
  test_serialize_fields();
  test_serialize_mirror_policy();
  test_serialize_keep_policy();
  test_password_masked_on_serialize();
  test_parse_full_body();
  test_parse_deletion_policy_mirror();
  test_parse_deletion_policy_keep();
  test_parse_bad_deletion_policy();
  test_password_preserve_when_absent();
  test_password_preserve_when_empty_sentinel();
  test_password_updated_when_provided();
  test_parse_empty_body();
  test_parse_missing_fields_default();
  test_round_trip();

  std::printf("\n%d checks, %d failures\n", g_checks, g_failures);
  if (g_failures == 0) {
    std::printf("ALL TESTS PASSED\n");
    return 0;
  }
  std::printf("TESTS FAILED\n");
  return 1;
}
