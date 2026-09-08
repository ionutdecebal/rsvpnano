#pragma once

#include <Arduino.h>
#include <FS.h>
#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "reader/ReadingState.h"

class IndexedBookStore {
public:
    struct Header {
        uint32_t magic = 0;
        uint32_t version = 0;
        uint32_t headerSize = 0;
        uint32_t recordSize = 0;
        reading::BookIdentity identity;
        uint32_t paragraphCount = 0;
        uint32_t chapterCount = 0;
        uint32_t recordsOffset = 0;
        uint32_t paragraphsOffset = 0;
        uint32_t chaptersOffset = 0;
        uint32_t dataSize = 0;
        uint32_t textRunCount = 0;
        uint32_t textRunsOffset = 0;
        uint32_t scriptMask = 0;
        uint32_t requiredCapabilities = 0;
        std::array<char, 36> locale{};
        uint8_t baseDirection = 0;
        uint8_t writingMode = 0;
    };

    struct __attribute__((packed)) WordRecord {
        uint32_t offset = 0;
        uint16_t length = 0;
    };

    struct ChapterRecord {
        uint32_t wordIndex = 0;
        uint32_t titleLength = 0;
        char title[64] = {};
    };

    struct __attribute__((packed)) TextRunRecord {
        uint32_t wordIndex = 0;
        uint32_t scriptMask = 0;
        std::array<char, 36> locale{};
        uint8_t direction = 0;
    };

    static constexpr uint32_t kMagic = 0x58444952UL; // RIDX
    static constexpr uint32_t kVersion = 13; // Reindex existing books with em-dash word boundaries.
    static constexpr size_t kWordCacheSize = 256;

    IndexedBookStore() = default;
    IndexedBookStore(const IndexedBookStore&) = delete;
    IndexedBookStore& operator=(const IndexedBookStore&) = delete;

    bool open(std::string_view sourcePath, const Header& header);
    void close();
    void releaseCache();
    bool isOpen() const;

    size_t wordCount() const;
    std::string_view wordAt(size_t index) const;
    void prefetchAround(size_t index) const;

    uint32_t sourceSize() const {
        return isOpen() ? identity_.sourceSize : 0;
    }
    uint32_t sourceFingerprint() const {
        return isOpen() ? identity_.sourceFingerprint : 0;
    }
    const reading::BookIdentity& identity() const {
        return identity_;
    }
    std::string_view sourcePath() const {
        return sourcePath_;
    }

private:
    bool loadWordWindow(size_t index) const;
    bool hasCachedWord(size_t index) const;
    bool readRecords(size_t startIndex, size_t count, std::vector<WordRecord>& records) const;

    std::string sourcePath_;
    reading::BookIdentity identity_;
    uint32_t recordsOffset_ = 0;
    uint32_t dataSize_ = 0;
    mutable File indexFile_;
    mutable File dataFile_;
    mutable std::vector<WordRecord> cachedRecords_;
    mutable std::vector<char> cachedData_;
    mutable size_t cachedStart_ = static_cast<size_t>(-1);
    mutable uint32_t cachedDataStart_ = 0;
};
