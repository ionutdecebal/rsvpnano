#pragma once

#include <Arduino.h>
#include <FS.h>
#include <vector>

class BufferedWriter {
public:
    static constexpr size_t kDefaultCapacity = 4096;

    explicit BufferedWriter(File& file, size_t capacity = kDefaultCapacity);
    ~BufferedWriter();

    bool write(const void* data, size_t len);
    bool flush();
    bool seek(uint32_t position);
    void discard();

private:
    File& file_;
    std::vector<uint8_t> buffer_;
    bool failed_ = false;
};
