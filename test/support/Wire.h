#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

class TwoWire {
public:
    std::vector<std::vector<uint8_t>> writes;
    std::vector<std::vector<uint8_t>> responses;
    uint8_t transmissionStatus = 0;
    void (*onRequest)() = nullptr;

    void beginTransmission(uint8_t) { writes.emplace_back(); }
    uint8_t endTransmission(bool = true) { return transmissionStatus; }
    size_t write(uint8_t value) {
        writes.back().push_back(value);
        return 1;
    }
    size_t write(const uint8_t* data, size_t length) {
        writes.back().insert(writes.back().end(), data, data + length);
        return length;
    }
    size_t requestFrom(uint8_t, size_t length, bool = true) {
        if (onRequest)
            onRequest();
        readOffset_ = 0;
        currentResponse_ = nextResponse_++;
        return currentResponse_ < responses.size() ? responses[currentResponse_].size() : length;
    }
    int available() const {
        return currentResponse_ < responses.size() ? responses[currentResponse_].size() - readOffset_ : 0;
    }
    int read() {
        return currentResponse_ < responses.size() ? responses[currentResponse_][readOffset_++] : 0;
    }

private:
    size_t nextResponse_ = 0;
    size_t currentResponse_ = 0;
    size_t readOffset_ = 0;
};

inline TwoWire Wire;
