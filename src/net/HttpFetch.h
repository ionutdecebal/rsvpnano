#pragma once

#include <Arduino.h>

#include <functional>

// A thin HTTP GET helper over the existing HTTPClient / WiFiClient transport
// that OtaUpdater and RssFeedManager already use. Mirrors the request setup in
// src/update/OtaUpdater.cpp (OtaUpdater::fetchLatestRelease) and the streaming
// read loop in src/rss/RssFeedManager.cpp (RssFeedManager::fetchUrl), but
// factors them into one reusable call so the Calibre sync engine can:
//   - fetch small ajax JSON bodies into a String, and
//   - stream large /get/rsvp downloads straight to SD via a sink callback,
//     without buffering the whole file in RAM.
//
// No JSON parsing lives here -- that is in src/calibre/CalibreClient + the pure
// parsers in CalibreClient.cpp so they stay host-testable. Logging uses the
// same Serial.printf("[tag] ...") style as the analog files; the tag here is
// "[http]".
namespace net {

// Optional HTTP Basic auth. When username is non-empty, HttpFetch adds an
// "Authorization: Basic <base64(user:pass)>" header. calibre-server is commonly
// run behind basic auth, so the sync engine needs this knob.
struct HttpAuth {
  String username;  // empty => no Authorization header is sent
  String password;
};

// Receives response bytes as they arrive. Return false to abort the transfer
// early (e.g. SD write failed); HttpFetch then reports failure. data is valid
// only for the duration of the call.
using HttpSink = std::function<bool(const uint8_t *data, size_t length)>;

// Result of a GET. ok is true only on a 2xx response that fully streamed
// without the sink aborting.
struct HttpResult {
  bool ok = false;
  int statusCode = 0;       // HTTP status, or a negative HTTPClient error code
  size_t bytesReceived = 0;  // total bytes handed to the sink
  String error;             // human-readable detail when !ok, else empty
};

// Maximum bytes a GET will accept before treating the response as runaway. The
// sync engine overrides this per call (small for ajax JSON, large for a book).
constexpr size_t kDefaultMaxResponseBytes = 256UL * 1024UL;

// GETs url, following redirects (HTTPC_STRICT_FOLLOW_REDIRECTS, matching
// OtaUpdater), and feeds the body to sink in chunks. Picks WiFiClientSecure for
// https:// urls and a plain WiFiClient otherwise, exactly like
// RssFeedManager::fetchUrl. WiFi must already be associated (callers bring the
// radio up with net::connectStation). auth is applied when set. maxBytes caps
// the accepted body. Returns an HttpResult; never throws.
HttpResult get(const String &url, const HttpSink &sink, const HttpAuth &auth = {},
               size_t maxBytes = kDefaultMaxResponseBytes);

// Convenience wrapper: GETs url and accumulates the body into out (replacing
// any prior contents). Intended for the small ajax JSON endpoints. Honors the
// same maxBytes cap so a misbehaving server cannot exhaust RAM.
HttpResult getToString(const String &url, String &out, const HttpAuth &auth = {},
                       size_t maxBytes = kDefaultMaxResponseBytes);

// Builds the value for an HTTP Basic "Authorization" header from user:password
// (i.e. "Basic " + base64). Exposed for unit testing and so callers can reuse
// it; returns "" when username is empty.
String basicAuthHeaderValue(const HttpAuth &auth);

}  // namespace net
