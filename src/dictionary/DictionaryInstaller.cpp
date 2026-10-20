#include "dictionary/DictionaryInstaller.h"
#include "dictionary/PackTransaction.h"
#include "dictionary/DictionaryPack.h"
#include "board/BoardStorage.h"
#include "network/WifiConnection.h"
#include <WiFi.h>
#include <SHA2Builder.h>
#include <esp_vfs_fat.h>
#include <glaze/glaze.hpp>
#include <array>

namespace dictionary {
namespace {
// Dictionary data is versioned independently of firmware releases.
constexpr char kBaseUrl[] = "https://github.com/ionutdecebal/rsvpnano/releases/download/dictionary-en-2025.1/";
struct Manifest { std::string sha256; uint32_t bytes = 0; };
struct WifiScope {
    bool owned = WiFi.status() != WL_CONNECTED;
    ~WifiScope() { if (owned) net::disconnect(); }
};
bool writeText(fs::FS& fs, const std::string& path, std::string_view text) {
    auto file = fs.open(path.c_str(), FILE_WRITE);
    if (!file) return false;
    const bool ok = file.write(reinterpret_cast<const uint8_t*>(text.data()), text.size()) == text.size();
    file.flush(); file.close();
    if (!ok) return false;
    file = fs.open(path.c_str(), FILE_READ);
    if (!file || file.size() != text.size()) return false;
    std::array<uint8_t, 256> check;
    size_t offset = 0;
    while (offset < text.size()) {
        const size_t n = std::min(check.size(), text.size() - offset);
        if (file.read(check.data(), n) != n
            || std::string_view{reinterpret_cast<const char*>(check.data()), n} != text.substr(offset, n))
            return false;
        offset += n;
    }
    return true;
}
}
bool installedPack() { return packExists(Board::Storage::filesystem(), packPath("english.oewn")); }
bool recoverInstall() { return recoverPack(Board::Storage::filesystem()); }
std::expected<void, std::string> installEnglish(const settings::DeviceSettings& settings,
    const settings::DeviceSecrets& secrets, bool replace, const InstallStatus& status,
    const std::function<bool()>& cancelled) {
    auto& fs = Board::Storage::filesystem();
    if (!Board::Storage::cardSize()) return std::unexpected("Insert a writable SD card");
    if (!recoverPack(fs)) return std::unexpected("SD recovery needed; existing files retained");
    if (installedPack() && !replace) return std::unexpected("A dictionary is installed; choose Replace explicitly");
    for (const auto* name : kPackNames) {
        const auto staged = packPath(name, ".download");
        if (packExists(fs, staged) && !fs.remove(staged.c_str()))
            return std::unexpected("Could not clear interrupted download");
    }
    uint64_t total = 0, free = 0;
    if (esp_vfs_fat_info("/sdcard", &total, &free) != ESP_OK)
        return std::unexpected("Could not check SD free space");
    if (free < kEnglishBytes + 1024 * 1024) return std::unexpected("Need at least 49 MiB free on SD");
    auto directory = fs.open("/dictionaries", FILE_READ);
    const bool haveDirectory = directory && directory.isDirectory();
    directory.close();
    if (!haveDirectory && !fs.mkdir("/dictionaries")) return std::unexpected("Could not create dictionary folder");
    // Probe staging before spending time on a large network transfer.
    if (!writeText(fs, packPath("english.oewn", ".download"), {})) return std::unexpected("SD card is not writable");
    WifiScope wifi;
    status("Connecting Wi-Fi", "Middle button cancels", -1);
    if (wifi.owned && !net::connectSaved(settings.network, secrets))
        return std::unexpected("Set up Wi-Fi, then retry");
    if (cancelled()) return std::unexpected("Cancelled");
    auto manifestText = net::downloadText(std::string{kBaseUrl} + "manifest.json", 4096);
    auto attribution = net::downloadText(std::string{kBaseUrl} + "ATTRIBUTION.txt", 4096);
    if (!manifestText || !attribution) return std::unexpected("Dictionary download unavailable; check Wi-Fi and retry");
    Manifest manifest;
    if (glz::read<glz::opts{.error_on_unknown_keys = false}>(manifest, *manifestText)
        || manifest.bytes != kEnglishBytes || manifest.sha256 != kEnglishSha256)
        return std::unexpected("Dictionary manifest does not match this release");
    auto file = fs.open(packPath("english.oewn", ".download").c_str(), FILE_WRITE);
    if (!file) return std::unexpected("SD card is not writable");
    SHA256Builder hash; hash.begin();
    int last = -1;
    net::DownloadControl control{cancelled, [&](size_t done, size_t expected) {
        const int percent = int(done * 100ULL / expected);
        if (percent == last) return;
        last = percent;
        char line[64];
        snprintf(line, sizeof(line), "%u / %u KiB · middle cancels", unsigned(done / 1024), unsigned(expected / 1024));
        status("Downloading English dictionary", line, percent);
    }};
    auto result = net::download(std::string{kBaseUrl} + "english.oewn", kEnglishBytes, kEnglishBytes,
        [&](const uint8_t* data, size_t count) {
            if (file.write(data, count) != count) return false;
            hash.add(data, count); return true;
        }, control);
    file.flush(); file.close(); hash.calculate();
    if (!result || std::string_view{hash.toString().c_str()} != kEnglishSha256) {
        fs.remove(packPath("english.oewn", ".download").c_str());
        return std::unexpected(result ? "Dictionary checksum mismatch; retry" : result.error());
    }
    if (cancelled()) return std::unexpected("Cancelled");
    status("Verifying SD dictionary", "Existing pack is still untouched", 100);
    file = fs.open(packPath("english.oewn", ".download").c_str(), FILE_READ);
    SHA256Builder diskHash; diskHash.begin(); std::array<uint8_t, 2048> buffer;
    size_t remaining = kEnglishBytes;
    if (!file || file.size() != kEnglishBytes) return std::unexpected("Downloaded file size is wrong");
    while (remaining) {
        if (cancelled()) { file.close(); return std::unexpected("Cancelled"); }
        const size_t wanted = std::min(remaining, buffer.size());
        const size_t got = file.read(buffer.data(), wanted);
        if (got != wanted) { file.close(); return std::unexpected("Could not verify SD file"); }
        diskHash.add(buffer.data(), got); remaining -= got;
        delay(1);
    }
    diskHash.calculate();
    std::string definition;
    const bool headerOk = findDefinition([&](uint32_t offset, uint8_t* data, size_t count) {
        return file.seek(offset) && file.read(data, count) == count;
    }, file.size(), "book", definition);
    file.close();
    if (!headerOk || std::string_view{diskHash.toString().c_str()} != kEnglishSha256)
        return std::unexpected("SD dictionary verification failed");
    if (!writeText(fs, packPath("manifest.json", ".download"), *manifestText)
        || !writeText(fs, packPath("ATTRIBUTION.txt", ".download"), *attribution))
        return std::unexpected("Could not save dictionary attribution");
    if (cancelled()) return std::unexpected("Cancelled");
    status("Installing verified dictionary", "Keep the reader powered", 100);
    if (!promotePack(fs)) return std::unexpected("Could not finish install; old pack recovery retained");
    return {};
}
}
