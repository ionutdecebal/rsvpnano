#include "storage/fs/OnboardingStorage.h"

#include <FS.h>
#include <array>
#include <cstdio>
#include <string>

#include "storage/fs/StoragePaths.h"

namespace OnboardingStorage {
namespace {
constexpr const char* folders[]{StoragePaths::kLibraryPath, StoragePaths::kBookFilesPath,
    StoragePaths::kArticleFilesPath, StoragePaths::kConfigPath, StoragePaths::kThemesPath,
    StoragePaths::kFontsPath, StoragePaths::kLocalesPath};

bool directory(fs::FS& disk, const char* path) {
    auto file = disk.open(path, FILE_READ);
    if (file) { const bool result = file.isDirectory(); file.close(); return result; }
    if (!disk.mkdir(path)) return false;
    file = disk.open(path, FILE_READ);
    const bool result = file && file.isDirectory();
    if (file) file.close();
    return result;
}

bool probe(fs::FS& disk, const char* folder, uint32_t nonce, Stop stop, void* context) {
    const auto stopped = [&] { return stop && stop(context); };
    std::string path;
    bool available = false;
    // A random per-job nonce plus bounded collision handling avoids overwriting
    // even an existing probe left by a previous interrupted run.
    for (uint32_t attempt = 0; attempt < 16 && !stopped(); ++attempt) {
        char suffix[40];
        std::snprintf(suffix, sizeof(suffix), "/.nano-check-%08lx-%lu.tmp",
                      static_cast<unsigned long>(nonce), static_cast<unsigned long>(attempt));
        path = std::string{folder} + suffix;
        if (!disk.exists(path.c_str())) { available = true; break; }
    }
    if (!available || stopped()) return false;
    auto file = disk.open(path.c_str(), FILE_WRITE);
    if (!file) return false;
    bool good = true;
    std::array<uint8_t, 512> buffer{};
    // 4 KiB in each data directory is a setup smoke test, not a speed benchmark.
    for (uint32_t block = 0; block < 8 && good; ++block) {
        if (stopped()) { good = false; break; }
        for (size_t i = 0; i < buffer.size(); ++i) buffer[i] = uint8_t(nonce + block * 31 + i * 17);
        good = file.write(buffer.data(), buffer.size()) == buffer.size();
    }
    file.flush(); file.close();
    if (good && !stopped()) {
        file = disk.open(path.c_str(), FILE_READ);
        good = file && !file.isDirectory() && file.size() == buffer.size() * 8;
        for (uint32_t block = 0; block < 8 && good; ++block) {
            if (stopped()) { good = false; break; }
            good = file.read(buffer.data(), buffer.size()) == buffer.size();
            for (size_t i = 0; i < buffer.size() && good; ++i)
                good = buffer[i] == uint8_t(nonce + block * 31 + i * 17);
        }
        if (file) file.close();
    } else good = false;
    // Remove only this attempt's file, including short writes or cancellation.
    const bool cleaned = disk.remove(path.c_str());
    return good && cleaned && !stopped();
}
}

Progress check(fs::FS& disk, bool mounted, uint32_t nonce, Report report, Stop stop, void* context) {
    const auto stopped = [&] { return stop && stop(context); };
    const auto emit = [&](Progress p) { if (report) report(context, p); return p; };
    emit({State::Checking, 0});
    if (stopped()) return {State::Cancelled, 0};
    if (!mounted) return {State::Unavailable, 0};
    auto root = disk.open("/", FILE_READ);
    const bool open = root && root.isDirectory();
    if (root) root.close();
    if (stopped()) return {State::Cancelled, 0};
    if (!open) return {State::Unavailable, 0};
    emit({State::Checking, 1});
    for (const auto* folder : folders) {
        if (stopped()) return {State::Cancelled, 1};
        if (!directory(disk, folder)) return {stopped() ? State::Cancelled : State::FoldersFailed, 1};
    }
    emit({State::Checking, 2});
    for (const auto* folder : folders) {
        if (stopped()) return {State::Cancelled, 2};
        if (!probe(disk, folder, nonce, stop, context))
            return {stopped() ? State::Cancelled : State::SaveFailed, 2};
    }
    return {stopped() ? State::Cancelled : State::Ready, 2};
}
}
