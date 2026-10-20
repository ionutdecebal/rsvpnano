#pragma once
#include <expected>
#include <string>
#include <vector>
namespace fake {
inline bool backupOpen = true, backupWrite = true, bootWrite = true, partition = true;
inline int flushFailures = 0;
inline std::vector<std::string> events;
}
namespace settings {
struct UpdateSettings { std::string repositoryOwner, releaseTag; };
struct DeviceSettings { UpdateSettings updates; };
struct SettingsStore {
    DeviceSettings value;
    DeviceSettings& settings() { return value; }
    void acceptChanges() {}
    std::expected<void, std::string> flush() {
        fake::events.push_back("settings");
        if (fake::flushFailures > 0) { --fake::flushFailures; return std::unexpected("SD unavailable"); }
        return {};
    }
};
}
