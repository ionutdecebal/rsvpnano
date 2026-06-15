#include "update/ReleaseParser.h"

#include <algorithm>

namespace releaseparser {
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

bool extractJsonStringValue(const String &json, const char *key, size_t searchStart, String &value,
                            int *keyPosition = nullptr) {
  const String pattern = "\"" + String(key) + "\"";
  const int keyIndex = json.indexOf(pattern, static_cast<unsigned int>(searchStart));
  if (keyIndex < 0) {
    return false;
  }

  const int colonIndex = json.indexOf(':', keyIndex + pattern.length());
  if (colonIndex < 0) {
    return false;
  }

  int quoteIndex = colonIndex + 1;
  while (static_cast<size_t>(quoteIndex) < json.length() && isAsciiWhitespace(json[quoteIndex])) {
    ++quoteIndex;
  }
  if (static_cast<size_t>(quoteIndex) >= json.length() || json[quoteIndex] != '"') {
    return false;
  }

  if (keyPosition != nullptr) {
    *keyPosition = keyIndex;
  }
  return parseJsonStringAt(json, quoteIndex, value);
}

bool extractAssetDownloadUrl(const String &json, const String &assetName, String &assetUrl) {
  size_t searchStart = 0;
  String candidateName;
  int nameKeyIndex = -1;
  while (extractJsonStringValue(json, "name", searchStart, candidateName, &nameKeyIndex)) {
    if (candidateName == assetName &&
        extractJsonStringValue(json, "browser_download_url",
                               static_cast<size_t>(std::max(0, nameKeyIndex)), assetUrl)) {
      return true;
    }

    searchStart = static_cast<size_t>(nameKeyIndex) + 1;
  }

  return false;
}

}  // namespace

bool parse(const String &json, const String &assetName, ReleaseInfo &out) {
  out = ReleaseInfo();
  const bool haveTag = extractJsonStringValue(json, "tag_name", 0, out.tagName) &&
                       !out.tagName.isEmpty();
  extractAssetDownloadUrl(json, assetName, out.assetUrl);
  return haveTag;
}

}  // namespace releaseparser
