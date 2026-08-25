#include "update/ReleaseParser.h"

#include <algorithm>
#include <cctype>

#include "text/AsciiText.h"

namespace releaseparser {
    std::expected<std::string, std::error_code> tagFromAssetLocation(std::string_view location,
                                                                    std::string_view assetName) {
        constexpr std::string_view marker = "/releases/download/";
        const size_t start = location.find(marker);
        if (start == std::string_view::npos || assetName.empty() || !location.ends_with(assetName)) {
            return std::unexpected(std::make_error_code(std::errc::invalid_argument));
        }

        const size_t tagStart = start + marker.size();
        const size_t assetStart = location.size() - assetName.size();
        if (assetStart <= tagStart || location[assetStart - 1] != '/')
            return std::unexpected(std::make_error_code(std::errc::invalid_argument));

        const std::string_view tag = location.substr(tagStart, assetStart - tagStart - 1);
        if (tag.empty() || tag.contains('/'))
            return std::unexpected(std::make_error_code(std::errc::invalid_argument));
        return std::string{tag};
    }

    std::expected<std::string, std::error_code> versionForCommit(std::string_view tagName, std::string_view commitSha) {
        commitSha = AsciiText::trim(commitSha);
        if (tagName.empty() || commitSha.length() != 40) {
            return std::unexpected(std::make_error_code(std::errc::invalid_argument));
        }
        if (!std::ranges::all_of(commitSha, [](char c) {
                return std::isxdigit(static_cast<unsigned char>(c));
            })) {
            return std::unexpected(std::make_error_code(std::errc::invalid_argument));
        }

        std::string version;
        version.reserve(tagName.size() + 13);
        version.append(tagName).append("+").append(commitSha.substr(0, 12));
        return version;
    }

} // namespace releaseparser
