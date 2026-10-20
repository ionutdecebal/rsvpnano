#pragma once
#include "settings/SettingsStore.h"
#include <cstring>
struct Preferences {
    bool begin(const char*) { return fake::backupOpen; }
    size_t putString(const char*, const char* value) {
        fake::events.push_back("backup");
        return fake::backupWrite ? std::strlen(value) : 0;
    }
    void end() {}
};
