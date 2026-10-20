#pragma once
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <map>
#include <string>

// In-memory NVS with injected write failures; production SettingsStore is used.
namespace fixture {
inline std::map<std::string, std::string> blobs;
inline std::map<std::string, uint32_t> integers;
inline std::string failingKey;
inline size_t blobWrites = 0;
}
class Preferences {
public:
    bool begin(const char*, bool = false) { return true; }
    void end() {}
    size_t getBytesLength(const char* key) { return fixture::blobs[key].size(); }
    size_t getBytes(const char* key, void* data, size_t size) {
        const auto& value = fixture::blobs[key];
        size = std::min(size, value.size()); std::memcpy(data, value.data(), size); return size;
    }
    size_t putBytes(const char* key, const void* data, size_t size) {
        ++fixture::blobWrites;
        if (fixture::failingKey == key) return 0;
        fixture::blobs[key].assign(static_cast<const char*>(data), size); return size;
    }
    uint32_t getUInt(const char* key, uint32_t fallback) {
        const auto it = fixture::integers.find(key);
        return it == fixture::integers.end() ? fallback : it->second;
    }
    size_t putUInt(const char* key, uint32_t value) { fixture::integers[key] = value; return sizeof(value); }
};
