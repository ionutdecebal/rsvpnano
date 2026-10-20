#include "network/VerifiedDownload.h"
#include <Arduino.h>
#include <esp_http_client.h>
#include <esp_crt_bundle.h>
#include <time.h>
#include <algorithm>
#include <array>
#include <strings.h>

namespace net {
bool httpsUrl(std::string_view url) {
    if (!url.starts_with("https://") || url.size() > 4096) return false;
    const auto authority = url.substr(8, url.find('/', 8) - 8);
    return !authority.empty() && authority.find('@') == std::string_view::npos
        && url.find_first_of("\r\n\t ") == std::string_view::npos;
}
std::expected<void, std::string> syncDownloadClock() {
    if (time(nullptr) >= 1700000000) return {};
    configTime(0, 0, "pool.ntp.org", "time.cloudflare.com");
    const uint32_t start = millis();
    while (time(nullptr) < 1700000000 && millis() - start < 10000) delay(50);
    if (time(nullptr) < 1700000000)
        return std::unexpected("Clock not synced; retry with Wi-Fi");
    return {};
}
namespace {
struct Headers { std::string location; };
esp_err_t headers(esp_http_client_event_t* event) {
    if (event->event_id == HTTP_EVENT_ON_HEADER
        && !strcasecmp(event->header_key, "Location")) {
        auto& out = *static_cast<Headers*>(event->user_data);
        const std::string_view value{event->header_value};
        if (value.size() <= 4096) out.location = value;
    }
    return ESP_OK;
}
struct Client {
    esp_http_client_handle_t value;
    ~Client() { if (value) esp_http_client_cleanup(value); }
};
}
std::expected<size_t, std::string> download(
    std::string_view initialUrl, size_t maximum, size_t expected,
    const DownloadSink& sink, const DownloadControl& control) {
    if (!maximum || expected > maximum) return std::unexpected("Invalid download size");
    auto cancelled = [&] { return control.cancelled && control.cancelled(); };
    if (cancelled()) return std::unexpected("Cancelled");
    if (auto clock = syncDownloadClock(); !clock) return std::unexpected(clock.error());
    std::string url{initialUrl};
    for (unsigned redirects = 0; redirects <= 5; ++redirects) {
        if (cancelled()) return std::unexpected("Cancelled");
        if (!httpsUrl(url)) return std::unexpected("Download requires a valid HTTPS URL");
        Headers response;
        esp_http_client_config_t config{};
        config.url = url.c_str();
        config.crt_bundle_attach = esp_crt_bundle_attach;
        config.timeout_ms = 15000;
        config.buffer_size = 2048;
        config.buffer_size_tx = 1024;
        config.disable_auto_redirect = true;
        config.event_handler = headers;
        config.user_data = &response;
        config.user_agent = "RSVP-Nano/0.2";
        Client client{esp_http_client_init(&config)};
        if (!client.value) return std::unexpected("Download memory unavailable");
        esp_http_client_set_header(client.value, "Accept-Encoding", "identity");
        if (esp_http_client_open(client.value, 0) != ESP_OK)
            return std::unexpected("HTTPS connection failed; retry");
        const auto length = esp_http_client_fetch_headers(client.value);
        const int status = esp_http_client_get_status_code(client.value);
        if (status == 301 || status == 302 || status == 303 || status == 307 || status == 308) {
            if (!httpsUrl(response.location)) return std::unexpected("Invalid HTTPS redirect");
            url = std::move(response.location);
            continue;
        }
        if (status != 200) return std::unexpected("Download HTTP " + std::to_string(status));
        if (length < 0) return std::unexpected("Could not read download headers");
        if (uint64_t(length) > maximum || (expected && length > 0 && uint64_t(length) != expected))
            return std::unexpected("Download size does not match");
        std::array<uint8_t, 2048> buffer;
        size_t count = 0;
        const uint32_t started = millis();
        uint32_t lastData = started;
        while (!esp_http_client_is_complete_data_received(client.value)) {
            if (cancelled()) return std::unexpected("Cancelled");
            if (millis() - lastData > 20000 || millis() - started > 30U * 60 * 1000)
                return std::unexpected("Download timed out; retry");
            const int got = esp_http_client_read(client.value, reinterpret_cast<char*>(buffer.data()), buffer.size());
            if (got < 0) return std::unexpected("Download interrupted; retry");
            if (!got) {
                if (esp_http_client_is_complete_data_received(client.value)) break;
                delay(1);
                continue;
            }
            lastData = millis();
            if (size_t(got) > maximum - count || (expected && size_t(got) > expected - count))
                return std::unexpected("Download exceeds expected size");
            if (!sink(buffer.data(), got)) return std::unexpected("Could not write download");
            count += got;
            if (control.progress) control.progress(count, expected ? expected : size_t(length));
            delay(1);
        }
        if (!count || (expected && count != expected) || (length > 0 && count != size_t(length)))
            return std::unexpected("Incomplete download; retry");
        return count;
    }
    return std::unexpected("Too many download redirects");
}
std::expected<std::string, std::string> downloadText(std::string_view url, size_t maximum) {
    std::string out;
    auto result = download(url, maximum, 0, [&](const uint8_t* data, size_t n) {
        out.append(reinterpret_cast<const char*>(data), n);
        return true;
    });
    if (!result) return std::unexpected(result.error());
    return out;
}
}
