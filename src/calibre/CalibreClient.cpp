#include "calibre/CalibreClient.h"

#include <algorithm>

// The pure parsers mirror src/update/ReleaseParser.cpp (String scanning,
// jsonUnescape, key/colon/quote walking). The networking methods mirror
// src/update/OtaUpdater.cpp (fetchLatestRelease: build URL, GET via the shared
// transport, then parse the body). Networking is delegated to net::HttpFetch.

// ---- Pure parsers ----------------------------------------------------------
namespace calibreparser {
namespace {

bool isAsciiWhitespace(char c) {
  switch (c) {
    case ' ':
    case '\t':
    case '\n':
    case '\r':
    case '\f':
    case '\v':
      return true;
    default:
      return false;
  }
}

// Copied in spirit from ReleaseParser::jsonUnescape -- decode the JSON string
// escapes Calibre emits (notably "\/" in URLs).
String jsonUnescape(const String &input) {
  String output;
  output.reserve(input.length());

  bool escaping = false;
  for (size_t i = 0; i < input.length(); ++i) {
    const char c = input[i];
    if (escaping) {
      switch (c) {
        case '"':
        case '\\':
        case '/':
          output += c;
          break;
        case 'b':
          output += '\b';
          break;
        case 'f':
          output += '\f';
          break;
        case 'n':
          output += '\n';
          break;
        case 'r':
          output += '\r';
          break;
        case 't':
          output += '\t';
          break;
        default:
          output += c;
          break;
      }
      escaping = false;
      continue;
    }
    if (c == '\\') {
      escaping = true;
      continue;
    }
    output += c;
  }
  return output;
}

// Reads the JSON string literal that starts at the opening quote quoteIndex.
bool parseJsonStringAt(const String &json, int quoteIndex, String &value) {
  if (quoteIndex < 0 || static_cast<size_t>(quoteIndex) >= json.length() ||
      json[quoteIndex] != '"') {
    return false;
  }

  String raw;
  raw.reserve(64);
  bool escaping = false;
  for (size_t i = static_cast<size_t>(quoteIndex) + 1; i < json.length(); ++i) {
    const char c = json[i];
    if (!escaping && c == '"') {
      value = jsonUnescape(raw);
      return true;
    }
    raw += c;
    if (escaping) {
      escaping = false;
    } else if (c == '\\') {
      escaping = true;
    }
  }
  return false;
}

// Index just past the colon that follows the next occurrence of "key", starting
// at searchStart. Returns -1 if not found. keyPosition (when given) receives the
// index of the key's opening quote.
int colonAfterKey(const String &json, const char *key, size_t searchStart, int *keyPosition) {
  const String pattern = "\"" + String(key) + "\"";
  const int keyIndex = json.indexOf(pattern, static_cast<unsigned int>(searchStart));
  if (keyIndex < 0) {
    return -1;
  }
  const int colonIndex = json.indexOf(':', keyIndex + pattern.length());
  if (colonIndex < 0) {
    return -1;
  }
  if (keyPosition != nullptr) {
    *keyPosition = keyIndex;
  }
  return colonIndex + 1;
}

// Index of the first non-whitespace char at/after pos.
int skipWhitespace(const String &json, int pos) {
  while (pos >= 0 && static_cast<size_t>(pos) < json.length() && isAsciiWhitespace(json[pos])) {
    ++pos;
  }
  return pos;
}

// Mirrors ReleaseParser::extractJsonStringValue: find "key": "value".
bool extractStringValue(const String &json, const char *key, size_t searchStart, String &value,
                        int *keyPosition = nullptr) {
  const int afterColon = colonAfterKey(json, key, searchStart, keyPosition);
  if (afterColon < 0) {
    return false;
  }
  const int quoteIndex = skipWhitespace(json, afterColon);
  if (static_cast<size_t>(quoteIndex) >= json.length() || json[quoteIndex] != '"') {
    return false;
  }
  return parseJsonStringAt(json, quoteIndex, value);
}

// find "key": <integer>. Reads an unsigned decimal run; returns false if the
// value after the colon is not a number.
bool extractUintValue(const String &json, const char *key, size_t searchStart, size_t &value,
                      int *keyPosition = nullptr) {
  const int afterColon = colonAfterKey(json, key, searchStart, keyPosition);
  if (afterColon < 0) {
    return false;
  }
  int pos = skipWhitespace(json, afterColon);
  if (static_cast<size_t>(pos) >= json.length() || json[pos] < '0' || json[pos] > '9') {
    return false;
  }
  size_t acc = 0;
  bool any = false;
  while (static_cast<size_t>(pos) < json.length() && json[pos] >= '0' && json[pos] <= '9') {
    acc = acc * 10 + static_cast<size_t>(json[pos] - '0');
    any = true;
    ++pos;
  }
  if (!any) {
    return false;
  }
  value = acc;
  return true;
}

}  // namespace

bool parseLibraryInfo(const String &json, LibraryInfo &out) {
  out = LibraryInfo();
  return extractStringValue(json, "default_library", 0, out.defaultLibrary) &&
         !out.defaultLibrary.isEmpty();
}

bool parseSearchBookIds(const String &json, std::vector<int> &out) {
  out.clear();
  int keyPos = -1;
  const int afterColon = colonAfterKey(json, "book_ids", 0, &keyPos);
  if (afterColon < 0) {
    return false;
  }
  int pos = skipWhitespace(json, afterColon);
  if (static_cast<size_t>(pos) >= json.length() || json[pos] != '[') {
    return false;
  }
  ++pos;  // step past '['

  while (static_cast<size_t>(pos) < json.length()) {
    pos = skipWhitespace(json, pos);
    if (static_cast<size_t>(pos) >= json.length()) {
      break;
    }
    const char c = json[pos];
    if (c == ']') {
      return true;  // book_ids present (possibly empty)
    }
    if (c == ',') {
      ++pos;
      continue;
    }
    if (c == '-' || (c >= '0' && c <= '9')) {
      int sign = 1;
      if (c == '-') {
        sign = -1;
        ++pos;
      }
      int acc = 0;
      bool any = false;
      while (static_cast<size_t>(pos) < json.length() && json[pos] >= '0' && json[pos] <= '9') {
        acc = acc * 10 + (json[pos] - '0');
        any = true;
        ++pos;
      }
      if (any) {
        out.push_back(sign * acc);
      }
      continue;
    }
    // Unexpected token inside the array -- stop defensively.
    break;
  }
  // book_ids was found and we scanned what we could; treat as present.
  return true;
}

bool parseBookRsvpRef(const String &json, RsvpRef &out) {
  out = RsvpRef();

  // Locate the other_formats object, then the rsvp url within it. We scope the
  // "rsvp" lookup to start at other_formats so an unrelated "rsvp" elsewhere
  // cannot masquerade as the format url.
  int otherFormatsKey = -1;
  const int otherFormatsColon = colonAfterKey(json, "other_formats", 0, &otherFormatsKey);
  if (otherFormatsColon < 0) {
    return false;  // no formats at all -> not found, no crash
  }

  // Calibre keys the format lowercase in other_formats ("rsvp").
  String rsvpUrl;
  if (!extractStringValue(json, "rsvp", static_cast<size_t>(otherFormatsColon), rsvpUrl) ||
      rsvpUrl.isEmpty()) {
    return false;  // book has no rsvp format -> not-found (edge case, no crash)
  }
  out.url = rsvpUrl;

  // Best-effort metadata. Calibre keys formats LOWERCASE everywhere, so the
  // per-format block is format_metadata.rsvp.{size,mtime}. format_metadata holds
  // one sub-object per format (e.g. "epub" and "rsvp"), each with its own size /
  // mtime, so we must scope the size/mtime lookups to the "rsvp" sub-object's
  // braces -- otherwise we could read a neighbor format's size.
  int formatMetadataKey = -1;
  const int formatMetadataColon =
      colonAfterKey(json, "format_metadata", 0, &formatMetadataKey);
  if (formatMetadataColon >= 0) {
    int rsvpMetaKey = -1;
    const int rsvpMetaColon =
        colonAfterKey(json, "rsvp", static_cast<size_t>(formatMetadataColon), &rsvpMetaKey);
    if (rsvpMetaColon >= 0) {
      const size_t metaStart = static_cast<size_t>(rsvpMetaColon);
      // Bound the search to this sub-object so size/mtime come from rsvp's block.
      const int objEnd = json.indexOf('}', static_cast<unsigned int>(metaStart));
      const String metaBlock =
          objEnd >= 0 ? json.substring(static_cast<unsigned int>(metaStart),
                                       static_cast<unsigned int>(objEnd) + 1)
                      : json.substring(static_cast<unsigned int>(metaStart));
      size_t size = 0;
      if (extractUintValue(metaBlock, "size", 0, size)) {
        out.size = size;
      }
      String mtime;
      if (extractStringValue(metaBlock, "mtime", 0, mtime) && !mtime.isEmpty()) {
        out.lastModified = mtime;
      }
    }
  }

  // Fall back to the top-level last_modified when the per-format mtime is absent.
  if (out.lastModified.isEmpty()) {
    String lastModified;
    if (extractStringValue(json, "last_modified", 0, lastModified)) {
      out.lastModified = lastModified;
    }
  }

  return true;
}

}  // namespace calibreparser

// ---- On-device client ------------------------------------------------------
// The networking methods use Serial + net::HttpFetch, which only exist in the
// Arduino build. Guarded out of the host unit test (which compiles this TU only
// to reach the pure calibreparser functions above), mirroring the guard in
// src/net/HttpFetch.cpp. The class is declared unconditionally in the header;
// only its definitions are device-only.
#if defined(ARDUINO) || defined(ESP32)

namespace {
constexpr const char *kLogTag = "[calibre]";
constexpr size_t kMaxAjaxJsonBytes = 64UL * 1024UL;

// Minimal percent-encoding for a search query's value. Encodes the characters
// that would break the URL or the query syntax; leaves typical word chars alone.
String urlEncodeQuery(const String &value) {
  String out;
  out.reserve(value.length() + 8);
  for (size_t i = 0; i < value.length(); ++i) {
    const char c = value[i];
    const bool unreserved = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                            (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~';
    if (unreserved) {
      out += c;
    } else if (c == ' ') {
      out += "%20";
    } else {
      static const char *hex = "0123456789ABCDEF";
      out += '%';
      out += hex[(static_cast<uint8_t>(c) >> 4) & 0x0F];
      out += hex[static_cast<uint8_t>(c) & 0x0F];
    }
  }
  return out;
}
}  // namespace

String CalibreClient::urlFor(const String &path) const {
  String base = config_.baseUrl;
  while (base.endsWith("/")) {
    base.remove(base.length() - 1);
  }
  if (path.startsWith("/")) {
    return base + path;
  }
  return base + "/" + path;
}

String CalibreClient::downloadUrl(const RsvpRef &ref) const {
  if (ref.url.isEmpty()) {
    return String();
  }
  if (ref.url.startsWith("http://") || ref.url.startsWith("https://")) {
    return ref.url;
  }
  return urlFor(ref.url);
}

String CalibreClient::resolveLibraryId() {
  String body;
  const net::HttpResult res =
      net::getToString(urlFor("/ajax/library-info"), body, config_.auth, kMaxAjaxJsonBytes);
  if (!res.ok) {
    Serial.printf("%s library-info failed: %s\n", kLogTag, res.error.c_str());
    return String();
  }

  calibreparser::LibraryInfo info;
  if (!calibreparser::parseLibraryInfo(body, info)) {
    Serial.printf("%s library-info missing default_library\n", kLogTag);
    return String();
  }
  config_.libraryId = info.defaultLibrary;
  return config_.libraryId;
}

std::vector<int> CalibreClient::search(const String &query) {
  std::vector<int> ids;
  if (config_.libraryId.isEmpty() && resolveLibraryId().isEmpty()) {
    return ids;
  }

  const String url = urlFor("/ajax/search?query=" + urlEncodeQuery(query) +
                            "&library_id=" + urlEncodeQuery(config_.libraryId));
  String body;
  const net::HttpResult res = net::getToString(url, body, config_.auth, kMaxAjaxJsonBytes);
  if (!res.ok) {
    Serial.printf("%s search failed: %s\n", kLogTag, res.error.c_str());
    return ids;
  }

  calibreparser::parseSearchBookIds(body, ids);
  return ids;
}

bool CalibreClient::resolveRsvp(int bookId, RsvpRef &out) {
  out = RsvpRef();
  if (config_.libraryId.isEmpty() && resolveLibraryId().isEmpty()) {
    return false;
  }

  const String url = urlFor("/ajax/book/" + String(bookId) +
                            "?library_id=" + urlEncodeQuery(config_.libraryId));
  String body;
  const net::HttpResult res = net::getToString(url, body, config_.auth, kMaxAjaxJsonBytes);
  if (!res.ok) {
    Serial.printf("%s book %d failed: %s\n", kLogTag, bookId, res.error.c_str());
    return false;
  }

  if (!calibreparser::parseBookRsvpRef(body, out)) {
    Serial.printf("%s book %d has no rsvp format\n", kLogTag, bookId);
    return false;
  }
  return true;
}

#endif  // defined(ARDUINO) || defined(ESP32)
