#pragma once

// The on-device reconciling sync engine. Mirrors src/sync/CompanionSyncManager.
// {h,cpp} for structure, logging style ("[calibre-sync] ...") and hand-rolled
// String JSON (this repo does NOT use ArduinoJson -- see the note in
// src/calibre/CalibreClient.h).
//
// Flow (see runSync()):
//   read CalibreSettings
//     -> CalibreClient.search(query)
//     -> CalibreClient.resolveRsvp(id, ref)
//     -> calibresync::computeSyncPlan(...)          (pure core, host-tested)
//     -> net::get(url, sink) streaming to SD
//     -> rewrite /books/.calibre-sync.json manifest
//     -> StorageManager::refreshBooks() reindex
//
// The pure reconcile logic lives in src/sync/CalibreSyncPlan.h so it can be
// unit tested on the host (test/calibre/test_sync_plan.cpp).

#include <Arduino.h>

#include <functional>
#include <vector>

#include "calibre/CalibreClient.h"
#include "calibre/CalibreSettings.h"
#include "net/HttpFetch.h"
#include "sync/CalibreSyncPlan.h"

class StorageManager;

class CalibreSyncManager {
 public:
  // Progress callback, modelled on StorageManager::StatusCallback and the
  // status lines CompanionSyncManager surfaces. phase is a short tag
  // ("search", "download", "delete", "done", "error"); current/total drive a
  // percentage; detail is a human-readable line (e.g. a book title).
  using ProgressCallback = std::function<void(const String &phase, int current,
                                              int total, const String &detail)>;

  struct Result {
    bool ok = false;
    int downloaded = 0;
    int deleted = 0;
    int unchanged = 0;
    int failed = 0;     // downloads that errored (counted, sync continues)
    String error;       // set when ok == false (a fatal, abort-the-run error)
  };

  // storage may be null (the engine still syncs files; it just skips the
  // reindex). When non-null, refreshBooks() is called once after a successful
  // run so the device library picks up the new/removed files.
  explicit CalibreSyncManager(StorageManager *storage = nullptr)
      : storage_(storage) {}

  void setProgressCallback(ProgressCallback callback) {
    progress_ = std::move(callback);
  }

  // Brings up station WiFi (net::connectStation, using settings.* and the
  // wifi creds), runs one full reconcile against the configured library, then
  // tears the radio down. Returns a Result; never throws. When
  // settings.enabled is false this is a no-op success.
  Result runSync(const CalibreSettings &settings, const String &wifiSsid,
                 const String &wifiPassword);

  // Lower-level entry point: assumes WiFi is already up and does not touch the
  // radio. Useful for callers that manage their own connection (and for the
  // future menu). Performs search -> diff -> download -> delete ->
  // manifest rewrite -> reindex.
  Result reconcile(const CalibreSettings &settings);

  // Manifest location on SD.
  static const char *manifestPath();

  // Parse a manifest JSON body into entries (cleared first). Hand-rolled with
  // the same indexOf/substring idiom as the rest of the repo. Exposed for
  // testing and reuse. Returns true when the "books" object was found (even if
  // empty).
  static bool parseManifest(const String &json,
                            std::vector<calibresync::ManifestEntry> &out);

  // Serialise a manifest. titles is parallel to entries (same index) so the
  // human-friendly title survives a round-trip even though the reconcile core
  // does not need it. Simple String concat, matching settingsJson() etc.
  static String serializeManifest(
      const std::vector<calibresync::ManifestEntry> &entries,
      const std::vector<String> &titles);

 private:
  // Builds the change-key compared as a raw string: size + "|" + mtime. No
  // epoch parsing -- the device has no reliable clock.
  static String changeKey(const CalibreClient::RsvpRef &ref);

  // /books/books/<sanitized title or id>.rsvp. Folder routing default is
  // /books/books. TODO(future tag rule): route to /books/articles when a
  // Calibre tag (e.g. "article") is present on the book. The tag rule is
  // intentionally NOT implemented here yet -- only the hook is left. See
  // targetDirectoryFor().
  static String destinationPath(const calibresync::RemoteEntry &remote);

  // Folder routing hook. Today always returns StoragePaths::kBookFilesPath
  // (/books/books). The future articles rule plugs in here.
  static const char *targetDirectoryFor(const calibresync::RemoteEntry &remote);

  // sanitize a title/id into a filesystem-safe base name (no extension),
  // mirroring CompanionSyncManager::sanitizeFilename().
  static String sanitizeBaseName(const String &name);

  // Streams url to path via net::get, writing through a .tmp then renaming.
  // Returns true on success.
  bool downloadTo(const String &url, const String &path,
                  const net::HttpAuth &auth);

  void report(const String &phase, int current, int total,
              const String &detail);

  StorageManager *storage_ = nullptr;
  ProgressCallback progress_;
};
