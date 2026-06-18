#include "calibre/CalibreSettings.h"

// NVS persistence for CalibreSettings.
//
// Access pattern mirrored from src/sync/CompanionSyncManager.cpp:
//   preferences.begin(kPrefsNamespace, /*readOnly=*/false)
//   preferences.getString / getBool / getUChar   (reads)
//   preferences.putString / putBool / putUChar   (writes)
//   preferences.end()
//
// The NVS namespace ("rsvp") and key constants (kPrefCal*) are defined in
// src/settings/PreferenceKeys.h and shared with the rest of the firmware so
// keys cannot silently drift.

// Guard: Preferences is an Arduino/ESP-IDF header; the host unit-test build
// (which tests CalibreClient parsers without the embedded toolchain) must not
// pull it in. Mirrors the #if guard in src/calibre/CalibreClient.cpp.
#if defined(ARDUINO) || defined(ESP32)

#include <Preferences.h>

#include "settings/PreferenceKeys.h"

using namespace settings;

bool loadCalibreSettings(CalibreSettings &settings) {
  Preferences preferences;
  if (!preferences.begin(kPrefsNamespace, /*readOnly=*/false)) {
    return false;
  }

  settings.enabled       = preferences.getBool(kPrefCalEnabled, false);
  settings.baseUrl       = preferences.getString(kPrefCalUrl, "");
  settings.libraryId     = preferences.getString(kPrefCalLibrary, "");
  settings.searchQuery   = preferences.getString(kPrefCalQuery, "");
  settings.username      = preferences.getString(kPrefCalUser, "");
  settings.password      = preferences.getString(kPrefCalPass, "");

  const uint8_t delPol = preferences.getUChar(kPrefCalDelPol, 0);
  settings.deletionPolicy =
      (delPol == 1) ? CalibreSettings::Keep : CalibreSettings::Mirror;

  preferences.end();
  return true;
}

bool saveCalibreSettings(const CalibreSettings &settings) {
  Preferences preferences;
  if (!preferences.begin(kPrefsNamespace, /*readOnly=*/false)) {
    return false;
  }

  preferences.putBool(kPrefCalEnabled,  settings.enabled);
  preferences.putString(kPrefCalUrl,    normalizeCalibreBaseUrl(settings.baseUrl));
  preferences.putString(kPrefCalLibrary, settings.libraryId);
  preferences.putString(kPrefCalQuery,  settings.searchQuery);
  // Credentials stored in NVS only -- never written to SD.
  // Plain HTTP Basic is cleartext on the LAN; acceptable per design.
  preferences.putString(kPrefCalUser,   settings.username);
  preferences.putString(kPrefCalPass,   settings.password);
  preferences.putUChar(kPrefCalDelPol,
                       settings.deletionPolicy == CalibreSettings::Keep ? 1 : 0);

  preferences.end();
  return true;
}

#endif  // defined(ARDUINO) || defined(ESP32)
