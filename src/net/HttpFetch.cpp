#include "net/HttpFetch.h"

#include <algorithm>

// Transport skeleton mirroring:
//   - src/update/OtaUpdater.cpp  (WiFiClientSecure + HTTPClient setup, GET,
//     getStreamPtr() read loop, http.end())
//   - src/rss/RssFeedManager.cpp (https-vs-plain client selection, chunked
//     readBytes loop with a byte cap)
//
// The HTTP body below is a faithful skeleton: it calls the same Arduino APIs the
// analog files call, but it is guarded out of the host build (which has no
// HTTPClient/WiFi) by RSVP_HTTPFETCH_HAS_ARDUINO_NET. On device that macro is
// defined (Arduino core present); on the native_test host it is not, so only the
// pure helpers (base64 / basicAuthHeaderValue) compile and are unit tested.
//
// TODO: exercise this against the live calibre-server fixtures, then
// drop the skeleton guard once it is verified on hardware.

#if defined(ARDUINO) || defined(ESP32)
#define RSVP_HTTPFETCH_HAS_ARDUINO_NET 1
#endif

#if RSVP_HTTPFETCH_HAS_ARDUINO_NET
#include <HTTPClient.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#endif

namespace net {
namespace {

constexpr const char *kLogTag = "[http]";

// Standard base64 alphabet. Kept here (not in the JSON parser) because it is a
// transport concern; pure and host-compilable so basicAuthHeaderValue is tested.
const char kBase64Alphabet[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

String base64Encode(const String &input) {
  String output;
  const size_t len = input.length();
  output.reserve(((len + 2) / 3) * 4);

  size_t i = 0;
  while (i + 3 <= len) {
    const uint32_t triple = (static_cast<uint8_t>(input[i]) << 16) |
                            (static_cast<uint8_t>(input[i + 1]) << 8) |
                            static_cast<uint8_t>(input[i + 2]);
    output += kBase64Alphabet[(triple >> 18) & 0x3F];
    output += kBase64Alphabet[(triple >> 12) & 0x3F];
    output += kBase64Alphabet[(triple >> 6) & 0x3F];
    output += kBase64Alphabet[triple & 0x3F];
    i += 3;
  }

  const size_t remaining = len - i;
  if (remaining == 1) {
    const uint32_t triple = static_cast<uint8_t>(input[i]) << 16;
    output += kBase64Alphabet[(triple >> 18) & 0x3F];
    output += kBase64Alphabet[(triple >> 12) & 0x3F];
    output += '=';
    output += '=';
  } else if (remaining == 2) {
    const uint32_t triple = (static_cast<uint8_t>(input[i]) << 16) |
                            (static_cast<uint8_t>(input[i + 1]) << 8);
    output += kBase64Alphabet[(triple >> 18) & 0x3F];
    output += kBase64Alphabet[(triple >> 12) & 0x3F];
    output += kBase64Alphabet[(triple >> 6) & 0x3F];
    output += '=';
  }

  return output;
}

}  // namespace

String basicAuthHeaderValue(const HttpAuth &auth) {
  if (auth.username.isEmpty()) {
    return String();
  }
  return String("Basic ") + base64Encode(auth.username + ":" + auth.password);
}

#if RSVP_HTTPFETCH_HAS_ARDUINO_NET

HttpResult get(const String &url, const HttpSink &sink, const HttpAuth &auth, size_t maxBytes) {
  HttpResult result;

  // Mirror RssFeedManager::fetchUrl: secure transport for https, plain otherwise.
  WiFiClientSecure secureClient;
  WiFiClient plainClient;
  secureClient.setInsecure();  // TODO: pin/verify the calibre cert.
  secureClient.setHandshakeTimeout(15);

  HTTPClient http;
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.setTimeout(15000);

  const bool begun = url.startsWith("https://") ? http.begin(secureClient, url)
                                                : http.begin(plainClient, url);
  if (!begun) {
    result.error = "HTTP begin failed";
    Serial.printf("%s begin failed url=%s\n", kLogTag, url.c_str());
    return result;
  }

  const String authHeader = basicAuthHeaderValue(auth);
  if (!authHeader.isEmpty()) {
    http.addHeader("Authorization", authHeader);
  }

  result.statusCode = http.GET();
  if (result.statusCode != HTTP_CODE_OK) {
    result.error = "HTTP " + String(result.statusCode);
    Serial.printf("%s http failed status=%d url=%s\n", kLogTag, result.statusCode, url.c_str());
    http.end();
    return result;
  }

  WiFiClient *stream = http.getStreamPtr();
  if (stream == nullptr) {
    result.error = "No response stream";
    http.end();
    return result;
  }

  // Chunked drain identical in shape to OtaUpdater::readBodyLimited, but handing
  // each chunk to the sink instead of buffering into a String.
  const int reportedSize = http.getSize();
  uint8_t buffer[512];
  size_t totalRead = 0;
  bool aborted = false;
  while (http.connected() || stream->available()) {
    if (reportedSize > 0 && totalRead >= static_cast<size_t>(reportedSize)) {
      break;
    }
    if (totalRead >= maxBytes) {
      result.error = "Response too large";
      Serial.printf("%s response exceeded cap=%u url=%s\n", kLogTag,
                    static_cast<unsigned int>(maxBytes), url.c_str());
      http.end();
      return result;
    }

    const int available = stream->available();
    if (available <= 0) {
      delay(1);
      continue;
    }

    const size_t remaining = maxBytes - totalRead;
    const size_t chunkSize =
        std::min(remaining, std::min(sizeof(buffer), static_cast<size_t>(available)));
    const int bytesRead = stream->readBytes(buffer, chunkSize);
    if (bytesRead <= 0) {
      break;
    }

    totalRead += static_cast<size_t>(bytesRead);
    if (sink && !sink(buffer, static_cast<size_t>(bytesRead))) {
      aborted = true;
      result.error = "Transfer aborted by sink";
      break;
    }
  }

  http.end();
  result.bytesReceived = totalRead;
  result.ok = !aborted && result.error.isEmpty();
  return result;
}

#else  // Host build: no Arduino networking. Keep the symbol so src compiles under
       // native_test if ever included; it always reports unavailability.

HttpResult get(const String &url, const HttpSink &sink, const HttpAuth &auth, size_t maxBytes) {
  (void)url;
  (void)sink;
  (void)auth;
  (void)maxBytes;
  HttpResult result;
  result.error = "HTTP not available on host build";
  return result;
}

#endif  // RSVP_HTTPFETCH_HAS_ARDUINO_NET

HttpResult getToString(const String &url, String &out, const HttpAuth &auth, size_t maxBytes) {
  out = "";
  out.reserve(1024);
  HttpResult result = get(
      url,
      [&out](const uint8_t *data, size_t length) {
        for (size_t i = 0; i < length; ++i) {
          out += static_cast<char>(data[i]);
        }
        return true;
      },
      auth, maxBytes);
  return result;
}

}  // namespace net
