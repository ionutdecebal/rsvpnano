#pragma once

#include <cstdint>

namespace fs { class FS; }

namespace OnboardingStorage {
enum class State : uint8_t { Idle, Checking, Ready, Unavailable, FoldersFailed, SaveFailed, TimedOut, Cancelled };
struct Progress {
    State state = State::Idle;
    uint8_t stage = 0; // Open card, prepare folders, save/read back.
};
inline constexpr uint32_t kTimeoutMs = 30000;
using Stop = bool (*)(void*);
using Report = void (*)(void*, Progress);

// The caller exclusively owns storage while this runs. Never repairs, migrates,
// formats, scans books, or removes files it did not create.
Progress check(fs::FS& filesystem, bool mounted, uint32_t nonce,
               Report report = nullptr, Stop stop = nullptr, void* context = nullptr);
}
