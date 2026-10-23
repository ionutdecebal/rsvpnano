#pragma once
#include <cstdint>
#include <expected>
#include <string>
#include <string_view>
#include <vector>

namespace update {
inline constexpr char kCatalogUrl[] = "https://ionutdecebal.github.io/rsvpnano/firmware/channels.json";
inline constexpr char kOfficialRepo[] = "ionutdecebal/rsvpnano";
inline constexpr char kNewUiBoard[] = "waveshare_esp32s3_touch_lcd_3_49";
inline constexpr char kNewUiRev2Board[] = "waveshare_esp32s3_touch_lcd_3_49_rev2";
constexpr bool supportsNewUi(std::string_view board) {
    return board == kNewUiBoard || board == kNewUiRev2Board;
}
#ifdef RSVP_CLASSIC_BUILD
inline constexpr char kCurrentChannel[] = "classic";
inline constexpr char kOtherChannel[] = "new-ui";
#else
inline constexpr char kCurrentChannel[] = "new-ui";
inline constexpr char kOtherChannel[] = "classic";
#endif
struct Release {
    std::string channel, board, tag, commit, asset, sha256;
    uint32_t bytes = 0;
};
struct Catalog {
    uint32_t schema = 0;
    std::vector<Release> releases;
};
bool validToken(std::string_view value);
bool validHex(std::string_view value, size_t length);
bool validRelease(const Release& release, std::string_view board, std::string_view asset);
std::expected<Release, std::string> selectRelease(
    std::string_view json, std::string_view channel, std::string_view board, std::string_view asset);
}
