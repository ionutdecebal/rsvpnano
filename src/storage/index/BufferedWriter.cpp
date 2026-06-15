#include "storage/index/BufferedWriter.h"

#include <cstring>

BufferedWriter::BufferedWriter(File& file, size_t capacity) : file_(file) {
    buffer_.reserve(capacity);
}

BufferedWriter::~BufferedWriter() {
    flush();
}

bool BufferedWriter::write(const void* data, size_t len) {
    if (failed_) {
        return false;
    }

    const uint8_t* bytes = static_cast<const uint8_t*>(data);
    const size_t capacity = buffer_.capacity();
    if (capacity == 0) {
        if (file_.write(bytes, len) != len) {
            failed_ = true;
        }
        return !failed_;
    }

    if (buffer_.size() + len > capacity) {
        if (!flush()) {
            return false;
        }
        if (len >= capacity) {
            if (file_.write(bytes, len) != len) {
                failed_ = true;
            }
            return !failed_;
        }
    }

    const size_t offset = buffer_.size();
    buffer_.resize(offset + len);
    std::memcpy(buffer_.data() + offset, bytes, len);
    return true;
}

bool BufferedWriter::flush() {
    if (failed_) {
        return false;
    }
    if (buffer_.empty()) {
        return true;
    }
    const bool ok = file_.write(buffer_.data(), buffer_.size()) == buffer_.size();
    buffer_.clear();
    if (!ok) {
        failed_ = true;
        return false;
    }
    return ok;
}

bool BufferedWriter::seek(uint32_t position) {
    return flush() && file_.seek(position);
}

void BufferedWriter::discard() {
    buffer_.clear();
    failed_ = true;
}
