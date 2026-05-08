// SPDX-License-Identifier: MIT
//
// Copy to `src/Secrets.h` and fill in your Wi-Fi creds.
// Secrets.h is gitignored so your real credentials never land in version
// control. Leave the SSID empty to disable OTA entirely (offline-only build).

#pragma once

namespace Secrets {

constexpr const char *kWifiSsid     = "";   // leave empty to disable OTA
constexpr const char *kWifiPassword = "";

constexpr const char *kOtaHostname  = "rsvpnano";  // → rsvpnano.local
constexpr const char *kOtaPassword  = "rsvpnano";  // matches --auth=

}  // namespace Secrets
