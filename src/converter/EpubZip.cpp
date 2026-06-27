#include "converter/EpubZip.h"

#include <SD_MMC.h>
#include <algorithm>
#include <array>
#include <esp32s3/rom/miniz.h>
#include <esp_heap_caps.h>

#include "converter/EpubContentWriter.h"
#include "converter/EpubPackage.h"

namespace EpubZip {
    namespace {

        using EpubPackage::isArchiveHintEntry;
        using EpubPackage::normalizeZipName;
        using EpubPackage::toLowerCopy;

        constexpr uint32_t kZipEocdSignature = 0x06054B50UL;
        constexpr uint32_t kZipCentralFileSignature = 0x02014B50UL;
        constexpr uint32_t kZipLocalFileSignature = 0x04034B50UL;
        constexpr uint16_t kZipStored = 0;
        constexpr uint16_t kZipDeflated = 8;
        constexpr size_t kZipEocdMaxSearch = 66UL * 1024UL;
        constexpr uint16_t kMaxZipEntries = 2048;
        constexpr uint16_t kMaxZipNameLength = 512;
        constexpr size_t kReadChunkBytes = 4096;
        constexpr size_t kInflateInputChunkBytes = 4096;


        uint16_t readLe16(const uint8_t* data) {
            return static_cast<uint16_t>(data[0]) | (static_cast<uint16_t>(data[1]) << 8);
        }

        uint32_t readLe32(const uint8_t* data) {
            return static_cast<uint32_t>(data[0]) | (static_cast<uint32_t>(data[1]) << 8)
                 | (static_cast<uint32_t>(data[2]) << 16) | (static_cast<uint32_t>(data[3]) << 24);
        }

        void serviceBackground() {
            yield();
            delay(0);
        }

        bool readExact(File& file, uint8_t* buffer, size_t length) {
            size_t offset = 0;
            while (offset < length) {
                const size_t chunk = std::min(kReadChunkBytes, length - offset);
                const uint32_t beforePosition = static_cast<uint32_t>(file.position());
                const int bytesRead = file.read(buffer + offset, chunk);
                if (bytesRead != static_cast<int>(chunk)) {
                    Serial.printf("[epub-zip] Short read at pos=%lu wanted=%u got=%d "
                                  "totalWanted=%u offset=%u\n",
                                  static_cast<unsigned long>(beforePosition), static_cast<unsigned int>(chunk),
                                  bytesRead, static_cast<unsigned int>(length), static_cast<unsigned int>(offset));
                    return false;
                }
                offset += chunk;
                serviceBackground();
            }

            return true;
        }

        void reportProgress(const EpubConverter::Options& options, const char* line1, const char* line2,
                            int progressPercent) {
            if (options.progressCallback == nullptr) {
                return;
            }

            progressPercent = std::max(0, std::min(100, progressPercent));
            options.progressCallback(options, line1, line2, progressPercent);
            serviceBackground();
        }

        void* allocateBuffer(size_t bytes) {
            if (bytes == 0) {
                return nullptr;
            }

            void* buffer = heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
            if (buffer == nullptr) {
                buffer = heap_caps_malloc(bytes, MALLOC_CAP_8BIT);
            }
            return buffer;
        }

        void* allocateInternalBuffer(size_t bytes) {
            if (bytes == 0) {
                return nullptr;
            }

            return heap_caps_malloc(bytes, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
        }

        void freeBuffer(void* buffer) {
            if (buffer != nullptr) {
                heap_caps_free(buffer);
            }
        }

        void reportContentProgress(const EpubConverter::Options& options, size_t itemIndex, size_t itemCount,
                                   uint32_t bytesRead, uint32_t totalBytes, size_t wordCount) {
            if (itemCount == 0 || totalBytes == 0) {
                return;
            }

            const uint32_t cappedBytes = std::min(bytesRead, totalBytes);
            const int contentPercent = static_cast<int>((cappedBytes * 100ULL) / totalBytes);
            const int itemPercent = static_cast<int>(((itemIndex * 100ULL) + contentPercent) / itemCount);
            const int progressPercent = 25 + ((itemPercent * 70) / 100);
            const String detail = String(itemIndex + 1) + "/" + String(itemCount) + " " + String(wordCount) + " words";
            reportProgress(options, "Extracting content", detail.c_str(), progressPercent);
        }

        bool seekToEntryPayload(File& file, const ZipEntry& entry, const char* context, uint32_t& dataOffset) {
            std::array<uint8_t, 30> localHeader;
            if (!file.seek(entry.localHeaderOffset)) {
                Serial.printf("[epub-zip] Could not seek to %s local header: %s offset=%lu\n", context,
                              entry.name.c_str(), static_cast<unsigned long>(entry.localHeaderOffset));
                return false;
            }
            if (!readExact(file, localHeader.data(), localHeader.size())) {
                Serial.printf("[epub-zip] Could not read %s local header: %s\n", context, entry.name.c_str());
                return false;
            }

            const uint32_t localSignature = readLe32(localHeader.data());
            if (localSignature != kZipLocalFileSignature) {
                Serial.printf("[epub-zip] Bad %s local signature for %s signature=0x%08lx\n", context,
                              entry.name.c_str(), static_cast<unsigned long>(localSignature));
                return false;
            }

            const uint16_t fileNameLength = readLe16(localHeader.data() + 26);
            const uint16_t extraLength = readLe16(localHeader.data() + 28);
            dataOffset = entry.localHeaderOffset + localHeader.size() + fileNameLength + extraLength;
            Serial.printf("[epub-zip] %s data: %s nameLen=%u extraLen=%u dataOffset=%lu\n", context, entry.name.c_str(),
                          fileNameLength, extraLength, static_cast<unsigned long>(dataOffset));
            if (!file.seek(dataOffset)) {
                Serial.printf("[epub-zip] Could not seek to %s data: %s offset=%lu\n", context, entry.name.c_str(),
                              static_cast<unsigned long>(dataOffset));
                return false;
            }

            return true;
        }

        template<typename ChunkSink>
        bool readStoredPayload(File& file, const ZipEntry& entry, uint32_t& totalOutputBytes, ChunkSink onChunk,
                               const char* context) {
            uint8_t* buffer = static_cast<uint8_t*>(allocateInternalBuffer(kReadChunkBytes));
            if (buffer == nullptr) {
                Serial.printf("[epub-zip] No internal buffer for stored %s: %s\n", context, entry.name.c_str());
                return false;
            }

            bool ok = true;
            uint32_t remaining = entry.uncompressedSize;
            while (remaining > 0) {
                const size_t chunk = std::min(kReadChunkBytes, static_cast<size_t>(remaining));
                if (!readExact(file, buffer, chunk)) {
                    Serial.printf("[epub-zip] Stored %s read failed: %s remaining=%lu\n", context, entry.name.c_str(),
                                  static_cast<unsigned long>(remaining));
                    ok = false;
                    break;
                }
                if (!onChunk(buffer, chunk)) {
                    ok = false;
                    break;
                }

                totalOutputBytes += static_cast<uint32_t>(chunk);
                remaining -= static_cast<uint32_t>(chunk);
                serviceBackground();
            }

            freeBuffer(buffer);
            return ok;
        }

        template<typename ChunkSink>
        bool inflatePayload(File& file, const ZipEntry& entry, uint32_t& totalOutputBytes, ChunkSink onChunk,
                            const char* context) {
            uint8_t* inputBuffer = static_cast<uint8_t*>(allocateInternalBuffer(kInflateInputChunkBytes));
            uint8_t* dictionary = static_cast<uint8_t*>(allocateInternalBuffer(TINFL_LZ_DICT_SIZE));
            tinfl_decompressor* inflator =
                static_cast<tinfl_decompressor*>(allocateInternalBuffer(sizeof(tinfl_decompressor)));
            if (inputBuffer == nullptr || dictionary == nullptr || inflator == nullptr) {
                Serial.printf("[epub-zip] No internal inflate buffers for %s: %s input=%s dict=%s inflator=%s\n",
                              context, entry.name.c_str(), inputBuffer == nullptr ? "no" : "yes",
                              dictionary == nullptr ? "no" : "yes", inflator == nullptr ? "no" : "yes");
                freeBuffer(inputBuffer);
                freeBuffer(dictionary);
                freeBuffer(inflator);
                return false;
            }

            tinfl_init(inflator);

            bool ok = true;
            uint32_t compressedRemaining = entry.compressedSize;
            size_t inputAvailable = 0;
            size_t inputOffset = 0;
            tinfl_status status = TINFL_STATUS_NEEDS_MORE_INPUT;

            while (status > TINFL_STATUS_DONE) {
                if (inputAvailable == 0 && compressedRemaining > 0) {
                    const size_t chunk = std::min(kInflateInputChunkBytes, static_cast<size_t>(compressedRemaining));
                    if (!readExact(file, inputBuffer, chunk)) {
                        Serial.printf("[epub-zip] Deflated %s read failed: %s remaining=%lu\n", context,
                                      entry.name.c_str(), static_cast<unsigned long>(compressedRemaining));
                        ok = false;
                        break;
                    }

                    compressedRemaining -= static_cast<uint32_t>(chunk);
                    inputAvailable = chunk;
                    inputOffset = 0;
                }

                const size_t dictionaryOffset = totalOutputBytes & (TINFL_LZ_DICT_SIZE - 1);
                uint8_t* writeCursor = dictionary + dictionaryOffset;
                size_t inSize = inputAvailable;
                size_t outSize = TINFL_LZ_DICT_SIZE - dictionaryOffset;
                const mz_uint32 flags = compressedRemaining > 0 ? TINFL_FLAG_HAS_MORE_INPUT : 0;

                status = tinfl_decompress(inflator, inputBuffer + inputOffset, &inSize, dictionary, writeCursor,
                                          &outSize, flags);
                inputAvailable -= inSize;
                inputOffset += inSize;

                if (outSize > 0) {
                    if (!onChunk(writeCursor, outSize)) {
                        ok = false;
                        break;
                    }
                    totalOutputBytes += static_cast<uint32_t>(outSize);
                }

                serviceBackground();

                if (status < TINFL_STATUS_DONE) {
                    Serial.printf("[epub-zip] Inflate failed for %s status=%d context=%s\n", entry.name.c_str(),
                                  static_cast<int>(status), context);
                    ok = false;
                    break;
                }

                if (inSize == 0 && outSize == 0 && status != TINFL_STATUS_DONE && inputAvailable == 0
                    && compressedRemaining == 0) {
                    Serial.printf("[epub-zip] Inflate stalled for %s status=%d context=%s\n", entry.name.c_str(),
                                  static_cast<int>(status), context);
                    ok = false;
                    break;
                }
            }

            freeBuffer(inputBuffer);
            freeBuffer(dictionary);
            freeBuffer(inflator);
            return ok;
        }

    } // namespace

    bool Archive::open(const String& path) {
        archivePath_ = path;
        file_ = SD_MMC.open(path);

        const auto failWithClosedArchive = [&]() {
            close();
            return false;
        };

        if (!file_ || file_.isDirectory()) {
            Serial.printf("[epub-zip] Open failed: %s\n", path.c_str());
            return failWithClosedArchive();
        }

        Serial.printf("[epub-zip] Opened archive: %s size=%lu\n", path.c_str(),
                      static_cast<unsigned long>(file_.size()));
        if (!readCentralDirectory()) {
            Serial.printf("[epub-zip] Central directory read failed: %s\n", path.c_str());
            return failWithClosedArchive();
        }
        Serial.printf("[epub-zip] Archive ready: %u file entries\n", static_cast<unsigned int>(entries_.size()));
        logArchiveHints("open");
        return true;
    }

    void Archive::close() {
        if (file_) {
            file_.close();
        }
        entries_.clear();
    }

    const ZipEntry* Archive::find(const String& name) const {
        const String normalized = normalizeZipName(name);
        const auto exact = std::find_if(entries_.begin(), entries_.end(), [&](const ZipEntry& entry) {
            return entry.name == normalized;
        });
        if (exact != entries_.end()) {
            return &(*exact);
        }

        const String lowered = toLowerCopy(normalized);
        const auto insensitive = std::find_if(entries_.begin(), entries_.end(), [&](const ZipEntry& entry) {
            return toLowerCopy(entry.name) == lowered;
        });
        if (insensitive != entries_.end()) {
            Serial.printf("[epub-zip] Case-insensitive ZIP match: requested=%s actual=%s\n", normalized.c_str(),
                          insensitive->name.c_str());
            return &(*insensitive);
        }

        Serial.printf("[epub-zip] Entry not found: %s\n", normalized.c_str());
        logArchiveHints("missing entry");
        return nullptr;
    }

    bool Archive::extractToString(const String& name, String& output, size_t maxBytes) {
        Serial.printf("[epub-zip] Request string entry: %s\n", name.c_str());
        Serial.flush();
        const ZipEntry* entry = find(name);
        if (entry == nullptr) {
            return false;
        }
        return extractToString(*entry, output, maxBytes);
    }

    ContentExtractStatus Archive::extractContentToRsvp(const String& name, File& output, size_t& wordCount,
                                                       size_t maxWords, String& lastChapterTitle,
                                                       const EpubConverter::Options& options, size_t itemIndex,
                                                       size_t itemCount) {
        const ZipEntry* entry = find(name);
        if (entry == nullptr) {
            Serial.printf("[epub-zip] Content entry not found: %s\n", name.c_str());
            return ContentExtractStatus::Failed;
        }
        return extractContentToRsvp(*entry, output, wordCount, maxWords, lastChapterTitle, options, itemIndex,
                                    itemCount);
    }

    void Archive::logArchiveHints(const char* reason) const {
        Serial.printf("[epub-zip] Archive hints (%s): entries=%u\n", reason == nullptr ? "" : reason,
                      static_cast<unsigned int>(entries_.size()));

        auto logEntry = [](const char* label, size_t displayIndex, const ZipEntry& entry) {
            Serial.printf("[epub-zip]   %s[%u] %s method=%u flags=0x%04x c=%lu "
                          "u=%lu local=%lu\n",
                          label, static_cast<unsigned int>(displayIndex), entry.name.c_str(), entry.method, entry.flags,
                          static_cast<unsigned long>(entry.compressedSize),
                          static_cast<unsigned long>(entry.uncompressedSize),
                          static_cast<unsigned long>(entry.localHeaderOffset));
        };

        size_t printed = 0;
        for (size_t i = 0; i < entries_.size() && printed < 10; ++i) {
            logEntry("entry", i, entries_[i]);
            ++printed;
        }

        size_t hinted = 0;
        for (size_t i = 0; i < entries_.size() && hinted < 20; ++i) {
            if (!isArchiveHintEntry(entries_[i].name)) {
                continue;
            }
            logEntry("hint", i, entries_[i]);
            ++hinted;
        }
    }

    bool Archive::readCentralDirectory() {
        const uint32_t fileSize = static_cast<uint32_t>(file_.size());
        if (fileSize < 22) {
            Serial.printf("[epub-zip] File too small for ZIP EOCD: %lu\n", static_cast<unsigned long>(fileSize));
            return false;
        }

        uint16_t entryCount = 0;
        uint32_t centralDirectoryOffset = 0;

        // Locate and validate the end-of-central-directory record before parsing entries.
        {
            const size_t tailSize = fileSize < kZipEocdMaxSearch ? static_cast<size_t>(fileSize) : kZipEocdMaxSearch;
            uint8_t* tail = static_cast<uint8_t*>(allocateBuffer(tailSize));
            if (tail == nullptr) {
                Serial.printf("[epub-zip] No memory for EOCD tail buffer: %u bytes\n",
                              static_cast<unsigned int>(tailSize));
                return false;
            }

            const uint32_t tailOffset = fileSize - static_cast<uint32_t>(tailSize);
            Serial.printf("[epub-zip] Searching EOCD: fileSize=%lu tailOffset=%lu tailSize=%u\n",
                          static_cast<unsigned long>(fileSize), static_cast<unsigned long>(tailOffset),
                          static_cast<unsigned int>(tailSize));
            const bool ok = file_.seek(tailOffset) && readExact(file_, tail, tailSize);
            const int eocdIndex = [&]() {
                if (!ok) {
                    return -1;
                }
                for (int i = static_cast<int>(tailSize) - 22; i >= 0; --i) {
                    if (readLe32(tail + i) == kZipEocdSignature) {
                        return i;
                    }
                }
                return -1;
            }();

            if (eocdIndex < 0) {
                Serial.printf("[epub-zip] EOCD signature not found (tailRead=%s)\n", ok ? "yes" : "no");
                freeBuffer(tail);
                return false;
            }

            const uint16_t diskNumber = readLe16(tail + eocdIndex + 4);
            const uint16_t directoryDisk = readLe16(tail + eocdIndex + 6);
            entryCount = readLe16(tail + eocdIndex + 10);
            centralDirectoryOffset = readLe32(tail + eocdIndex + 16);
            const uint32_t centralDirectorySize = readLe32(tail + eocdIndex + 12);
            freeBuffer(tail);

            Serial.printf("[epub-zip] EOCD found: eocdOffset=%lu entries=%u "
                          "cdOffset=%lu cdSize=%lu disk=%u "
                          "dirDisk=%u\n",
                          static_cast<unsigned long>(tailOffset + static_cast<uint32_t>(eocdIndex)), entryCount,
                          static_cast<unsigned long>(centralDirectoryOffset),
                          static_cast<unsigned long>(centralDirectorySize), diskNumber, directoryDisk);

            if (diskNumber != 0 || directoryDisk != 0 || entryCount == 0 || entryCount > kMaxZipEntries) {
                Serial.printf("[epub] Unsupported ZIP directory entry count: %u\n", entryCount);
                return false;
            }
        }

        entries_.clear();
        entries_.reserve(entryCount);
        if (!file_.seek(centralDirectoryOffset)) {
            Serial.printf("[epub-zip] Could not seek to central directory offset=%lu\n",
                          static_cast<unsigned long>(centralDirectoryOffset));
            return false;
        }

        for (uint16_t i = 0; i < entryCount; ++i) {
            if ((i & 0x1F) == 0) {
                serviceBackground();
            }

            ZipEntry entry;
            uint16_t extraLength = 0;
            uint16_t commentLength = 0;

            // Keep the fixed header and temporary filename buffer scoped to one entry.
            {
                std::array<uint8_t, 46> header;
                if (!readExact(file_, header.data(), header.size())
                    || readLe32(header.data()) != kZipCentralFileSignature) {
                    Serial.printf("[epub-zip] Bad central header at index=%u pos=%lu\n", i,
                                  static_cast<unsigned long>(file_.position()));
                    return false;
                }

                const uint16_t fileNameLength = readLe16(header.data() + 28);
                extraLength = readLe16(header.data() + 30);
                commentLength = readLe16(header.data() + 32);
                if (fileNameLength == 0 || fileNameLength > kMaxZipNameLength) {
                    Serial.printf("[epub] Unsupported ZIP filename length: %u\n", fileNameLength);
                    return false;
                }

                char* nameBuffer = static_cast<char*>(allocateBuffer(fileNameLength + 1));
                if (nameBuffer == nullptr) {
                    Serial.printf("[epub-zip] No memory for filename buffer: %u bytes\n", fileNameLength + 1);
                    return false;
                }

                const bool nameRead = readExact(file_, reinterpret_cast<uint8_t*>(nameBuffer), fileNameLength);
                nameBuffer[fileNameLength] = '\0';

                entry.name = normalizeZipName(String(nameBuffer));
                entry.method = readLe16(header.data() + 10);
                entry.flags = readLe16(header.data() + 8);
                entry.compressedSize = readLe32(header.data() + 20);
                entry.uncompressedSize = readLe32(header.data() + 24);
                entry.localHeaderOffset = readLe32(header.data() + 42);
                freeBuffer(nameBuffer);

                if (!nameRead) {
                    return false;
                }
            }

            const uint32_t nextPosition = static_cast<uint32_t>(file_.position()) + extraLength + commentLength;
            if (!file_.seek(nextPosition)) {
                Serial.printf("[epub-zip] Could not seek past central extras for %s next=%lu\n", entry.name.c_str(),
                              static_cast<unsigned long>(nextPosition));
                return false;
            }

            if (!entry.name.endsWith("/")) {
                entries_.push_back(entry);
            }
        }

        Serial.printf("[epub-zip] Central directory parsed: kept=%u rawEntries=%u\n",
                      static_cast<unsigned int>(entries_.size()), entryCount);
        return true;
    }

    bool Archive::extractToString(const ZipEntry& entry, String& output, size_t maxBytes) {
        output = "";

        Serial.printf("[epub-zip] Extract string: %s method=%u flags=0x%04x c=%lu "
                      "u=%lu max=%u\n",
                      entry.name.c_str(), entry.method, entry.flags, static_cast<unsigned long>(entry.compressedSize),
                      static_cast<unsigned long>(entry.uncompressedSize), static_cast<unsigned int>(maxBytes));
        Serial.flush();

        if (entry.uncompressedSize == 0 || entry.uncompressedSize > maxBytes || entry.compressedSize == 0
            || entry.compressedSize > maxBytes) {
            Serial.printf("[epub] Skipping %s (%lu compressed, %lu uncompressed bytes)\n", entry.name.c_str(),
                          static_cast<unsigned long>(entry.compressedSize),
                          static_cast<unsigned long>(entry.uncompressedSize));
            return false;
        }

        uint32_t dataOffset = 0;
        if (!seekToEntryPayload(file_, entry, "string", dataOffset)) {
            return false;
        }

        if (!output.reserve(static_cast<unsigned int>(entry.uncompressedSize + 1))) {
            Serial.printf("[epub-zip] No memory to reserve string for %s (%lu bytes)\n", entry.name.c_str(),
                          static_cast<unsigned long>(entry.uncompressedSize));
            return false;
        }

        uint32_t totalOutputBytes = 0;
        auto appendBytes = [&](const uint8_t* data, size_t length) -> bool {
            if (length == 0) {
                return true;
            }
            if (totalOutputBytes + length > maxBytes) {
                Serial.printf("[epub-zip] String extraction exceeded limit for %s\n", entry.name.c_str());
                return false;
            }
            if (!output.concat(reinterpret_cast<const char*>(data), static_cast<unsigned int>(length))) {
                Serial.printf("[epub-zip] String append failed for %s length=%u\n", entry.name.c_str(),
                              static_cast<unsigned int>(length));
                return false;
            }
            return true;
        };

        bool ok = [&]() {
            if (entry.method == kZipStored) {
                Serial.printf("[epub-zip] Reading stored string payload: %s\n", entry.name.c_str());
                return readStoredPayload(file_, entry, totalOutputBytes, appendBytes, "string");
            }
            if (entry.method == kZipDeflated) {
                Serial.printf("[epub-zip] Streaming inflate string payload: %s\n", entry.name.c_str());
                return inflatePayload(file_, entry, totalOutputBytes, appendBytes, "string");
            }
            Serial.printf("[epub] Unsupported ZIP method %u for %s\n", entry.method, entry.name.c_str());
            return false;
        }();

        if (ok && totalOutputBytes != entry.uncompressedSize) {
            Serial.printf("[epub-zip] String inflate size mismatch for %s (%lu of %lu bytes)\n", entry.name.c_str(),
                          static_cast<unsigned long>(totalOutputBytes),
                          static_cast<unsigned long>(entry.uncompressedSize));
            ok = false;
        }

        if (ok) {
            Serial.printf("[epub-zip] Extracted string OK: %s textLen=%u\n", entry.name.c_str(),
                          static_cast<unsigned int>(output.length()));
        }

        return ok;
    }

    ContentExtractStatus Archive::extractContentToRsvp(const ZipEntry& entry, File& output, size_t& wordCount,
                                                       size_t maxWords, String& lastChapterTitle,
                                                       const EpubConverter::Options& options, size_t itemIndex,
                                                       size_t itemCount) {
        Serial.printf("[epub-zip] Extract content: %s method=%u flags=0x%04x c=%lu u=%lu\n", entry.name.c_str(),
                      entry.method, entry.flags, static_cast<unsigned long>(entry.compressedSize),
                      static_cast<unsigned long>(entry.uncompressedSize));

        if (entry.uncompressedSize == 0 || entry.compressedSize == 0 || entry.uncompressedSize > options.maxContentBytes
            || entry.compressedSize > options.maxContentBytes) {
            Serial.printf("[epub] Skipping oversized content %s (%lu compressed, %lu "
                          "uncompressed bytes)\n",
                          entry.name.c_str(), static_cast<unsigned long>(entry.compressedSize),
                          static_cast<unsigned long>(entry.uncompressedSize));
            return ContentExtractStatus::Unsupported;
        }

        uint32_t dataOffset = 0;
        if (!seekToEntryPayload(file_, entry, "content", dataOffset)) {
            return ContentExtractStatus::Failed;
        }

        EpubContent::RsvpContentWriter writer(output, wordCount, maxWords, lastChapterTitle);
        uint32_t totalOutputBytes = 0;
        uint32_t lastProgressBytes = 0;
        ContentExtractStatus result = ContentExtractStatus::Complete;

        auto finishWriter = [&]() -> ContentExtractStatus {
            if (!writer.finish()) {
                return writer.reachedWordLimit() ? ContentExtractStatus::WordLimitReached
                                                 : ContentExtractStatus::Failed;
            }
            return ContentExtractStatus::Complete;
        };

        auto reportMaybe = [&](bool force) {
            if (!force && totalOutputBytes - lastProgressBytes < 32UL * 1024UL) {
                return;
            }
            lastProgressBytes = totalOutputBytes;
            reportContentProgress(options, itemIndex, itemCount, totalOutputBytes, entry.uncompressedSize, wordCount);
        };

        auto writeChunk = [&](const uint8_t* data, size_t length) -> bool {
            if (!writer.write(data, length)) {
                result =
                    writer.reachedWordLimit() ? ContentExtractStatus::WordLimitReached : ContentExtractStatus::Failed;
                return false;
            }
            reportMaybe(false);
            return true;
        };

        const auto extractPayload = [&]() {
            if (entry.method == kZipStored) {
                return readStoredPayload(file_, entry, totalOutputBytes, writeChunk, "content");
            }
            if (entry.method == kZipDeflated) {
                return inflatePayload(file_, entry, totalOutputBytes, writeChunk, "content");
            }
            Serial.printf("[epub] Unsupported ZIP method %u for %s\n", entry.method, entry.name.c_str());
            result = ContentExtractStatus::Unsupported;
            return false;
        };

        const bool ok = extractPayload();

        if (!ok) {
            return result == ContentExtractStatus::Complete ? ContentExtractStatus::Failed : result;
        }

        if (totalOutputBytes != entry.uncompressedSize) {
            Serial.printf("[epub] Inflate size mismatch for %s (%lu of %lu bytes)\n", entry.name.c_str(),
                          static_cast<unsigned long>(totalOutputBytes),
                          static_cast<unsigned long>(entry.uncompressedSize));
            return ContentExtractStatus::Failed;
        }

        reportMaybe(true);
        return finishWriter();
    }


} // namespace EpubZip
