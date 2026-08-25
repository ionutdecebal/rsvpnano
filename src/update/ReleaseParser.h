#pragma once

#include <expected>
#include <string>
#include <string_view>
#include <system_error>

// Pure parsing of GitHub release metadata. No networking or SD access.
namespace releaseparser {

    std::expected<std::string, std::error_code> tagFromAssetLocation(std::string_view location,
                                                                    std::string_view assetName);

    // Published builds use the release tag plus a stable abbreviated commit.
    std::expected<std::string, std::error_code> versionForCommit(std::string_view tagName, std::string_view commitSha);

} // namespace releaseparser
