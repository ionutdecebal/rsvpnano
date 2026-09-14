#pragma once

#include <Arduino.h>
#include <algorithm>

// Register-backed I2C fake shared by the real TCA9554 and FT6336 drivers.
class TwoWire {
public:
    std::array<std::array<uint8_t, 256>, 128> registers = {};
    std::array<uint8_t, 128> transmissionStatus = {};
    std::vector<std::array<uint8_t, 3>> writes;
    unsigned transactions = 0;
    bool shortRead = false;
    void (*onRequest)() = nullptr;

    void beginTransmission(uint8_t address) {
        address_ = address;
        tx_.clear();
        ++transactions;
    }
    size_t write(uint8_t value) {
        tx_.push_back(value);
        return 1;
    }
    uint8_t endTransmission(bool = true) {
        if (transmissionStatus.at(address_) != 0)
            return transmissionStatus.at(address_);
        if (!tx_.empty()) {
            reg_ = tx_[0];
            for (size_t i = 1; i < tx_.size(); ++i) {
                const auto reg = static_cast<uint8_t>(reg_ + i - 1);
                registers.at(address_).at(reg) = tx_[i];
                writes.push_back({address_, reg, tx_[i]});
            }
        }
        return 0;
    }
    size_t requestFrom(uint8_t address, size_t length, bool = true) {
        rx_.clear();
        cursor_ = 0;
        ++transactions;
        const size_t count = shortRead && length > 0 ? length - 1 : length;
        for (size_t i = 0; i < count; ++i)
            rx_.push_back(registers.at(address).at(static_cast<uint8_t>(reg_ + i)));
        if (auto callback = std::exchange(onRequest, nullptr))
            callback();
        return count;
    }
    int read() {
        return rx_.at(cursor_++);
    }

private:
    uint8_t address_ = 0;
    uint8_t reg_ = 0;
    size_t cursor_ = 0;
    std::vector<uint8_t> tx_;
    std::vector<uint8_t> rx_;
};

inline TwoWire Wire1;
