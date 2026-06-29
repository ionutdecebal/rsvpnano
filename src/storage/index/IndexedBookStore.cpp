#include "storage/index/IndexedBookStore.h"

#include "board/BoardStorage.h"
#include <algorithm>
#include <limits>

using StoreHeader = IndexedBookStore::Header;
using StoreWordRecord = IndexedBookStore::WordRecord;
using StoreChapterRecord = IndexedBookStore::ChapterRecord;

static_assert(sizeof(StoreHeader) == 52, "RIDX header size changed");
static_assert(sizeof(StoreWordRecord) == 6, "RIDX word record size changed");
static_assert(sizeof(StoreChapterRecord) == 72, "RIDX chapter record size changed");

namespace {

    bool checkedAdd(uint32_t left, uint32_t right, uint32_t& result) {
        if (left > std::numeric_limits<uint32_t>::max() - right) {
            return false;
        }
        result = left + right;
        return true;
    }

    bool validateLayout(const StoreHeader& header, size_t indexBytes, size_t dataBytes) {
        // Reject corrupt sidecars before later reads seek through the files.
        if (header.magic != IndexedBookStore::kMagic || header.version != IndexedBookStore::kVersion
            || header.headerSize != sizeof(StoreHeader) || header.recordSize != sizeof(StoreWordRecord)
            || header.wordCount == 0) {
            return false;
        }

        uint32_t recordsBytes = 0;
        if (header.wordCount > std::numeric_limits<uint32_t>::max() / sizeof(StoreWordRecord)) {
            return false;
        }
        recordsBytes = header.wordCount * sizeof(StoreWordRecord);

        uint32_t recordsEnd = 0;
        uint32_t paragraphsEnd = 0;
        uint32_t chaptersEnd = 0;
        if (!checkedAdd(header.recordsOffset, recordsBytes, recordsEnd)
            || header.paragraphCount > std::numeric_limits<uint32_t>::max() / sizeof(uint32_t)
            || !checkedAdd(header.paragraphsOffset, header.paragraphCount * sizeof(uint32_t), paragraphsEnd)
            || header.chapterCount > std::numeric_limits<uint32_t>::max() / sizeof(StoreChapterRecord)
            || !checkedAdd(header.chaptersOffset, header.chapterCount * sizeof(StoreChapterRecord), chaptersEnd)) {
            return false;
        }

        return header.recordsOffset >= sizeof(StoreHeader) && header.paragraphsOffset == recordsEnd
            && header.chaptersOffset == paragraphsEnd && chaptersEnd <= indexBytes && header.dataSize <= dataBytes;
    }

} // namespace

bool IndexedBookStore::open(const String& indexPath, const String& dataPath, const Header& header) {
    File nextIndexFile = Board::Storage::filesystem().open(indexPath, FILE_READ);
    if (!nextIndexFile || nextIndexFile.isDirectory()) {
        if (nextIndexFile) {
            nextIndexFile.close();
        }
        return false;
    }

    File nextDataFile = Board::Storage::filesystem().open(dataPath, FILE_READ);
    if (!nextDataFile || nextDataFile.isDirectory()) {
        nextIndexFile.close();
        if (nextDataFile) {
            nextDataFile.close();
        }
        return false;
    }

    if (!validateLayout(header, nextIndexFile.size(), nextDataFile.size())) {
        Serial.printf("[storage-index] invalid store layout index=%s data=%s\n", indexPath.c_str(), dataPath.c_str());
        nextIndexFile.close();
        nextDataFile.close();
        return false;
    }

    close();
    indexPath_ = indexPath;
    dataPath_ = dataPath;
    header_ = header;
    indexFile_ = nextIndexFile;
    dataFile_ = nextDataFile;
    cachedStart_ = static_cast<size_t>(-1);
    cachedCount_ = 0;
    cachedWords_.clear();
    return true;
}

void IndexedBookStore::close() {
    if (indexFile_) {
        indexFile_.close();
    }
    if (dataFile_) {
        dataFile_.close();
    }
    indexPath_ = "";
    dataPath_ = "";
    header_ = Header();
    cachedWords_.clear();
    cachedStart_ = static_cast<size_t>(-1);
    cachedCount_ = 0;
}

bool IndexedBookStore::isOpen() const {
    return indexFile_ && dataFile_ && header_.magic == kMagic && header_.wordCount > 0;
}

size_t IndexedBookStore::wordCount() const {
    return isOpen() ? static_cast<size_t>(header_.wordCount) : 0;
}

String IndexedBookStore::wordAt(size_t index) const {
    if (!isOpen() || index >= wordCount()) {
        return "";
    }

    if (cachedStart_ == static_cast<size_t>(-1) || index < cachedStart_ || index >= cachedStart_ + cachedCount_) {
        if (!loadWordWindow(index)) {
            return "";
        }
    }

    return cachedWords_[index - cachedStart_];
}

void IndexedBookStore::prefetchAround(size_t index) const {
    if (!isOpen() || index >= wordCount()) {
        return;
    }
    if (cachedStart_ == static_cast<size_t>(-1) || index < cachedStart_ || index >= cachedStart_ + cachedCount_) {
        (void) loadWordWindow(index);
    }
}

bool IndexedBookStore::readRecords(size_t startIndex, size_t count, std::vector<WordRecord>& records) const {
    records.clear();
    if (!isOpen() || count == 0 || startIndex >= wordCount()) {
        return false;
    }

    const size_t available = wordCount() - startIndex;
    count = std::min(count, available);
    records.resize(count);

    if (startIndex > std::numeric_limits<uint32_t>::max() / sizeof(WordRecord)) {
        records.clear();
        return false;
    }
    uint32_t offset = 0;
    if (!checkedAdd(header_.recordsOffset, static_cast<uint32_t>(startIndex * sizeof(WordRecord)), offset)) {
        records.clear();
        return false;
    }
    if (!indexFile_.seek(offset)) {
        records.clear();
        return false;
    }

    const size_t bytes = count * sizeof(WordRecord);
    const size_t read = indexFile_.read(reinterpret_cast<uint8_t*>(records.data()), bytes);
    if (read != bytes) {
        records.clear();
        return false;
    }

    return true;
}

bool IndexedBookStore::loadWordWindow(size_t index) const {
    if (!isOpen() || index >= wordCount()) {
        return false;
    }

    const size_t start = (index / kWordCacheSize) * kWordCacheSize;
    const size_t count = std::min(kWordCacheSize, wordCount() - start);
    std::vector<WordRecord> records;
    if (!readRecords(start, count, records) || records.empty()) {
        return false;
    }

    // Read one contiguous data range for the cache window.
    const uint32_t dataStart = records.front().offset;
    const WordRecord& last = records.back();
    uint32_t dataEnd = 0;
    if (!checkedAdd(last.offset, last.length, dataEnd)) {
        return false;
    }
    if (dataEnd < dataStart || dataEnd > header_.dataSize) {
        return false;
    }

    const size_t dataBytes = dataEnd - dataStart;
    std::vector<char> buffer(dataBytes);
    if (dataBytes > 0) {
        if (!dataFile_.seek(dataStart)) {
            return false;
        }
        const size_t read = dataFile_.read(reinterpret_cast<uint8_t*>(buffer.data()), dataBytes);
        if (read != dataBytes) {
            return false;
        }
    }

    // Rebuild cached words from record offsets inside the data window.
    cachedWords_.clear();
    cachedWords_.reserve(records.size());
    for (const WordRecord& record: records) {
        uint32_t recordEnd = 0;
        if (record.offset < dataStart || !checkedAdd(record.offset, record.length, recordEnd) || recordEnd > dataEnd) {
            cachedWords_.clear();
            return false;
        }

        const size_t localOffset = record.offset - dataStart;
        String word;
        word.reserve(record.length);
        for (uint16_t i = 0; i < record.length; ++i) {
            word += buffer[localOffset + i];
        }
        cachedWords_.push_back(word);
    }

    cachedStart_ = start;
    cachedCount_ = cachedWords_.size();
    return cachedCount_ > 0;
}
