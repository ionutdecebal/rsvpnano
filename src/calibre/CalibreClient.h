#pragma once

#include <Arduino.h>

#include <vector>

#include "net/HttpFetch.h"

// The on-device ajax client for a calibre-server. Mirrors the split used by
// src/update/OtaUpdater.* and src/update/ReleaseParser.*: a networking class
// (CalibreClient, analog of OtaUpdater) that does WiFi/HTTP, and a set of pure
// parsing free functions (namespace calibreparser, analog of releaseparser) that
// take a JSON String and fill plain structs so they can be unit tested on the
// host with the test/support Arduino String shim -- no networking, no SD.
//
// IMPORTANT convention note: this repo does NOT use ArduinoJson (nothing in
// src/ or platformio.ini depends on it -- ReleaseParser, FeedParser and
// CompanionSyncManager all hand-roll String-based JSON). To match the existing
// firmware, the parsers below use the same String scanning approach as
// ReleaseParser rather than ArduinoJson.
//
// Known ajax shapes (live calibre-server, captured from a live calibre-server):
//   GET /ajax/library-info
//     {"default_library":"<lib>","library_map":{...}}
//   GET /ajax/search?query=<q>&library_id=<lib>
//     {"book_ids":[1,...],"num":N,"total_num":N,"offset":0}
//   GET /ajax/book/<id>?library_id=<lib>   (format names are LOWERCASE)
//     {... "other_formats":{"rsvp":"/get/rsvp/<id>/<lib>"},
//          "format_metadata":{"rsvp":{"size":<int>,"mtime":"<iso w/ frac sec>"}},
//          "last_modified":"<iso, no frac sec>"}
//   Download: GET /get/rsvp/<book_id>/<library_id>
// Note: library_id is a ?query param on /ajax/ URLs but a path segment in /get/.

// ---- Pure parsers: host-testable, no networking ----------------------------
namespace calibreparser {

// /ajax/library-info
struct LibraryInfo {
  String defaultLibrary;  // empty if absent
};

// One book's resolved RSVP format reference (from /ajax/book/<id>).
struct RsvpRef {
  String url;           // e.g. "/get/rsvp/42/Calibre_Library"; empty => not found
  size_t size = 0;      // bytes, from format_metadata.rsvp.size; 0 if absent
  String lastModified;  // ISO timestamp; prefers format_metadata.rsvp.mtime
                        // (may carry fractional seconds), else top-level
                        // last_modified; stored raw as the manifest change-key
};

// Extracts default_library from /ajax/library-info. Returns true when a
// non-empty default_library was found.
bool parseLibraryInfo(const String &json, LibraryInfo &out);

// Extracts the integer book_ids array from /ajax/search into out (cleared
// first). Returns true when the book_ids key was present (even if empty).
bool parseSearchBookIds(const String &json, std::vector<int> &out);

// Extracts the RSVP format reference from an /ajax/book/<id> payload. Returns
// true only when other_formats.rsvp exists and is non-empty; size/lastModified
// are filled best-effort. A book with no "rsvp" key returns false and leaves
// out.url empty (the not-found case -- must not crash).
bool parseBookRsvpRef(const String &json, RsvpRef &out);

}  // namespace calibreparser

// ---- On-device client: networking, mirrors OtaUpdater ----------------------
class CalibreClient {
 public:
  struct Config {
    String baseUrl;      // e.g. "http://192.168.1.20:8080" (no trailing slash)
    String libraryId;    // optional; resolveLibraryId() fills it when empty
    net::HttpAuth auth;  // optional HTTP Basic credentials
  };

  explicit CalibreClient(const Config &config) : config_(config) {}

  // Re-export so callers do not need a second include.
  using RsvpRef = calibreparser::RsvpRef;

  // GET /ajax/library-info, store and return the default library id. On success
  // libraryId() reflects it. Returns "" on failure.
  String resolveLibraryId();

  // GET /ajax/search?query=<query>&library_id=<lib>. Returns the matching book
  // ids; empty on failure or no matches. Uses libraryId() (resolving first if
  // unset).
  std::vector<int> search(const String &query);

  // GET /ajax/book/<bookId>?library_id=<lib>, fill out with the RSVP format
  // reference. Returns true when a downloadable rsvp format was found.
  bool resolveRsvp(int bookId, RsvpRef &out);

  // Absolute download URL for a book's rsvp format, given the (possibly
  // relative) url from a RsvpRef. Joined against baseUrl.
  String downloadUrl(const RsvpRef &ref) const;

  const String &libraryId() const { return config_.libraryId; }

 private:
  // Joins config_.baseUrl with a server-relative path ("/ajax/...").
  String urlFor(const String &path) const;

  Config config_;
};
