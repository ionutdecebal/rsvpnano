#pragma once

#include <Arduino.h>
#include <FS.h>
#include <cstdint>
#include <vector>

#include "reader/BookWordSource.h"

class IndexedBookStore : public BookWordSource {
public:
    struct Header {
        uint32_t magic = 0;
        uint32_t version = 0;
        uint32_t headerSize = 0;
        uint32_t recordSize = 0;
        uint32_t sourceSize = 0;
        uint32_t sourceFingerprint = 0;
        uint32_t wordCount = 0;
        uint32_t paragraphCount = 0;
        uint32_t chapterCount = 0;
        uint32_t recordsOffset = 0;
        uint32_t paragraphsOffset = 0;
        uint32_t chaptersOffset = 0;
        uint32_t dataSize = 0;
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

    static constexpr uint32_t kMagic = 0x58444952UL; // RIDX
    static constexpr uint32_t kVersion = 5;
    static constexpr size_t kWordCacheSize = 256;

    IndexedBookStore() = default;
    IndexedBookStore(const IndexedBookStore&) = delete;
    IndexedBookStore& operator=(const IndexedBookStore&) = delete;

    bool open(const String& indexPath, const String& dataPath, const Header& header);
    void close();
    bool isOpen() const;

    size_t wordCount() const override;
    String wordAt(size_t index) const override;
    void prefetchAround(size_t index) const override;

    const String& indexPath() const {
        return indexPath_;
    }
    const String& dataPath() const {
        return dataPath_;
    }

private:
    bool loadWordWindow(size_t index) const;
    bool readRecords(size_t startIndex, size_t count, std::vector<WordRecord>& records) const;

    String indexPath_;
    String dataPath_;
    Header header_;
    mutable File indexFile_;
    mutable File dataFile_;
    mutable std::vector<String> cachedWords_;
    mutable size_t cachedStart_ = static_cast<size_t>(-1);
    mutable size_t cachedCount_ = 0;
};
