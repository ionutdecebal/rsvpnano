#include "storage/index/IndexedBook.h"
#include <esp_log.h>
#include "logging/Logger.h"

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstdint>
#include <esp_heap_caps.h>
#include <limits>
#include <system_error>
#include "board/BoardStorage.h"

#include "hash/Fnv1a.h"
#include "storage/fs/StorageFiles.h"
#include "storage/fs/StoragePaths.h"
#include "storage/index/BufferedWriter.h"
#include "storage/library/EpubCache.h"
#include "text/LocaleTag.h"
#include "text/RsvpDirectives.h"
#include "text/RsvpTokenizer.h"
#include "text/UnicodeText.h"
#include "text/Utf8Text.h"

#ifndef RSVP_ON_DEVICE_EPUB_CONVERSION
#define RSVP_ON_DEVICE_EPUB_CONVERSION 0
#endif

namespace IndexedBook {

    using IndexHeader = IndexedBookStore::Header;
    using WordRecord = IndexedBookStore::WordRecord;
    using ChapterRecord = IndexedBookStore::ChapterRecord;
    using TextRunRecord = IndexedBookStore::TextRunRecord;

    namespace {

        using namespace StoragePaths;

        constexpr size_t kFingerprintSampleBytes = 512;
        constexpr size_t kParseBufferBytes = 4096;
        constexpr size_t kIndexProgressStepBytes = 256UL * 1024UL;
        constexpr size_t kParseMemoryCheckWordInterval = 512;
        constexpr size_t kParseMinFreeHeapBytes = 32 * 1024;
        constexpr size_t kParseMinLargestHeapBlockBytes = 8 * 1024;

        bool readExact(File& file, void* data, size_t bytes) {
            return file.read(reinterpret_cast<uint8_t*>(data), bytes) == bytes;
        }

        std::error_code ioError(int value) {
            return value == 0 ? std::make_error_code(std::errc::io_error)
                              : std::error_code{value, std::generic_category()};
        }

        bool checkedAdd(uint32_t left, uint32_t right, uint32_t& result) {
            if (left > std::numeric_limits<uint32_t>::max() - right) {
                return false;
            }
            result = left + right;
            return true;
        }

        bool indexHeaderLayoutValid(const IndexHeader& header, size_t indexBytes, size_t dataBytes) {
            uint32_t recordsBytes = 0;
            uint32_t recordsEnd = 0;
            uint32_t paragraphsEnd = 0;
            uint32_t chaptersEnd = 0;
            uint32_t textRunsEnd = 0;
            if (header.identity.wordCount > std::numeric_limits<uint32_t>::max() / sizeof(WordRecord)
                || header.paragraphCount > std::numeric_limits<uint32_t>::max() / sizeof(uint32_t)
                || header.chapterCount > std::numeric_limits<uint32_t>::max() / sizeof(ChapterRecord)
                || header.textRunCount > std::numeric_limits<uint32_t>::max() / sizeof(TextRunRecord)) {
                return false;
            }

            recordsBytes = header.identity.wordCount * sizeof(WordRecord);
            return checkedAdd(header.recordsOffset, recordsBytes, recordsEnd)
                && checkedAdd(header.paragraphsOffset, header.paragraphCount * sizeof(uint32_t), paragraphsEnd)
                && checkedAdd(header.chaptersOffset, header.chapterCount * sizeof(ChapterRecord), chaptersEnd)
                && checkedAdd(header.textRunsOffset, header.textRunCount * sizeof(TextRunRecord), textRunsEnd)
                && header.recordsOffset >= sizeof(IndexHeader) && header.paragraphsOffset == recordsEnd
                && header.chaptersOffset == paragraphsEnd && header.textRunsOffset == chaptersEnd
                && textRunsEnd <= indexBytes && header.dataSize <= dataBytes
                && header.baseDirection <= static_cast<uint8_t>(TextDirection::rtl)
                && header.writingMode <= static_cast<uint8_t>(WritingMode::verticalRl);
        }

        template<size_t Size>
        void storeFixedString(std::array<char, Size>& destination, std::string_view value) {
            const size_t length = std::min(value.size(), Size - 1);
            std::ranges::copy(value.substr(0, length), destination.begin());
        }

        template<size_t Size>
        std::string loadFixedString(const std::array<char, Size>& source) {
            const auto end = std::ranges::find(source, '\0');
            return {source.begin(), end};
        }

        std::array<uint32_t, 3> fingerprintOffsets(uint32_t sourceSize) {
            const bool hasFullSample = sourceSize > kFingerprintSampleBytes;
            const uint32_t sampleBytes = static_cast<uint32_t>(kFingerprintSampleBytes);
            return {
                0,
                hasFullSample ? sourceSize / 2 : 0,
                hasFullSample ? sourceSize - sampleBytes : 0,
            };
        }

        uint32_t combineFingerprint(uint32_t sourceSize, const std::array<uint32_t, 3>& samples) {
            const std::array<uint8_t, 4> sizeBytes = {{
                static_cast<uint8_t>(sourceSize & 0xFF),
                static_cast<uint8_t>((sourceSize >> 8) & 0xFF),
                static_cast<uint8_t>((sourceSize >> 16) & 0xFF),
                static_cast<uint8_t>((sourceSize >> 24) & 0xFF),
            }};
            uint32_t hash = Fnv1a::append(Fnv1a::kOffsetBasis, sizeBytes);
            for (const uint32_t sample: samples) {
                const std::array<uint8_t, 4> sampleBytes = {{
                    static_cast<uint8_t>(sample & 0xFF),
                    static_cast<uint8_t>((sample >> 8) & 0xFF),
                    static_cast<uint8_t>((sample >> 16) & 0xFF),
                    static_cast<uint8_t>((sample >> 24) & 0xFF),
                }};
                hash = Fnv1a::append(hash, sampleBytes);
            }
            return hash;
        }

        uint32_t sourceFingerprint(File& file, uint32_t sourceSize) {
            uint8_t buffer[kFingerprintSampleBytes];
            std::array<uint32_t, 3> samples{};
            samples.fill(Fnv1a::kOffsetBasis);
            const auto offsets = fingerprintOffsets(sourceSize);
            for (size_t index = 0; index < offsets.size(); ++index) {
                const uint32_t offset = offsets[index];
                if (!file.seek(offset)) {
                    return 0;
                }
                const size_t wanted =
                    static_cast<size_t>(std::min<uint32_t>(kFingerprintSampleBytes, sourceSize - offset));
                const size_t read = file.read(buffer, wanted);
                if (read != wanted) {
                    return 0;
                }
                samples[index] = Fnv1a::append(samples[index], std::span{buffer, read});
            }
            return combineFingerprint(sourceSize, samples);
        }

        bool readIndexHeader(std::string_view path, IndexHeader& header) {
            const std::string indexPath = indexedIndexPathFor(path);
            File file = Board::Storage::filesystem().open(indexPath.c_str(), FILE_READ);
            if (!file) {
                return false;
            }

            if (file.isDirectory()) {
                file.close();
                return false;
            }

            const bool ok = readExact(file, &header, sizeof(header));
            file.close();

            return ok && header.magic == IndexedBookStore::kMagic && header.version == IndexedBookStore::kVersion
                && header.headerSize == sizeof(IndexHeader) && header.recordSize == sizeof(WordRecord)
                && header.recordsOffset >= sizeof(IndexHeader);
        }

        bool parseMemoryLow() {
            return heap_caps_get_free_size(MALLOC_CAP_8BIT) < kParseMinFreeHeapBytes
                || heap_caps_get_largest_free_block(MALLOC_CAP_8BIT) < kParseMinLargestHeapBlockBytes;
        }

    } // namespace

    Builder::Builder(fs::FS& filesystem, std::string_view sourcePath, uint32_t sourceSize, bool rsvpFormat) :
            filesystem_(filesystem), temporaryIndexPath_(indexedTempPathFor(indexedIndexPathFor(sourcePath))),
            temporaryDataPath_(indexedTempPathFor(indexedDataPathFor(sourcePath))), indexWriter_(indexFile_),
            dataWriter_(dataFile_), sourceSize_(sourceSize), rsvpFormat_(rsvpFormat) {
        line_.reserve(256);
        sampleOffsets_ = fingerprintOffsets(sourceSize_);
        sampleHashes_.fill(Fnv1a::kOffsetBasis);
    }

    Builder::~Builder() {
        cleanup();
    }

    std::expected<void, std::error_code> Builder::fail(std::error_code error, const char* detail) {
        if (!error_) {
            error_ = error;
            failure_ = detail;
        }
        return std::unexpected(error_);
    }

    std::expected<void, std::error_code> Builder::begin() {
        if (begun_ || sourceSize_ == 0)
            return fail(std::make_error_code(std::errc::invalid_argument), "No readable words");
        if (!StorageFiles::directoryExists(parentDirectoryForPath(temporaryIndexPath_).c_str()))
            return fail(std::make_error_code(std::errc::no_such_file_or_directory), "Folder missing");

        filesystem_.remove(temporaryIndexPath_.c_str());
        filesystem_.remove(temporaryDataPath_.c_str());

        errno = 0;
        indexFile_ = filesystem_.open(temporaryIndexPath_.c_str(), FILE_WRITE);
        if (!indexFile_) {
            const std::error_code error = ioError(errno);
            Logger::failure("storage-index", "open index FILE_WRITE", temporaryIndexPath_.c_str(), error);
            return fail(error, "SD write failed");
        }
        errno = 0;
        dataFile_ = filesystem_.open(temporaryDataPath_.c_str(), FILE_WRITE);
        if (!dataFile_) {
            const std::error_code error = ioError(errno);
            Logger::failure("storage-index", "open data FILE_WRITE", temporaryDataPath_.c_str(), error);
            return fail(error, "SD write failed");
        }

        if (auto written = indexWriter_.write(&header_, sizeof(header_)); !written)
            return fail(written.error(), "SD write failed");
        begun_ = true;
        return {};
    }

    std::expected<void, std::error_code> Builder::append(std::span<const uint8_t> bytes) {
        if (!begun_ || finished_ || error_)
            return fail(error_ ? error_ : std::make_error_code(std::errc::operation_not_permitted),
                        "Index builder is not writable");
        if (bytes.size() > sourceSize_ - receivedBytes_)
            return fail(std::make_error_code(std::errc::value_too_large), "Source size changed");

        updateFingerprint(bytes);
        receivedBytes_ += static_cast<uint32_t>(bytes.size());
        if (parsingComplete_)
            return {};

        for (const uint8_t byte: bytes) {
            const char character = static_cast<char>(byte);
            if (character == '\r')
                continue;
            if (character == '\n') {
                if (!processLine(line_)) {
                    line_.clear();
                    if (error_)
                        return std::unexpected(error_);
                    parsingComplete_ = true;
                    ESP_LOGD("storage-index", "Reached %lu word limit, truncating book",
                             static_cast<unsigned long>(RsvpText::kMaxBookWords));
                    break;
                }
                line_.clear();
                continue;
            }

            line_ += character;
            if (line_.size() >= RsvpText::kMaxBookLineChars) {
                if (!processLine(line_)) {
                    line_.clear();
                    if (error_)
                        return std::unexpected(error_);
                    parsingComplete_ = true;
                    ESP_LOGD("storage-index", "Reached %lu word limit, truncating book",
                             static_cast<unsigned long>(RsvpText::kMaxBookWords));
                    break;
                }
                ++stats_.longLineSplits;
                line_.clear();
            }
        }
        yield();
        return {};
    }

    std::expected<void, std::error_code> Builder::finish() {
        if (!begun_ || finished_ || error_)
            return fail(error_ ? error_ : std::make_error_code(std::errc::operation_not_permitted),
                        "Index builder cannot finish");
        if (receivedBytes_ != sourceSize_)
            return fail(std::make_error_code(std::errc::io_error), "Source read failed");
        if (!parsingComplete_ && !line_.empty() && !processLine(line_) && error_)
            return std::unexpected(error_);
        line_.clear();

        if (wordCount_ == 0)
            return fail(std::make_error_code(std::errc::invalid_argument), "No readable words");
        if (metadata_.paragraphStarts.empty())
            metadata_.paragraphStarts.push_back(0);
        if (wordCount_ > UINT32_MAX || metadata_.paragraphStarts.size() > UINT32_MAX
            || metadata_.chapters.size() > UINT32_MAX || metadata_.textRuns.size() > UINT32_MAX) {
            return fail(std::make_error_code(std::errc::value_too_large), "Index limit reached");
        }

        header_.magic = IndexedBookStore::kMagic;
        header_.version = IndexedBookStore::kVersion;
        header_.headerSize = sizeof(IndexHeader);
        header_.recordSize = sizeof(WordRecord);
        header_.identity.sourceSize = sourceSize_;
        header_.identity.sourceFingerprint = fingerprint();
        header_.identity.wordCount = static_cast<uint32_t>(wordCount_);
        header_.paragraphCount = static_cast<uint32_t>(metadata_.paragraphStarts.size());
        header_.chapterCount = static_cast<uint32_t>(metadata_.chapters.size());
        header_.textRunCount = static_cast<uint32_t>(metadata_.textRuns.size());
        header_.recordsOffset = sizeof(IndexHeader);
        header_.paragraphsOffset = header_.recordsOffset + header_.identity.wordCount * sizeof(WordRecord);
        header_.chaptersOffset = header_.paragraphsOffset + header_.paragraphCount * sizeof(uint32_t);
        header_.textRunsOffset = header_.chaptersOffset + header_.chapterCount * sizeof(ChapterRecord);
        header_.dataSize = dataSize_;
        header_.scriptMask = metadata_.scriptMask;
        header_.requiredCapabilities = metadata_.requiredCapabilities;
        storeFixedString(header_.locale, metadata_.locale);
        header_.baseDirection = static_cast<uint8_t>(metadata_.baseDirection);
        header_.writingMode = static_cast<uint8_t>(metadata_.writingMode);

        for (const size_t paragraph: metadata_.paragraphStarts) {
            const uint32_t wordIndex = static_cast<uint32_t>(paragraph);
            if (auto written = indexWriter_.write(&wordIndex, sizeof(wordIndex)); !written)
                return fail(written.error(), "SD write failed");
        }
        for (const ChapterMarker& chapter: metadata_.chapters) {
            ChapterRecord record;
            record.wordIndex = static_cast<uint32_t>(chapter.wordIndex);
            record.titleLength = std::min<uint32_t>(chapter.title.size(), sizeof(record.title));
            std::ranges::copy_n(chapter.title.begin(), record.titleLength, record.title);
            if (auto written = indexWriter_.write(&record, sizeof(record)); !written)
                return fail(written.error(), "SD write failed");
        }
        for (const BookTextRun& run: metadata_.textRuns) {
            TextRunRecord record;
            record.wordIndex = static_cast<uint32_t>(run.wordIndex);
            record.scriptMask = run.scriptMask;
            storeFixedString(record.locale, run.locale);
            record.direction = static_cast<uint8_t>(run.direction);
            if (auto written = indexWriter_.write(&record, sizeof(record)); !written)
                return fail(written.error(), "SD write failed");
        }
        if (auto seek = indexWriter_.seek(0); !seek)
            return fail(seek.error(), "SD write failed");
        if (auto written = indexWriter_.write(&header_, sizeof(header_)); !written)
            return fail(written.error(), "SD write failed");
        if (auto flushed = indexWriter_.flush(); !flushed)
            return fail(flushed.error(), "SD write failed");
        if (auto flushed = dataWriter_.flush(); !flushed)
            return fail(flushed.error(), "SD write failed");
        indexFile_.flush();
        dataFile_.flush();
        indexFile_.close();
        dataFile_.close();

        if (stats_.longLineSplits > 0 || stats_.normalization.malformedUtf8 > 0
            || stats_.normalization.nonAsciiCodepoints > 0) {
            ESP_LOGD("storage-index", "Parse cleanup: long_lines=%u malformed_utf8=%u non_ascii=%u",
                     static_cast<unsigned int>(stats_.longLineSplits),
                     static_cast<unsigned int>(stats_.normalization.malformedUtf8),
                     static_cast<unsigned int>(stats_.normalization.nonAsciiCodepoints));
        }
        finished_ = true;
        return {};
    }

    std::expected<void, std::error_code> Builder::commit() {
        if (!finished_ || committed_ || error_)
            return fail(error_ ? error_ : std::make_error_code(std::errc::operation_not_permitted),
                        "Index builder cannot commit");

        std::string indexPath = temporaryIndexPath_;
        std::string dataPath = temporaryDataPath_;
        indexPath.resize(indexPath.size() - std::string_view{kTempExtension}.size());
        dataPath.resize(dataPath.size() - std::string_view{kTempExtension}.size());
        filesystem_.remove(indexPath.c_str());
        filesystem_.remove(dataPath.c_str());
        errno = 0;
        if (!filesystem_.rename(temporaryDataPath_.c_str(), dataPath.c_str())) {
            const std::error_code error = ioError(errno);
            Logger::failure("storage-index", "rename data", temporaryDataPath_.c_str(), dataPath.c_str(), error);
            return fail(error, "Rename failed");
        }
        errno = 0;
        if (!filesystem_.rename(temporaryIndexPath_.c_str(), indexPath.c_str())) {
            const std::error_code error = ioError(errno);
            Logger::failure("storage-index", "rename index", temporaryIndexPath_.c_str(), indexPath.c_str(), error);
            filesystem_.remove(dataPath.c_str());
            return fail(error, "Rename failed");
        }
        committed_ = true;
        return {};
    }

    const IndexHeader& Builder::header() const noexcept {
        return header_;
    }

    BookMetadata Builder::takeMetadata() {
        return std::move(metadata_);
    }

    const char* Builder::failure() const noexcept {
        return failure_;
    }

    bool Builder::processLine(std::string_view line) {
        return rsvpFormat_ ? processRsvpLine(line) : processBookLine(line);
    }

    bool Builder::processBookLine(std::string_view line) {
        const std::string_view trimmed = RsvpText::stripBom(line);
        if (trimmed.empty()) {
            paragraphPending_ = true;
            return true;
        }
        std::string chapterTitle;
        if (RsvpText::chapterTitleFromLine(line, chapterTitle)) {
            addChapter(chapterTitle);
            paragraphPending_ = true;
        }
        if (paragraphPending_) {
            addParagraph();
            paragraphPending_ = false;
        }
        return appendLineWords(line);
    }

    bool Builder::processRsvpLine(std::string_view line) {
        std::string trimmed{RsvpText::stripBom(line)};
        if (trimmed.empty()) {
            paragraphPending_ = true;
            return true;
        }
        if (trimmed.starts_with("@@")) {
            trimmed.erase(0, 1);
            if (paragraphPending_) {
                addParagraph();
                paragraphPending_ = false;
            }
            return appendLineWords(trimmed);
        }
        if (trimmed.starts_with('@')) {
            if (RsvpText::prefixHasBoundary(trimmed, "@para")) {
                paragraphPending_ = true;
                return true;
            }
            if (RsvpText::prefixHasBoundary(trimmed, "@chapter")) {
                std::string title = RsvpText::directiveValue(trimmed, "@chapter");
                addChapter(title.empty() ? "Chapter" : title);
                paragraphPending_ = true;
                return true;
            }
            if (RsvpText::prefixHasBoundary(trimmed, "@title") || RsvpText::prefixHasBoundary(trimmed, "@author"))
                return true;
            if (RsvpText::prefixHasBoundary(trimmed, "@language")) {
                const std::string value = RsvpText::directiveValue(trimmed, "@language");
                if (value == "auto") {
                    locale_.clear();
                } else if (auto locale = LocaleTag::normalize(value)) {
                    locale_ = std::move(*locale);
                    if (wordCount_ == 0 && metadata_.locale.empty())
                        metadata_.locale = locale_;
                } else {
                    ESP_LOGW("storage-index", "ignoring invalid @language: %s", value.c_str());
                    return true;
                }
                addTextRun();
                return true;
            }
            if (RsvpText::prefixHasBoundary(trimmed, "@direction")) {
                const std::string value = RsvpText::directiveValue(trimmed, "@direction");
                const auto direction = textDirection(value);
                if (!direction) {
                    ESP_LOGW("storage-index", "ignoring invalid @direction: %s", value.c_str());
                    return true;
                }
                direction_ = *direction;
                if (wordCount_ == 0)
                    metadata_.baseDirection = *direction;
                addTextRun();
                return true;
            }
            if (RsvpText::prefixHasBoundary(trimmed, "@writing-mode")) {
                const std::string value = RsvpText::directiveValue(trimmed, "@writing-mode");
                if (const auto mode = ::writingMode(value))
                    metadata_.writingMode = *mode;
                else
                    ESP_LOGW("storage-index", "ignoring invalid @writing-mode: %s", value.c_str());
                return true;
            }
            return true;
        }
        if (paragraphPending_) {
            addParagraph();
            paragraphPending_ = false;
        }
        return appendLineWords(line);
    }

    bool Builder::appendLineWords(std::string_view line) {
        return RsvpText::appendLineWords(
            line,
            [this](const std::string& token) {
                return pushWord(token);
            },
            wordCount_, &stats_);
    }

    bool Builder::pushWord(std::string token) {
        if (token.empty() || (!RsvpText::hasReadableText(token) && token != "-"))
            return true;
        if (token.size() > UINT16_MAX || dataSize_ > UINT32_MAX - static_cast<uint32_t>(token.size())) {
            fail(std::make_error_code(std::errc::value_too_large), "Index limit reached");
            return false;
        }
        if ((wordCount_ % kParseMemoryCheckWordInterval) == 0 && wordCount_ > 0 && parseMemoryLow()) {
            stats_.memoryLow = true;
            fail(std::make_error_code(std::errc::not_enough_memory), "Memory limit reached");
            return false;
        }
        if (metadata_.textRuns.empty() && !locale_.empty())
            addTextRun();

        const WordRecord record{.offset = dataSize_, .length = static_cast<uint16_t>(token.size())};
        if (auto written = dataWriter_.write(token.data(), token.size()); !written) {
            fail(written.error(), "SD write failed");
            return false;
        }
        if (auto written = indexWriter_.write(&record, sizeof(record)); !written) {
            fail(written.error(), "SD write failed");
            return false;
        }

        dataSize_ += static_cast<uint32_t>(token.size());
        ++wordCount_;
        uint32_t scripts = 0;
        uint32_t codepoint = 0;
        std::string_view remaining = token;
        while (Utf8Text::next(remaining, codepoint)) {
            scripts |= UnicodeText::scriptMask(codepoint);
            metadata_.requiredCapabilities |= UnicodeText::capabilityMask(codepoint);
        }
        metadata_.scriptMask |= scripts;
        if (!metadata_.textRuns.empty())
            metadata_.textRuns.back().scriptMask |= scripts;
        return true;
    }

    void Builder::addChapter(std::string_view title) {
        if (title.empty())
            return;
        ChapterMarker marker{.title = std::string{title}, .wordIndex = wordCount_};
        if (!metadata_.chapters.empty() && metadata_.chapters.back().wordIndex == wordCount_)
            metadata_.chapters.back() = std::move(marker);
        else
            metadata_.chapters.push_back(std::move(marker));
    }

    void Builder::addParagraph() {
        if (metadata_.paragraphStarts.empty() || metadata_.paragraphStarts.back() != wordCount_)
            metadata_.paragraphStarts.push_back(wordCount_);
    }

    void Builder::addTextRun() {
        BookTextRun run{.wordIndex = wordCount_, .locale = locale_, .direction = direction_};
        if (!metadata_.textRuns.empty() && metadata_.textRuns.back().wordIndex == wordCount_)
            metadata_.textRuns.back() = std::move(run);
        else
            metadata_.textRuns.push_back(std::move(run));
    }

    void Builder::updateFingerprint(std::span<const uint8_t> bytes) {
        const uint32_t chunkStart = receivedBytes_;
        const uint32_t chunkEnd = chunkStart + static_cast<uint32_t>(bytes.size());
        for (size_t index = 0; index < sampleOffsets_.size(); ++index) {
            const uint32_t sampleStart = sampleOffsets_[index];
            const uint32_t sampleEnd = std::min<uint32_t>(sourceSize_, sampleStart + kFingerprintSampleBytes);
            const uint32_t overlapStart = std::max(chunkStart, sampleStart);
            const uint32_t overlapEnd = std::min(chunkEnd, sampleEnd);
            if (overlapStart < overlapEnd) {
                sampleHashes_[index] = Fnv1a::append(
                    sampleHashes_[index],
                    bytes.subspan(overlapStart - chunkStart, overlapEnd - overlapStart));
            }
        }
    }

    uint32_t Builder::fingerprint() const {
        return combineFingerprint(sourceSize_, sampleHashes_);
    }

    void Builder::cleanup() noexcept {
        indexWriter_.discard();
        dataWriter_.discard();
        if (indexFile_)
            indexFile_.close();
        if (dataFile_)
            dataFile_.close();
        if (!committed_) {
            filesystem_.remove(temporaryIndexPath_.c_str());
            filesystem_.remove(temporaryDataPath_.c_str());
        }
    }

    namespace {

        bool readIndexedMetadata(std::string_view path, BookMetadata& metadata, IndexHeader* headerOut = nullptr) {
            metadata.clear();
            const std::string sourcePath{path};
            const std::string indexPath = indexedIndexPathFor(path);
            const std::string dataPath = indexedDataPathFor(path);

            IndexHeader header;
            if (!readIndexHeader(path, header)) {
                if (StorageFiles::fileExistsWithBytes(indexPath.c_str())) {
                    ESP_LOGW("storage-index", "invalid index header: %s", indexPath.c_str());
                }
                return false;
            }

            {
                // Validate that the sidecar still matches the source file.
                File source = Board::Storage::filesystem().open(sourcePath.c_str(), FILE_READ);
                if (!source || source.isDirectory()) {
                    if (source) {
                        source.close();
                    }
                    ESP_LOGW("storage-index", "source missing while validating index: %s", sourcePath.c_str());
                    return false;
                }

                const size_t sourceBytes = source.size();
                const uint32_t actualFingerprint =
                    sourceBytes <= UINT32_MAX ? sourceFingerprint(source, static_cast<uint32_t>(sourceBytes)) : 0;
                source.close();
                if (sourceBytes > UINT32_MAX || header.identity.sourceSize != static_cast<uint32_t>(sourceBytes)
                    || header.identity.sourceFingerprint != actualFingerprint) {
                    ESP_LOGW("storage-index", "stale index: %s size=%lu/%lu fingerprint=%08lx/%08lx",
                             sourcePath.c_str(), static_cast<unsigned long>(header.identity.sourceSize),
                             static_cast<unsigned long>(sourceBytes),
                             static_cast<unsigned long>(header.identity.sourceFingerprint),
                             static_cast<unsigned long>(actualFingerprint));
                    return false;
                }
            }

            {
                // Ensure the word data sidecar is present and large enough.
                File data = Board::Storage::filesystem().open(dataPath.c_str(), FILE_READ);
                if (!data || data.isDirectory() || data.size() < header.dataSize) {
                    const size_t dataBytes = data ? data.size() : 0;
                    if (data) {
                        data.close();
                    }
                    ESP_LOGW("storage-index", "data sidecar invalid: %s size=%lu expected=%lu", dataPath.c_str(),
                             static_cast<unsigned long>(dataBytes), static_cast<unsigned long>(header.dataSize));
                    return false;
                }
                data.close();
            }

            File indexFile = Board::Storage::filesystem().open(indexPath.c_str(), FILE_READ);
            if (!indexFile || indexFile.isDirectory()) {
                if (indexFile) {
                    indexFile.close();
                }
                ESP_LOGE("storage-index", "index sidecar cannot reopen: %s", indexPath.c_str());
                return false;
            }

            // Validate index table offsets before loading paragraph/chapter metadata.
            File dataFile = Board::Storage::filesystem().open(dataPath.c_str(), FILE_READ);
            const size_t dataBytes = dataFile ? dataFile.size() : 0;
            if (dataFile) {
                dataFile.close();
            }
            if (!indexHeaderLayoutValid(header, indexFile.size(), dataBytes)) {
                indexFile.close();
                metadata.clear();
                ESP_LOGW("storage-index", "index layout invalid: %s", indexPath.c_str());
                return false;
            }

            metadata.locale = loadFixedString(header.locale);
            if (!metadata.locale.empty()) {
                const auto normalized = LocaleTag::normalize(metadata.locale);
                if (!normalized || *normalized != metadata.locale) {
                    indexFile.close();
                    metadata.clear();
                    ESP_LOGW("storage-index", "index locale invalid: %s", indexPath.c_str());
                    return false;
                }
            }
            metadata.baseDirection = static_cast<TextDirection>(header.baseDirection);
            metadata.scriptMask = header.scriptMask;
            metadata.requiredCapabilities = header.requiredCapabilities;
            metadata.writingMode = static_cast<WritingMode>(header.writingMode);
            if (header.paragraphCount > 0) {
                metadata.paragraphStarts.reserve(header.paragraphCount);
                if (!indexFile.seek(header.paragraphsOffset)) {
                    indexFile.close();
                    metadata.clear();
                    ESP_LOGE("storage-index", "paragraph section seek failed: %s offset=%lu", indexPath.c_str(),
                             static_cast<unsigned long>(header.paragraphsOffset));
                    return false;
                }
                for (uint32_t i = 0; i < header.paragraphCount; ++i) {
                    uint32_t wordIndex = 0;
                    if (!readExact(indexFile, &wordIndex, sizeof(wordIndex))) {
                        indexFile.close();
                        metadata.clear();
                        ESP_LOGE("storage-index", "paragraph section read failed: %s item=%lu", indexPath.c_str(),
                                 static_cast<unsigned long>(i));
                        return false;
                    }
                    metadata.paragraphStarts.push_back(wordIndex);
                }
            }

            if (header.chapterCount > 0) {
                metadata.chapters.reserve(header.chapterCount);
                if (!indexFile.seek(header.chaptersOffset)) {
                    indexFile.close();
                    metadata.clear();
                    ESP_LOGE("storage-index", "chapter section seek failed: %s offset=%lu", indexPath.c_str(),
                             static_cast<unsigned long>(header.chaptersOffset));
                    return false;
                }
                for (uint32_t i = 0; i < header.chapterCount; ++i) {
                    ChapterRecord record;
                    if (!readExact(indexFile, &record, sizeof(record))) {
                        indexFile.close();
                        metadata.clear();
                        ESP_LOGE("storage-index", "chapter section read failed: %s item=%lu", indexPath.c_str(),
                                 static_cast<unsigned long>(i));
                        return false;
                    }
                    ChapterMarker marker;
                    marker.wordIndex = record.wordIndex;
                    const uint32_t titleLength = std::min<uint32_t>(record.titleLength, sizeof(record.title));
                    marker.title.assign(record.title, titleLength);
                    metadata.chapters.push_back(marker);
                }
            }

            if (header.textRunCount > 0) {
                metadata.textRuns.reserve(header.textRunCount);
                if (!indexFile.seek(header.textRunsOffset)) {
                    indexFile.close();
                    metadata.clear();
                    ESP_LOGE("storage-index", "text run section seek failed: %s offset=%lu", indexPath.c_str(),
                             static_cast<unsigned long>(header.textRunsOffset));
                    return false;
                }
                for (uint32_t i = 0; i < header.textRunCount; ++i) {
                    TextRunRecord record;
                    if (!readExact(indexFile, &record, sizeof(record))) {
                        indexFile.close();
                        metadata.clear();
                        ESP_LOGE("storage-index", "text run section read failed: %s item=%lu", indexPath.c_str(),
                                 static_cast<unsigned long>(i));
                        return false;
                    }
                    BookTextRun run{.wordIndex = record.wordIndex,
                                    .locale = loadFixedString(record.locale),
                                    .direction = static_cast<TextDirection>(record.direction),
                                    .scriptMask = record.scriptMask};
                    const auto normalized = LocaleTag::normalize(run.locale);
                    if (run.wordIndex > header.identity.wordCount
                        || record.direction > static_cast<uint8_t>(TextDirection::rtl)
                        || (!run.locale.empty() && (!normalized || *normalized != run.locale))) {
                        indexFile.close();
                        metadata.clear();
                        ESP_LOGW("storage-index", "text run invalid: %s item=%lu", indexPath.c_str(),
                                 static_cast<unsigned long>(i));
                        return false;
                    }
                    metadata.textRuns.push_back(std::move(run));
                }
            }

            indexFile.close();
            if (header.identity.wordCount > 0 && metadata.paragraphStarts.empty()) {
                metadata.paragraphStarts.push_back(0);
            }
            if (headerOut != nullptr) {
                *headerOut = header;
            }
            if (header.identity.wordCount == 0) {
                ESP_LOGW("storage-index", "index has no words: %s", indexPath.c_str());
                return false;
            }
            return true;
        }

        bool build(std::string_view path, BookMetadata& metadata, IndexHeader& header, bool rsvpFormat,
                   StatusCallback statusCallback, void* statusContext) {
            metadata.clear();
            const std::string sourcePath{path};
            const std::string label = displayNameForPath(path);
            auto report = [&](const char* title, const char* line1 = "", const char* line2 = "",
                              int progressPercent = -1) {
                if (statusCallback != nullptr)
                    statusCallback(statusContext, title, line1, line2, progressPercent);
            };

            File source = Board::Storage::filesystem().open(sourcePath.c_str(), FILE_READ);
            if (!source || source.isDirectory()) {
                if (source)
                    source.close();
                ESP_LOGE("storage-index", "cannot open source: %s", sourcePath.c_str());
                report("Index failed", label.c_str(), "File unreadable", 100);
                return false;
            }

            const size_t sourceBytes = source.size();
            if (sourceBytes == 0 || sourceBytes > UINT32_MAX) {
                source.close();
                ESP_LOGW("storage-index", "unsupported source size: %s (%lu bytes)", sourcePath.c_str(),
                         static_cast<unsigned long>(sourceBytes));
                report("Index failed", label.c_str(), sourceBytes == 0 ? "No readable words" : "Book too large", 100);
                return false;
            }

            Builder builder{Board::Storage::filesystem(), path, static_cast<uint32_t>(sourceBytes), rsvpFormat};
            if (auto begun = builder.begin(); !begun) {
                source.close();
                report("Index failed", label.c_str(), builder.failure(), 100);
                return false;
            }

            report("Indexing book", label.c_str(), "Building word index", 0);
            const uint32_t startedMs = millis();
            static std::array<uint8_t, kParseBufferBytes> buffer;
            size_t totalBytesRead = 0;
            size_t nextProgressBytes = 0;
            while (source.available()) {
                const size_t bytesRead = source.read(buffer.data(), buffer.size());
                if (bytesRead == 0)
                    break;
                totalBytesRead += bytesRead;

                if (totalBytesRead >= nextProgressBytes) {
                    const int progress =
                        static_cast<int>(std::min<size_t>(90, (totalBytesRead * 90UL) / sourceBytes));
                    report("Indexing book", label.c_str(), "Building word index", progress);
                    nextProgressBytes = totalBytesRead + kIndexProgressStepBytes;
                }
                if (auto appended = builder.append(std::span{buffer.data(), bytesRead}); !appended) {
                    source.close();
                    report("Index failed", label.c_str(), builder.failure(), 100);
                    return false;
                }
            }
            source.close();

            if (auto finished = builder.finish(); !finished) {
                report("Index failed", label.c_str(), builder.failure(), 100);
                return false;
            }
            if (auto committed = builder.commit(); !committed) {
                report("Index failed", label.c_str(), builder.failure(), 100);
                return false;
            }

            header = builder.header();
            metadata = builder.takeMetadata();
            ESP_LOGI("storage-index", "Built %u words, %u chapters from %s in %lu ms",
                     static_cast<unsigned int>(header.identity.wordCount),
                     static_cast<unsigned int>(metadata.chapters.size()), sourcePath.c_str(),
                     static_cast<unsigned long>(millis() - startedMs));
            report("Index ready", label.c_str(), "Book ready", 100);
            return true;
        }

    } // namespace

    bool load(size_t index, BookLibrary::Listing& library, IndexedBookStore& store, BookMetadata& metadata,
              const OpenRequest& request) {
        metadata.clear();
        auto report = [&](const char* title, const char* line1 = "", const char* line2 = "", int progressPercent = -1) {
            if (request.statusCallback != nullptr) {
                request.statusCallback(request.statusContext, title, line1, line2, progressPercent);
            }
        };

        std::string path;
        size_t parsedIndex = index;

        {
            // Library selection and EPUB preparation.
            if (!StorageFiles::directoryExists(kLibraryPath)) {
                ESP_LOGE("storage", "/library directory not found");
                report("Book open failed", "Folders missing", "Run SD check", 100);
                return false;
            }

            if (library.empty()) {
                BookLibrary::refresh(library, false, RSVP_ON_DEVICE_EPUB_CONVERSION);
            }
            if (library.empty()) {
                ESP_LOGD("storage", "No readable .rsvp, .txt, or .epub books found under /library");
                report("Book open failed", "No books found", "Add books to SD", 100);
                return false;
            }

            if (index >= library.size()) {
                ESP_LOGW("storage", "Book index %u out of range", static_cast<unsigned int>(index));
                report("Book open failed", "Library changed", "Open list again", 100);
                return false;
            }

            path = library[index].path;
            if (hasEpubExtension(path)) {
                if (!request.allowEpubConversion) {
                    report("Index needed", displayNameForPath(path).c_str(), "Open from library", 100);
                    return false;
                }

                auto rsvpPath = EpubCache::ensureConverted(path, request.statusCallback, request.statusContext);
                if (!rsvpPath) {
                    return false;
                }

                BookLibrary::refresh(library, true, RSVP_ON_DEVICE_EPUB_CONVERSION);
                const int convertedIndex = BookLibrary::indexOfPath(library, *rsvpPath);
                if (convertedIndex < 0) {
                    ESP_LOGE("storage", "Converted RSVP not found in refreshed library: %s", rsvpPath->c_str());
                    report("Book open failed", displayNameForPath(path).c_str(), "Conversion cache missing", 100);
                    return false;
                }

                path = *rsvpPath;
                parsedIndex = static_cast<size_t>(convertedIndex);
            }
        }

        {
            // Source readability check.
            File entry = Board::Storage::filesystem().open(path.c_str(), FILE_READ);
            if (!entry || entry.isDirectory()) {
                if (entry) {
                    entry.close();
                }
                ESP_LOGE("storage", "selected book is not readable: %s", path.c_str());
                report("Book open failed", displayNameForPath(path).c_str(), "File unreadable", 100);
                return false;
            }
            entry.close();
        }

        {
            // Index validation, optional rebuild, and store open.
            IndexHeader header;
            auto ensureIndexedBook = [&]() -> bool {
                if (readIndexedMetadata(path, metadata, &header)) {
                    report("Opening book", displayNameForPath(path).c_str(), "Index is current", 45);
                    return true;
                }

                if (!request.allowIndexBuild) {
                    report("Index needed", displayNameForPath(path).c_str(), "Open from library", 100);
                    return false;
                }

                ESP_LOGW("storage-index", "rebuilding missing/stale index: %s", path.c_str());
                report("Opening book", displayNameForPath(path).c_str(), "Index needs rebuild", 20);
                if (!build(path, metadata, header, hasRsvpExtension(path), request.statusCallback,
                           request.statusContext)) {
                    return false;
                }
                return true;
            };

            report("Opening book", displayNameForPath(path).c_str(), "Checking index", 12);
            if (!ensureIndexedBook()) {
                metadata.clear();
                return false;
            }

            report("Opening book", displayNameForPath(path).c_str(), "Opening word cache", 80);
            if (!store.open(path, header)) {
                metadata.clear();
                report("Book open failed", displayNameForPath(path).c_str(), "Index unreadable", 100);
                return false;
            }
        }

        BookLibrary::refreshMetadata(library[parsedIndex]);
        ESP_LOGI("storage", "Opened indexed book %s: %u words, %u chapters", path.c_str(),
                 static_cast<unsigned int>(store.wordCount()), static_cast<unsigned int>(metadata.chapters.size()));
        return true;
    }

    bool readMetadata(std::string_view path, BookMetadata& metadata, IndexedBookStore::Header* headerOut) {
        return readIndexedMetadata(path, metadata, headerOut);
    }

} // namespace IndexedBook
