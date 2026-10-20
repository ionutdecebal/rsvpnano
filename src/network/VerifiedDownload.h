#pragma once
#include <cstddef>
#include <cstdint>
#include <expected>
#include <functional>
#include <string>
#include <string_view>

namespace net {
struct DownloadControl {
    std::function<bool()> cancelled;
    std::function<void(size_t, size_t)> progress;
};
using DownloadSink = std::function<bool(const uint8_t*, size_t)>;
// Uses verified HTTPS, bounded redirects, timeouts and bounded streaming memory.
std::expected<size_t, std::string> download(
    std::string_view url, size_t maximum, size_t expected,
    const DownloadSink& sink, const DownloadControl& control = {});
std::expected<std::string, std::string> downloadText(std::string_view url, size_t maximum);
std::expected<void, std::string> syncDownloadClock();
bool httpsUrl(std::string_view url);
}
