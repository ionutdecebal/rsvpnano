#include "update/SwitchSettings.h"
#include <algorithm>
#include <cassert>
#include <iostream>

int main() {
    OtaUpdater::Offer offer; offer.switching = true;
    auto reset = [] {
        fake::backupOpen = fake::backupWrite = fake::bootWrite = fake::partition = true;
        fake::flushFailures = 0; fake::events.clear();
    };
    auto store = [] {
        settings::SettingsStore result;
        result.value.updates = {"example/fork", "test-version"};
        return result;
    };
    for (int failure = 0; failure < 5; ++failure) {
        reset(); auto value = store();
        if (failure == 0) fake::partition = false;
        if (failure == 1) fake::backupOpen = false;
        if (failure == 2) fake::backupWrite = false;
        if (failure == 3) fake::flushFailures = 1;
        if (failure == 4) fake::bootWrite = false;
        assert(!OtaUpdater::finalizeInstall(value, offer));
        assert(value.value.updates.repositoryOwner == "example/fork");
        assert(value.value.updates.releaseTag == "test-version");
        if (failure != 4)
            assert(std::find(fake::events.begin(), fake::events.end(), "boot") == fake::events.end());
    }
    reset(); auto value = store();
    assert(OtaUpdater::finalizeInstall(value, offer));
    assert(value.value.updates.repositoryOwner.empty() && value.value.updates.releaseTag.empty());
    assert((fake::events == std::vector<std::string>{"backup", "settings", "boot"}));
    reset(); value = store(); offer.switching = false;
    assert(OtaUpdater::finalizeInstall(value, offer));
    assert(value.value.updates.repositoryOwner == "example/fork");
    assert((fake::events == std::vector<std::string>{"boot"}));
    std::cout << "OTA activation: settings commit precedes boot selection; five failure paths retain current software.\n";
}
