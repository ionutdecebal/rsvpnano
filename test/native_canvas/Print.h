#pragma once
#include <cstddef>
#include <cstdint>
class Print {
public:
    virtual ~Print() = default;
    virtual size_t write(uint8_t value) = 0;
    size_t write(const uint8_t* bytes, size_t length) {
        size_t count = 0;
        for (size_t i = 0; i < length; ++i)
            count += write(bytes[i]);
        return count;
    }
};
