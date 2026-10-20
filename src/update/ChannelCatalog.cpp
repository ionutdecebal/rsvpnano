#include "update/ChannelCatalog.h"
#include <glaze/glaze.hpp>
#include <algorithm>

namespace update {
bool validToken(std::string_view value) {
    return !value.empty() && value.size() <= 128 && value != "." && value != ".."
        && std::ranges::all_of(value, [](char c) {
            return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')
                || (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.';
        });
}
bool validHex(std::string_view value, size_t length) {
    return value.size() == length && std::ranges::all_of(value, [](char c) {
        return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
    });
}
bool validRelease(const Release& r, std::string_view board, std::string_view asset) {
    return (r.channel == "classic" || r.channel == "new-ui")
        && (r.channel != "new-ui" || board == kNewUiBoard)
        && r.board == board && r.asset == asset && r.asset.ends_with("-ota.bin")
        && validToken(r.asset) && validToken(r.tag) && validHex(r.commit, 40)
        && validHex(r.sha256, 64) && r.bytes >= 65536 && r.bytes <= 8U * 1024 * 1024;
}
std::expected<Release, std::string> selectRelease(
    std::string_view json, std::string_view channel, std::string_view board, std::string_view asset) {
    if (json.size() > 16384) return std::unexpected("Update catalog too large");
    Catalog catalog;
    if (glz::read<glz::opts{.error_on_unknown_keys = false}>(catalog, json)
        || catalog.schema != 1 || catalog.releases.size() > 32)
        return std::unexpected("Invalid update catalog");
    const Release* found = nullptr;
    for (const auto& release : catalog.releases) {
        if (release.channel != channel || release.board != board) continue;
        if (found || !validRelease(release, board, asset))
            return std::unexpected("Invalid or duplicate firmware entry");
        found = &release;
    }
    if (!found) return std::unexpected("This software is not published for your board yet");
    return *found;
}
}
