#pragma once
#include <array>
#include <string>

namespace dictionary {
// Same-directory renames plus a durable journal allow recovery after any rename.
// Only these three pack files are touched; other user SD files are never removed.
inline constexpr std::array<const char*, 3> kPackNames{"english.oewn", "manifest.json", "ATTRIBUTION.txt"};
inline constexpr char kInstallJournal[] = "/dictionaries/.install-pending";
inline std::string packPath(const char* name, const char* suffix = "") {
    return std::string{"/dictionaries/"} + name + suffix;
}
template<class Filesystem>
bool packExists(Filesystem& fs, const std::string& path) {
    auto file = fs.open(path.c_str(), "r");
    const bool result = bool(file) && !file.isDirectory();
    file.close(); return result;
}
template<class Filesystem>
bool recoverPack(Filesystem& fs) {
    auto journal = fs.open(kInstallJournal, "r");
    if (!journal) return true;
    std::array<uint8_t, 3> existed{};
    const bool valid = journal.size() == 3 && journal.read(existed.data(), existed.size()) == existed.size();
    journal.close();
    if (!valid) return false;
    for (const auto value : existed) if (value != '0' && value != '1') return false;
    for (size_t i = 0; i < kPackNames.size(); ++i) {
        const auto path = packPath(kPackNames[i]);
        const auto backup = packPath(kPackNames[i], ".previous");
        if (existed[i] == '1') {
            if (!packExists(fs, backup)) {
                if (!packExists(fs, path)) return false;
                continue;
            }
            if (packExists(fs, path) && !fs.remove(path.c_str())) return false;
            if (!fs.rename(backup.c_str(), path.c_str())) return false;
        } else if (packExists(fs, path) && !fs.remove(path.c_str())) return false;
    }
    return fs.remove(kInstallJournal);
}
template<class Filesystem>
bool promotePack(Filesystem& fs) {
    if (!recoverPack(fs)) return false;
    std::array<uint8_t, 3> existed;
    for (size_t i = 0; i < kPackNames.size(); ++i) {
        if (!packExists(fs, packPath(kPackNames[i], ".download"))) return false;
        existed[i] = packExists(fs, packPath(kPackNames[i])) ? '1' : '0';
        const auto backup = packPath(kPackNames[i], ".previous");
        if (packExists(fs, backup) && !fs.remove(backup.c_str())) return false;
    }
    auto journal = fs.open(kInstallJournal, "w");
    if (!journal) return false;
    const bool written = journal.write(existed.data(), existed.size()) == existed.size();
    journal.flush(); journal.close();
    if (!written) { fs.remove(kInstallJournal); return false; }
    // Confirm the journal is readable before changing any live file.
    journal = fs.open(kInstallJournal, "r");
    std::array<uint8_t, 3> check{};
    const bool durable = journal && journal.size() == 3
        && journal.read(check.data(), check.size()) == check.size() && check == existed;
    journal.close();
    if (!durable) { fs.remove(kInstallJournal); return false; }
    for (size_t i = 0; i < kPackNames.size(); ++i) {
        const auto path = packPath(kPackNames[i]);
        if ((existed[i] == '1' && !fs.rename(path.c_str(), packPath(kPackNames[i], ".previous").c_str()))
            || !fs.rename(packPath(kPackNames[i], ".download").c_str(), path.c_str())) {
            recoverPack(fs); return false;
        }
    }
    if (!fs.remove(kInstallJournal)) { recoverPack(fs); return false; }
    for (const auto* name : kPackNames) fs.remove(packPath(name, ".previous").c_str());
    return true;
}
}
