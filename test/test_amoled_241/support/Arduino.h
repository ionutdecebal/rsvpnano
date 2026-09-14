#pragma once

#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

#define IRAM_ATTR
constexpr int INPUT = 0;
constexpr int OUTPUT = 1;
constexpr int INPUT_PULLUP = 2;
constexpr int LOW = 0;
constexpr int HIGH = 1;
constexpr int FALLING = 3;

namespace FakeArduino {
    struct State {
        std::array<int, 49> modes;
        std::array<int, 49> levels;
        std::vector<std::pair<int, int>> writes;
        std::vector<unsigned long> delays;
        void (*interrupt)() = nullptr;
        int interruptPin = -1;
        int interruptMode = -1;
        unsigned notifications = 0;
        unsigned attaches = 0;
        unsigned detaches = 0;

        State() {
            modes.fill(-1);
            levels.fill(HIGH);
        }
    };
    inline State state;
}

inline void pinMode(int pin, int mode) {
    FakeArduino::state.modes.at(pin) = mode;
}
inline void digitalWrite(int pin, int level) {
    FakeArduino::state.levels.at(pin) = level;
    FakeArduino::state.writes.emplace_back(pin, level);
}
inline int digitalRead(int pin) {
    return FakeArduino::state.levels.at(pin);
}
inline void attachInterrupt(int pin, void (*callback)(), int mode) {
    auto& state = FakeArduino::state;
    assert(pin >= 0 && pin < 49);
    state.interruptPin = pin;
    state.interruptMode = mode;
    state.interrupt = callback;
    ++state.attaches;
}
inline void detachInterrupt(int pin) {
    auto& state = FakeArduino::state;
    assert(pin == state.interruptPin);
    state.interrupt = nullptr;
    ++state.detaches;
}
inline void delay(unsigned long ms) {
    FakeArduino::state.delays.push_back(ms);
}
inline void delayMicroseconds(unsigned int) {}
