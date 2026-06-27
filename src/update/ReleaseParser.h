#pragma once

#include <Arduino.h>

// Pure parsing of a GitHub release JSON payload. No networking, no
// SD access -- safe to unit test on the host with the Arduino String shim.
namespace releaseparser {

struct ReleaseInfo {
  String tagName;   // empty if the payload had no usable tag_name
  String assetUrl;  // empty if no asset matched assetName
};

// Extracts the release tag and the browser_download_url of the asset whose
// "name" equals assetName. Fills whatever it finds; missing fields stay empty.
// Returns true when a non-empty tag was found.
bool parse(const String &json, const String &assetName, ReleaseInfo &out);

}  // namespace releaseparser
