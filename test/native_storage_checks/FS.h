#pragma once
#include <algorithm>
#include <cstring>
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#define FILE_READ "r"
#define FILE_WRITE "w"

namespace fixture {
struct Node { std::string data; bool directory = false; };
}
class File {
public:
    File() = default;
    explicit File(std::shared_ptr<fixture::Node> node, size_t writeLimit = SIZE_MAX)
        : node_(std::move(node)), writeLimit_(writeLimit) {}
    explicit operator bool() const { return bool(node_); }
    bool isDirectory() const { return node_ && node_->directory; }
    size_t size() const { return node_ ? node_->data.size() : 0; }
    size_t position() const { return position_; }
    bool seek(size_t position) {
        if (position > size()) return false;
        position_ = position; return true;
    }
    size_t read(uint8_t* out, size_t count) {
        count = std::min(count, size() - position_);
        if (count) std::memcpy(out, node_->data.data() + position_, count);
        position_ += count; return count;
    }
    size_t write(const uint8_t* data, size_t count) {
        if (!node_) return 0;
        count = std::min(count, writeLimit_);
        node_->data.append(reinterpret_cast<const char*>(data), count); return count;
    }
    void flush() {}
    void close() { node_.reset(); }
private:
    std::shared_ptr<fixture::Node> node_;
    size_t position_ = 0;
    size_t writeLimit_ = SIZE_MAX;
};
namespace fs {
class FS {
public:
    bool failWrites = false;
    size_t writeOpens = 0;
    size_t writeLimit = SIZE_MAX;
    std::map<std::string, std::shared_ptr<fixture::Node>> nodes;
    File open(const char* path, const char* mode = FILE_READ) {
        if (std::string_view{mode} == FILE_WRITE) {
            ++writeOpens;
            if (failWrites) return {};
            nodes[path] = std::make_shared<fixture::Node>();
        }
        const auto it = nodes.find(path);
        return it == nodes.end() ? File{} : File{it->second, writeLimit};
    }
    bool mkdir(const char* path) {
        if (failWrites) return false;
        nodes[path] = std::make_shared<fixture::Node>(fixture::Node{{}, true}); return true;
    }
    bool remove(const char* path) { return !failWrites && nodes.erase(path); }
    bool rename(const char* from, const char* to) {
        if (failWrites || !nodes.contains(from)) return false;
        nodes[to] = nodes.at(from); nodes.erase(from); return true;
    }
};
}
