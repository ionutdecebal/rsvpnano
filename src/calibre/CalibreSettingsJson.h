#pragma once

// Pure, host-testable serialize + parse helpers for the /api/calibre-settings
// HTTP routes added in CompanionSyncManager.
//
// These free functions are intentionally free of any Arduino-only or ESP32-only
// includes so that test/calibre/test_calibre_settings_json.cpp can exercise
// them with plain g++ -std=c++17 against the test/support/Arduino.h shim.
// CompanionSyncManager.cpp includes this header and calls them directly.
//
// JSON contract (must match companion NanoCalibreSettings):
//   { "enabled": bool,
//     "baseUrl": str,
//     "searchQuery": str,
//     "username": str,
//     "password": str,       <- GET returns "" (masked); PUT preserves if blank
//     "libraryId": str,
//     "deletionPolicy": "mirror" | "keep" }
//
// Security: on serialize (GET), the stored password is NEVER returned to the
// caller; the password field is always emitted as "". This prevents credentials
// leaking over the plain-HTTP LAN channel on every settings round-trip.
// On parse (PUT), if the incoming password field is absent or empty AND a
// stored password already exists, the caller should keep the stored password
// unchanged. The sentinel value used by the companion to signal "no change" is
// also "" (empty string). The boolean `passwordProvided` out-param tells the
// caller whether a non-empty password was present in the body.

#include <Arduino.h>

#include "calibre/CalibreSettings.h"

// ---------------------------------------------------------------------------
// calibresettingsjson -- pure serialization/parse helpers (no NVS, no SD)
// ---------------------------------------------------------------------------
namespace calibresettingsjson {

// Serialize `settings` to a JSON string matching the companion contract.
// The password field is always emitted as "" regardless of `settings.password`
// -- see the security note above.
// `jsonEscape` must escape special characters for JSON string values
// (same contract as CompanionSyncManager::jsonEscape).
//
// Template parameter allows passing either a free function or a lambda so
// that the firmware can forward to its member jsonEscape, while host tests
// can supply a simple inline escape.
template <typename EscapeFn>
String serializeCalibreSettings(const CalibreSettings &settings,
                                EscapeFn jsonEscape) {
  const char *delPol =
      (settings.deletionPolicy == CalibreSettings::Keep) ? "keep" : "mirror";
  String body;
  body.reserve(256);
  body += "{\"ok\":true";
  body += ",\"enabled\":";
  body += settings.enabled ? "true" : "false";
  body += ",\"baseUrl\":\"";
  body += jsonEscape(settings.baseUrl);
  body += "\"";
  body += ",\"searchQuery\":\"";
  body += jsonEscape(settings.searchQuery);
  body += "\"";
  body += ",\"username\":\"";
  body += jsonEscape(settings.username);
  body += "\"";
  // Security: password is NEVER sent to the client. Always emit empty string.
  body += ",\"password\":\"\"";
  body += ",\"libraryId\":\"";
  body += jsonEscape(settings.libraryId);
  body += "\"";
  body += ",\"deletionPolicy\":\"";
  body += delPol;
  body += "\"";
  body += "}";
  return body;
}

// Parse a PUT /api/calibre-settings body into `out`. Fields missing from the
// body are left at their defaults (false / "" / Mirror). `passwordProvided` is
// set to true only when a non-empty password string appears in the body; the
// caller uses this to decide whether to overwrite the stored credential.
//
// Returns false + sets `error` only on a hard parse error (malformed JSON
// value for a recognised field). Missing optional fields are silently skipped.
//
// The `readJsonBool` / `readJsonString` function objects must match the
// signatures used in CompanionSyncManager's anonymous namespace.
template <typename ReadBoolFn, typename ReadStringFn>
bool parseCalibreSettingsJson(const String &body,
                              CalibreSettings &out,
                              bool &passwordProvided,
                              String &error,
                              ReadBoolFn readJsonBool,
                              ReadStringFn readJsonString) {
  passwordProvided = false;

  if (body.isEmpty()) {
    error = "Missing Calibre settings JSON";
    return false;
  }

  bool boolValue = false;
  String strValue;

  if (readJsonBool(body, "enabled", boolValue)) {
    out.enabled = boolValue;
  }
  if (readJsonString(body, "baseUrl", strValue)) {
    out.baseUrl = normalizeCalibreBaseUrl(strValue);
  }
  if (readJsonString(body, "searchQuery", strValue)) {
    out.searchQuery = strValue;
  }
  if (readJsonString(body, "username", strValue)) {
    out.username = strValue;
  }
  if (readJsonString(body, "password", strValue)) {
    if (!strValue.isEmpty()) {
      out.password = strValue;
      passwordProvided = true;
    }
    // If password is empty we leave out.password at its incoming value so the
    // caller can preserve the stored credential (see security note above).
  }
  if (readJsonString(body, "libraryId", strValue)) {
    out.libraryId = strValue;
  }
  if (readJsonString(body, "deletionPolicy", strValue)) {
    if (strValue == "mirror") {
      out.deletionPolicy = CalibreSettings::Mirror;
    } else if (strValue == "keep") {
      out.deletionPolicy = CalibreSettings::Keep;
    } else {
      error = "deletionPolicy must be mirror or keep";
      return false;
    }
  }

  return true;
}

}  // namespace calibresettingsjson
