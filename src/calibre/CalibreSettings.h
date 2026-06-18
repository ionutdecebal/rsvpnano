#pragma once

// Persisted sync configuration.
// This header defines the shared CalibreSettings contract consumed by the sync
// engine. load()/save() live in CalibreSettings.cpp and use the
// same NVS/Preferences access pattern as the rest of the firmware (namespace
// "rsvp", opened read-write with preferences.begin(kPrefsNamespace, false)).
// Preference key constants are in src/settings/PreferenceKeys.h (kPrefCal*).

#include <Arduino.h>

struct CalibreSettings {
  bool enabled = false;
  String baseUrl;      // e.g. http://192.168.0.120:8099  (no trailing slash)
  String libraryId;   // empty => use server default_library
  String searchQuery; // saved search / tag, e.g. "tag:rsvp"
  String username;    // optional HTTP Basic
  String password;    // optional HTTP Basic
  // NOTE: username/password are stored in NVS only; never written to SD.
  // HTTP Basic over plain HTTP is cleartext on the LAN -- acceptable per design
  // because calibre-server is typically a local, trusted network service.
  enum DeletionPolicy { Mirror, Keep };
  DeletionPolicy deletionPolicy = Mirror;
};

// Strip any trailing slashes from a Calibre base URL so stored values and
// constructed URLs are consistent regardless of what the user typed.
// Returns the normalised URL (may be the same string if already clean).
inline String normalizeCalibreBaseUrl(const String &url) {
  String result = url;
  while (result.length() > 0 && result[result.length() - 1] == '/') {
    result.remove(result.length() - 1);
  }
  return result;
}

// Populate settings from NVS. The caller must NOT have an open Preferences
// handle on kPrefsNamespace -- this function opens and closes it internally,
// mirroring the pattern in CompanionSyncManager::begin()/end().
// Returns true when the NVS namespace was opened successfully.
bool loadCalibreSettings(CalibreSettings &settings);

// Persist settings to NVS. Same open/close ownership rule as loadCalibreSettings.
// baseUrl is normalised (trailing slash stripped) before writing.
// Returns true when the NVS namespace was opened and all keys were written.
bool saveCalibreSettings(const CalibreSettings &settings);
