#include "update/OtaUpdater.h"
#include "update/ReleaseSource.h"
#include "update/ReleaseParser.h"
#include "network/VerifiedDownload.h"
#include "network/WifiConnection.h"
#include "board/BoardConfig.h"
#include "FirmwareVersion.generated.h"
#include <WiFi.h>
#include <esp_ota_ops.h>
#include <SHA2Builder.h>
#include <glaze/glaze.hpp>
#include <optional>

namespace {
struct ReleaseAsset { std::string name; uint32_t size = 0; std::optional<std::string> digest; };
struct GithubRelease { std::string tag_name; std::vector<ReleaseAsset> assets; };
struct GitObject { std::string sha, type; };
struct GitRef { GitObject object; };
constexpr glz::opts jsonOptions{.error_on_unknown_keys = false};
struct WifiScope {
    bool owned = WiFi.status() != WL_CONNECTED;
    ~WifiScope() { if (owned) net::disconnect(); }
};
using Callback = OtaUpdater::StatusCallback;
void report(Callback callback, void* context, const char* line1, const char* line2, int percent) {
    if (callback) callback(context, "Firmware update", line1, line2, percent);
}
std::expected<void, std::string> connect(const settings::DeviceSettings& s,
    const settings::DeviceSecrets& secrets, Callback cb, void* context) {
    if (WiFi.status() == WL_CONNECTED) return {};
    report(cb, context, "Connecting Wi-Fi", "Using saved networks", -1);
#ifdef RSVP_CLASSIC_BUILD
    if (!net::connectStation(s.network.ssid.c_str(), secrets.wifiPassword.c_str()))
#else
    if (!net::connectSaved(s.network, secrets))
#endif
        return std::unexpected("Set up Wi-Fi, then retry");
    return {};
}
std::string assetUrl(const std::string& repository, const update::Release& r) {
    return "https://github.com/" + repository + "/releases/download/" + r.tag + "/" + r.asset;
}
std::expected<update::Release, std::string> customRelease(const update::ReleaseSource& source) {
    if (!update::validToken(source.owner) || !update::validToken(source.repo)
        || (!source.tag.empty() && !update::validToken(source.tag)))
        return std::unexpected("Invalid repository or release tag");
    const std::string repo = source.owner + "/" + source.repo;
    const std::string api = "https://api.github.com/repos/" + repo;
    auto body = net::downloadText(api + "/releases/" + (source.tag.empty() ? "latest" : "tags/" + source.tag), 65536);
    if (!body) return std::unexpected(body.error());
    GithubRelease release;
    if (glz::read<jsonOptions>(release, *body) || !update::validToken(release.tag_name))
        return std::unexpected("Invalid release metadata");
    std::string{}.swap(*body);
    update::Release result;
    result.channel = "custom"; result.board = Board::Config::BOARD_ID;
    result.tag = release.tag_name; result.asset = Board::Config::OTA_ASSET_NAME;
    bool found = false;
    for (const auto& asset : release.assets) {
        if (asset.name != result.asset) continue;
        if (found) return std::unexpected("Duplicate firmware asset");
        found = true; result.bytes = asset.size;
        if (asset.digest && asset.digest->starts_with("sha256:")) result.sha256 = asset.digest->substr(7);
    }
    if (!found || result.bytes < 65536 || result.bytes > 8U * 1024 * 1024)
        return std::unexpected("Compatible OTA image unavailable");
    if (!update::validHex(result.sha256, 64)) {
        auto sums = net::downloadText("https://github.com/" + repo + "/releases/download/"
                                     + result.tag + "/SHA256SUMS.txt", 16384);
        if (!sums) return std::unexpected("Release needs SHA256SUMS.txt or GitHub asset digest");
        std::string_view text{*sums};
        while (!text.empty()) {
            const auto end = text.find('\n');
            const auto line = text.substr(0, end);
            if (line.size() > 66 && update::validHex(line.substr(0, 64), 64)) {
                auto filename = AsciiText::trim(line.substr(64));
                if (filename.starts_with("*")) filename.remove_prefix(1);
                if (filename == result.asset) result.sha256 = line.substr(0, 64);
            }
            if (end == std::string_view::npos) break;
            text.remove_prefix(end + 1);
        }
    }
    if (!update::validHex(result.sha256, 64)) return std::unexpected("OTA checksum missing");
    auto ref = net::downloadText(api + "/git/ref/tags/" + result.tag, 4096);
    GitRef git;
    if (!ref || glz::read<jsonOptions>(git, *ref)) return std::unexpected("Cannot resolve release commit");
    for (int depth = 0; git.object.type == "tag" && depth < 3; ++depth) {
        if (!update::validHex(git.object.sha, 40)) break;
        ref = net::downloadText(api + "/git/tags/" + git.object.sha, 8192);
        if (!ref || glz::read<jsonOptions>(git, *ref)) return std::unexpected("Cannot resolve annotated tag");
    }
    if (git.object.type != "commit" || !update::validHex(git.object.sha, 40))
        return std::unexpected("Invalid release commit");
    result.commit = git.object.sha;
    return result;
}
}

std::string_view OtaUpdater::currentVersion() { return kFirmwareVersion; }

OtaUpdater::Result OtaUpdater::checkOnly(const settings::DeviceSettings& s,
    const settings::DeviceSecrets& secrets, StatusCallback cb, void* context, Target target, Offer* destination) {
    if (destination) *destination = {};
    WifiScope wifi;
    auto fail = [&](std::string error) {
        if (destination) destination->message = error;
        return Result{"Update unavailable", std::move(error)};
    };
    if (auto connected = connect(s, secrets, cb, context); !connected) return fail(connected.error());
    Offer offer;
    offer.switching = target == Target::Other;
    const auto source = update::releaseSourceForSettings(s.updates);
    const bool custom = !offer.switching && (
#ifdef RSVP_CLASSIC_BUILD
        !update::supportsNewUi(Board::Config::BOARD_ID) ||
#endif
        !source.tag.empty()
        || source.owner + "/" + source.repo != update::kOfficialRepo);
    offer.repository = custom ? source.owner + "/" + source.repo : update::kOfficialRepo;
    report(cb, context, "Checking available firmware", offer.switching ? "Other interface" : "Current interface", -1);
    std::expected<update::Release, std::string> release = std::unexpected("No update catalog");
    if (custom) release = customRelease(source);
    else {
        auto catalog = net::downloadText(update::kCatalogUrl, 16384);
        if (!catalog) return fail("Update catalog unavailable; retry after publication");
        release = update::selectRelease(*catalog, offer.switching ? update::kOtherChannel : update::kCurrentChannel,
                                       Board::Config::BOARD_ID, Board::Config::OTA_ASSET_NAME);
    }
    if (!release) return fail(release.error());
    const auto* partition = esp_ota_get_next_update_partition(nullptr);
    if (!partition || release->bytes > partition->size) return fail("Firmware does not fit this board");
    const auto version = releaseparser::versionForCommit(release->tag, release->commit);
    if (!version) return fail("Invalid firmware version");
    offer.release = std::move(*release); offer.version = *version;
    offer.ready = offer.version != currentVersion();
    offer.message = !offer.ready ? "Already up to date" : offer.switching
        ? "Switch uses official updates; custom source is backed up."
        : "Review version, then confirm installation";
    const Result result{offer.ready ? "Update available" : "Already current", offer.version};
    if (destination) *destination = std::move(offer);
    return result;
}

OtaUpdater::Result OtaUpdater::checkAndInstall(const settings::DeviceSettings& s,
    const settings::DeviceSecrets& secrets, StatusCallback cb, void* context, const Offer* offer) {
    if (!offer || !offer->ready || !update::validHex(offer->release.sha256, 64)
        || offer->release.board != Board::Config::BOARD_ID
        || offer->release.asset != Board::Config::OTA_ASSET_NAME)
        return {"Check the selected software first", "Installation requires a reviewed version"};
    WifiScope wifi;
    if (auto connected = connect(s, secrets, cb, context); !connected)
        return {"Update failed", connected.error()};
    const auto* partition = esp_ota_get_next_update_partition(nullptr);
    if (!partition || offer->release.bytes > partition->size) return {"Update failed", "Wrong image size"};
    esp_ota_handle_t handle = 0;
    report(cb, context, "Preparing installation", offer->version.c_str(), 0);
    if (esp_ota_begin(partition, offer->release.bytes, &handle) != ESP_OK)
        return {"Update failed", "Could not prepare inactive firmware slot"};
    SHA256Builder hash; hash.begin();
    int last = -1;
    net::DownloadControl control;
    control.progress = [&](size_t done, size_t total) {
        const int percent = total ? int(done * 95ULL / total) : 0;
        if (percent != last) { last = percent; report(cb, context, "Downloading firmware", offer->version.c_str(), percent); }
    };
    auto downloaded = net::download(assetUrl(offer->repository, offer->release),
        offer->release.bytes, offer->release.bytes, [&](const uint8_t* data, size_t n) {
            hash.add(data, n);
            return esp_ota_write(handle, data, n) == ESP_OK;
        }, control);
    hash.calculate();
    if (!downloaded || std::string_view{hash.toString().c_str()} != offer->release.sha256) {
        esp_ota_abort(handle);
        return {"Update failed", downloaded ? "Firmware checksum mismatch" : downloaded.error()};
    }
    report(cb, context, "Verifying firmware", offer->version.c_str(), 98);
    if (esp_ota_end(handle) != ESP_OK) return {"Update failed", "Firmware image validation failed"};
    esp_app_desc_t description{};
    if (esp_ota_get_partition_description(partition, &description) != ESP_OK)
        return {"Update failed", "Firmware descriptor unavailable"};
    // The UI task commits switch settings before selecting this verified slot.
    return {"Update ready", offer->version, true};
}
