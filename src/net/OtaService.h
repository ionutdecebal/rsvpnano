// SPDX-License-Identifier: MIT
//
// Wireless OTA over Wi-Fi for RSVP Nano.
// Connects to Wi-Fi and runs ArduinoOTA so `pio run -e ota -t upload` works.

#pragma once
#include <Arduino.h>

class DisplayManager;

namespace OtaService {

// Call once during App::begin(). Connects to Wi-Fi and starts ArduinoOTA.
// Pass `display` to get an on-screen progress bar during flashes (optional).
void begin(const char *ssid, const char *password,
           const char *hostname = "rsvpnano",
           const char *otaPassword = "rsvpnano",
           DisplayManager *display = nullptr);

// Call every loop iteration. Pumps ArduinoOTA and reconnects Wi-Fi if dropped.
void handle();

bool wifiConnected();

}  // namespace OtaService
